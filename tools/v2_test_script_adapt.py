from pathlib import Path
for name in ['test-installer','test-installed-normal','test-installed-portable']:
 s=Path('tools/'+name+'.ps1').read_text(encoding='utf-8-sig').replace('OBS_Spatial_FX_V1.0_Setup','OBS_Spatial_FX_V2.0_Setup')
 s=s.replace('logs/','logs/v2/').replace("SPATIAL_QA_SCENARIO='hold'","SPATIAL_QA_SCENARIO='v2hold'")
 if name=='test-installed-normal':s=s.replace('tests/runtime_probe.py','tests/v2_runtime_probe.py')
 Path('tools/'+name+'-v2.ps1').write_text(s,encoding='utf-8-sig')
p=Path('tests/v2_runtime_probe.py');s=Path('tests/runtime_probe.py').read_text(encoding='utf-8-sig').replace("check(settings.get('camera_x')==177,'native dock camera control reached OBS settings')","check(settings.get('schema_version')==2,'native installed stage settings use schema2')");p.write_text(s,encoding='utf-8')
