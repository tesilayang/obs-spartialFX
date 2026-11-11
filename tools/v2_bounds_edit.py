from pathlib import Path
p=Path("src/model.hpp");s=p.read_text(encoding="utf-8-sig").replace("int depthOrder=0","double boundsWidth=0,boundsHeight=0;\n    int boundsType=0,depthOrder=0");p.write_text(s,encoding="utf-8")
p=Path("src/model.cpp");s=p.read_text(encoding="utf-8-sig").replace('l.hasDefault=boolean(item,"has_default",false);','l.boundsWidth=number(item,"bounds_width",0,0,100000);l.boundsHeight=number(item,"bounds_height",0,0,100000);l.boundsType=int(number(item,"bounds_type",0,0,6));\n        l.hasDefault=boolean(item,"has_default",false);')
s=s.replace('obs_data_set_bool(item,"has_default",l.hasDefault);','obs_data_set_double(item,"bounds_width",l.boundsWidth);obs_data_set_double(item,"bounds_height",l.boundsHeight);obs_data_set_int(item,"bounds_type",l.boundsType);\n        obs_data_set_bool(item,"has_default",l.hasDefault);');p.write_text(s,encoding="utf-8")
p=Path("src/dock_actions.cpp");s=p.read_text(encoding="utf-8-sig")
s=s.replace('const double scaleX=actualW?sx:double(transform.scale.x),scaleY=actualH?sy:double(transform.scale.y);\n        const double screenX=', 'double scaleX=actualW?sx:double(transform.scale.x),scaleY=actualH?sy:double(transform.scale.y);\n        double screenX=')
s=s.replace('const double depth=double(ctx.total-rank-1)*180;', '''if(transform.bounds_type!=OBS_BOUNDS_NONE&&transform.bounds.x>0&&transform.bounds.y>0){
            l.boundsWidth=transform.bounds.x;l.boundsHeight=transform.bounds.y;l.boundsType=int(transform.bounds_type);scaleX=transform.scale.x<0?-1:1;scaleY=transform.scale.y<0?-1:1;
            const double ox=(transform.alignment&OBS_ALIGN_LEFT)?l.boundsWidth*.5:((transform.alignment&OBS_ALIGN_RIGHT)?-l.boundsWidth*.5:0);
            const double oy=(transform.alignment&OBS_ALIGN_TOP)?l.boundsHeight*.5:((transform.alignment&OBS_ALIGN_BOTTOM)?-l.boundsHeight*.5:0);
            const auto offset=rotate({ox,oy,0},{0,0,transform.rot});screenX=transform.pos.x+offset.x;screenY=transform.pos.y+offset.y;
        }
        const double depth=double(ctx.total-rank-1)*180;''')
s=s.replace('demo.layers.push_back(l);','l.defaultPlane=l.plane;l.hasDefault=true;demo.layers.push_back(l);')
p.write_text(s,encoding="utf-8")
p=Path("src/dock.cpp");s=p.read_text(encoding="utf-8-sig").replace('}else model={};syncControls();','}else model={};for(auto&layer:model.layers)if(!layer.hasDefault){layer.defaultPlane=layer.plane;layer.hasDefault=true;}syncControls();');p.write_text(s,encoding="utf-8")
p=Path("src/stage.cpp");s=p.read_text(encoding="utf-8-sig");s=s.replace('planes[i]=effectivePlane(l);order[count++]=i;', '''planes[i]=effectivePlane(l);
        if(l.boundsType&&l.boundsWidth>0&&l.boundsHeight>0){
            double x=l.boundsWidth/widths[i],y=l.boundsHeight/heights[i];
            if(l.boundsType!=1){double factor=std::min(x,y);if(l.boundsType==3)factor=std::max(x,y);if(l.boundsType==4)factor=x;if(l.boundsType==5)factor=y;if(l.boundsType==6)factor=std::min(1.0,factor);x=y=factor;}
            planes[i].scaleX*=x;planes[i].scaleY*=y;
        }order[count++]=i;''')
p.write_text(s,encoding="utf-8")
