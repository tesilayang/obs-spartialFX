"""Read MSI tables and extract verified cabinets without Windows Installer service."""
import _msi, pathlib, subprocess, shutil, concurrent.futures
ROOT=pathlib.Path(__file__).resolve().parents[1]
DOWNLOADS=ROOT/'downloads';OUTPUT=ROOT/'msvc';STAGING=ROOT/'tools/sdk-staging'
STAGING.mkdir(parents=True,exist_ok=True)
SEVEN=r'F:\Program Files (x86)\7-Zip\7z.exe'
def extract(cab):
    subprocess.run([SEVEN,'x','-y','-o'+str(STAGING),str(cab)],stdout=subprocess.DEVNULL,check=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    list(pool.map(extract,DOWNLOADS.glob('*.cab')))
count=0
for msi in DOWNLOADS.glob('*.msi'):
    database=_msi.OpenDatabase(str(msi),_msi.MSIDBOPEN_READONLY)
    def rows(table,n):
        view=database.OpenView('SELECT * FROM '+table);view.Execute(None)
        while row:=view.Fetch(): yield [row.GetString(i+1) for i in range(n)]
    directories={r[0]:(r[1],r[2].split(':')[0].split('|')[-1]) for r in rows('Directory',3)}
    def directory(key):
        if key=='TARGETDIR' or not key:return pathlib.Path()
        parent,name=directories[key]
        return directory(parent)/(name if name not in ('.','SourceDir') else '')
    components={r[0]:r[2] for r in rows('Component',6)}
    package_count=0
    for file in rows('File',8):
        identifier,component,name=file[:3]
        source=STAGING/identifier
        if not source.exists():raise FileNotFoundError(f'{msi.name}: {identifier}')
        target=OUTPUT/directory(components[component])/name.split('|')[-1]
        if not target.resolve().is_relative_to(OUTPUT.resolve()):raise RuntimeError('MSI path escapes output')
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source,target);count+=1;package_count+=1
    print(msi.name,package_count,'files',flush=True)
# Final setup and cleanup from the audited upstream script, scoped to OUTPUT only.
source=(ROOT/'tools/portable-msvc-minimal.py').read_text(encoding='utf-8')
context={'OUTPUT':OUTPUT,'Path':pathlib.Path,'shutil':shutil,'host':'x64','targets':['x64'],'first':lambda items,cond=lambda x:True:next((x for x in items if cond(x)),None),'total_download':sum(p.stat().st_size for p in DOWNLOADS.iterdir() if p.is_file())}
exec(source[source.index('### versions'):],context)
print('OFFLINE SDK EXTRACTION READY',count,'files')
