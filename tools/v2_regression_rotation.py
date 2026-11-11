from pathlib import Path
p=Path('tests/core_tests.cpp');s=p.read_text(encoding='utf-8-sig');needle='    std::cout<<checks-failed'
extra='''    Camera turned;turned.rotation={20,60,5};
    auto angled=importPlane(960,540,640,360,1,1,90,300,turned,1920,1080);
    auto aq=corners(angled,640,360);auto a0=project(aq[0],turned,1920,1080),a1=project(aq[1],turned,1920,1080),a2=project(aq[2],turned,1920,1080);
    require(close(a1.x-a0.x,0,1e-5)&&close(a1.y-a0.y,640,1e-5)&&close(a2.x-a0.x,-360,1e-5),"import composes source rotation in tilted camera local coordinates");
'''
s=s.replace(needle,extra+needle);p.write_text(s,encoding='utf-8')
