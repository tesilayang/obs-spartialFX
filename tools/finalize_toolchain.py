from pathlib import Path
source=Path('tools/portable-msvc-minimal.py').read_text(encoding='utf-8')
context={'OUTPUT':Path('msvc'),'host':'x64','targets':['x64'],'msvcv':'14.44.35207','sdkv':'10.0.22621.0','total_download':0}
exec(source[source.index('### setup.bat'):],context)
