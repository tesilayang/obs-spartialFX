"""Small dependency-free OBS WebSocket v5 client for local integration tests."""
import base64, json, os, socket, struct, uuid
class OBS:
    def __init__(self, port=4457):
        self.socket=socket.create_connection(('127.0.0.1',port),timeout=15)
        key=base64.b64encode(os.urandom(16)).decode()
        self.socket.sendall((f'GET / HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n').encode())
        self.buffer=b''
        while b'\r\n\r\n' not in self.buffer: self.buffer+=self.socket.recv(4096)
        headers,self.buffer=self.buffer.split(b'\r\n\r\n',1)
        if b'101' not in headers.split(b'\r\n',1)[0]: raise RuntimeError(headers)
        hello=self.recv()
        if hello['op']!=0: raise RuntimeError(hello)
        if 'authentication' in hello.get('d',{}): raise RuntimeError('Test instance unexpectedly requires authentication')
        self.send({'op':1,'d':{'rpcVersion':1,'eventSubscriptions':0}})
        identified=self.recv()
        if identified['op']!=2: raise RuntimeError(identified)
    def read(self,n):
        while len(self.buffer)<n:
            block=self.socket.recv(max(4096,n-len(self.buffer)))
            if not block: raise ConnectionError('WebSocket closed')
            self.buffer+=block
        result,self.buffer=self.buffer[:n],self.buffer[n:]
        return result
    def frame(self,payload,opcode=1):
        size=len(payload); header=bytes([0x80|opcode])
        header+=bytes([0x80|size]) if size<126 else (bytes([0xfe])+struct.pack('!H',size) if size<65536 else bytes([0xff])+struct.pack('!Q',size))
        mask=os.urandom(4)
        self.socket.sendall(header+mask+bytes(c^mask[i%4] for i,c in enumerate(payload)))
    def send(self,obj): self.frame(json.dumps(obj).encode())
    def recv(self):
        chunks=[]
        while True:
            a,b=self.read(2);size=b&127
            if size==126:size=struct.unpack('!H',self.read(2))[0]
            elif size==127:size=struct.unpack('!Q',self.read(8))[0]
            mask=self.read(4) if b&128 else None
            payload=self.read(size)
            if mask: payload=bytes(c^mask[i%4] for i,c in enumerate(payload))
            opcode=a&15
            if opcode==8: raise ConnectionError('OBS closed WebSocket')
            if opcode==9:self.frame(payload,10);continue
            if opcode in (0,1):chunks.append(payload)
            if a&128 and chunks:return json.loads(b''.join(chunks))
    def call(self,kind,data=None):
        request_id=str(uuid.uuid4())
        self.send({'op':6,'d':{'requestType':kind,'requestId':request_id,'requestData':data or {}}})
        while True:
            response=self.recv()
            if response.get('op')==7 and response['d']['requestId']==request_id:
                d=response['d']
                if not d['requestStatus']['result']:raise RuntimeError(kind+': '+json.dumps(d,ensure_ascii=False))
                return d.get('responseData',{})
    def close(self):
        try:self.frame(b'',8)
        finally:self.socket.close()
