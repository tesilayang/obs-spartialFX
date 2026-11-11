from pathlib import Path
p=Path('tools/test-installed-normal.ps1');s=p.read_text(encoding='utf-8')
s=s.replace("$fixture.name='Spatial FX Installed QA';",'''$fixture.name='Spatial FX Installed QA'
 foreach($source in $fixture.sources){if($source.id -eq 'image_source' -and $source.settings.file -and ![IO.Path]::IsPathRooted($source.settings.file)){$source.settings.file=[IO.Path]::GetFullPath((Join-Path (Join-Path $root 'runtime/obs32/bin/64bit') $source.settings.file))}}
 ''')
s=s.replace("if($LASTEXITCODE){throw 'Normal installed render probe failed'}","$probeExit=$LASTEXITCODE")
s=s.replace("Copy-Item $last.FullName (Join-Path $root 'logs/phase6-installed-normal-obs-runtime.log')","Copy-Item $last.FullName (Join-Path $root 'logs/phase6-installed-normal-obs-runtime.log'); if($probeExit){throw 'Normal installed render probe failed'}")
p.write_text(s,encoding='utf-8')
