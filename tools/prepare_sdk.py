import pathlib,struct,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
OBS=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else r'F:\Program Files (x86)\obs-studio new')
OUT=ROOT/'tools/sdk'; OUT.mkdir(exist_ok=True)
def exports(path):
    data=path.read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]
    nsec=struct.unpack_from('<H',data,pe+6)[0]; optsize=struct.unpack_from('<H',data,pe+20)[0]; opt=pe+24
    assert struct.unpack_from('<H',data,opt)[0]==0x20b,'64-bit PE required'
    table=opt+optsize
    sections=[struct.unpack_from('<IIII',data,table+i*40+8) for i in range(nsec)]
    def offset(rva):
        for vsize,va,rsize,raw in sections:
            if va<=rva<va+max(vsize,rsize):return raw+rva-va
        raise ValueError(f'unmapped RVA {rva:x}')
    rva,size=struct.unpack_from('<II',data,opt+112)
    directory=offset(rva)
    count=struct.unpack_from('<I',data,directory+24)[0]; names=offset(struct.unpack_from('<I',data,directory+32)[0])
    result=[]
    for i in range(count):
        pos=offset(struct.unpack_from('<I',data,names+i*4)[0]); end=data.index(0,pos)
        result.append(data[pos:end].decode('ascii'))
    return result
for name in ['obs','obs-frontend-api']:
    names=exports(OBS/'bin/64bit'/f'{name}.dll')
    dest=OUT/f'{name}.def';dest.write_text(f'LIBRARY "{name}.dll"\nEXPORTS\n'+'\n'.join(names)+'\n')
    subprocess.run(['lib.exe','/nologo','/machine:x64','/def:'+str(dest),'/out:'+str(OUT/f'{name}.lib')],check=True)
    print(name,len(names),'exports; generated import library from installed OBS DLL')
