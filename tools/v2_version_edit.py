from pathlib import Path
p=Path("CMakeLists.txt");s=p.read_text(encoding="utf-8-sig").replace("VERSION 1.0.0","VERSION 2.0.0").replace("OBS_Spatial_FX_V1.0_Setup","OBS_Spatial_FX_V2.0_Setup");p.write_text(s,encoding="utf-8")
for name in ("src/plugin.cpp","installer/setup.cpp","installer/setup.rc.in"):
 p=Path(name);s=p.read_text(encoding="utf-8-sig").replace("1.0.0","2.0.0").replace("1.0","2.0")
 if name.endswith("rc.in"):s=s.replace("1,0,0,0","2,0,0,0")
 p.write_text(s,encoding="utf-8")
