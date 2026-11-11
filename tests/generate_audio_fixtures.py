"""Generate explicit QA audio in work/v3.2/audio/fixtures; FFmpeg optional, never shipped."""
from pathlib import Path
import argparse,math,struct,wave,shutil,subprocess
parser=argparse.ArgumentParser();parser.add_argument('--ffmpeg',default=shutil.which('ffmpeg'));args=parser.parse_args()
root=Path(__file__).resolve().parents[1]/'work/v3.2/audio/fixtures';root.mkdir(parents=True,exist_ok=True)
with wave.open(str(root/'点击_120_反相.wav'),'wb') as f:
 f.setparams((2,2,48000,0,'NONE','not compressed'))
 for i in range(48000*13):
  t=i/48000;age=t-.3-math.floor((t-.3)/.5)*.5
  value=int(28000*math.exp(-age*48000/180)*math.sin(2*math.pi*800*age)) if t>=.3 and age<.02 and t<12 else 0
  f.writeframesraw(struct.pack('<hh',value,-value))
(root/'invalid.mp3').write_bytes(b'not an audio file')
with wave.open(str(root/'silence-30min.wav'),'wb') as f:
 f.setparams((1,2,8000,0,'NONE','not compressed'))
 block=b'\0'*16000
 for _ in range(1800):f.writeframesraw(block)
if args.ffmpeg:
 for name,codec in [('click120.mp3','libmp3lame'),('click120-aac.mp4','aac'),('click120.flac','flac'),('click120.mkv','aac')]:
  subprocess.run([args.ffmpeg,'-hide_banner','-loglevel','error','-nostdin','-y','-i',str(root/'点击_120_反相.wav'),'-c:a',codec,str(root/name)],check=True)
 subprocess.run([args.ffmpeg,'-hide_banner','-loglevel','error','-nostdin','-y','-f','lavfi','-i','color=c=black:s=32x32:d=1','-an','-c:v','libx264',str(root/'no-audio.mp4')],check=True)
else:print('FFmpeg absent: WAV/silence/invalid fixtures only; codec probes must be NOT RUN.')
print(root)
