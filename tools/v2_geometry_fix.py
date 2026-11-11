from pathlib import Path
p=Path('src/core.hpp');s=p.read_text(encoding='utf-8-sig');s=s.replace('struct Projected', '''struct SourceTransform { double scaleX=1,scaleY=1,boundsWidth=0,boundsHeight=0; int boundsType=0,alignment=5,boundsAlignment=0; int left=0,right=0,top=0,bottom=0; bool cropToBounds=false; };
struct SourceLayout { double x=0,y=0,scaleX=1,scaleY=1,width=1,height=1; int left=0,right=0,top=0,bottom=0; };
SourceLayout sourceLayout(double width,double height,const SourceTransform &source);
struct Projected''');p.write_text(s,encoding='utf-8')
p=Path('src/core.cpp');s=p.read_text(encoding='utf-8-sig');s=s.replace('p.rotation=camera.rotation;p.rotation.z+=rotation;', '''// Compose R_camera * R_sourceZ, then extract the XYZ Euler convention used by rotate().
    const auto right=rotate(rotate({1,0,0},{0,0,rotation}),camera.rotation);
    const auto down=rotate(rotate({0,1,0},{0,0,rotation}),camera.rotation);
    const auto normal=rotate({0,0,1},camera.rotation);
    const double yaw=std::asin(std::clamp(-right.z,-1.0,1.0));
    if(std::abs(std::cos(yaw))>1e-7)p.rotation={std::atan2(down.z,normal.z)*180/pi,yaw*180/pi,std::atan2(right.y,right.x)*180/pi};
    else p.rotation={std::atan2(-normal.y,down.y)*180/pi,yaw*180/pi,0};''')
s+= '''
namespace spatial {
SourceLayout sourceLayout(double w,double h,const SourceTransform &t){
    SourceLayout r;r.left=t.left;r.right=t.right;r.top=t.top;r.bottom=t.bottom;
    r.width=std::max(1.0,w-t.left-t.right);r.height=std::max(1.0,h-t.top-t.bottom);
    r.scaleX=t.scaleX;r.scaleY=t.scaleY;double ox=0,oy=0;
    auto aligned=[](int flags,int a,int b,int right,int left){return (flags&right)?double(a):((flags&left)?0.0:double(a/2));};
    (void)aligned;
    auto ax=[](int flags,double size){return (flags&2)?size:((flags&1)?0.0:double(int(size)/2));};
    auto ay=[](int flags,double size){return (flags&8)?size:((flags&4)?0.0:double(int(size)/2));};
    if(t.boundsType&&t.boundsWidth>0&&t.boundsHeight>0){
        double width=r.width*std::abs(r.scaleX),height=r.height*std::abs(r.scaleY);int type=t.boundsType;
        if(type==6&&(width>t.boundsWidth||height>t.boundsHeight))type=2;
        if(type==1){r.scaleX=std::copysign(t.boundsWidth/r.width,r.scaleX);r.scaleY=std::copysign(t.boundsHeight/r.height,r.scaleY);}
        else if(type!=6){double factor=std::min(t.boundsWidth/std::max(width,.001),t.boundsHeight/std::max(height,.001));if(type==3)factor=std::max(t.boundsWidth/std::max(width,.001),t.boundsHeight/std::max(height,.001));if(type==4)factor=t.boundsWidth/std::max(width,.001);if(type==5)factor=t.boundsHeight/std::max(height,.001);r.scaleX*=factor;r.scaleY*=factor;}
        width=r.width*r.scaleX;height=r.height*r.scaleY;
        const double dx=t.boundsWidth-std::abs(width),dy=t.boundsHeight-std::abs(height);
        ox=ax(t.boundsAlignment,int(-dx));oy=ay(t.boundsAlignment,int(-dy));
        if(t.cropToBounds&&(type==3||type==4||type==5)&&(dx<-.1||dy<-.1)){
            const bool x=dx<-.1,flipped=(x?width:height)<0;const double diff=x?dx:dy,scale=x?r.scaleX:r.scaleY;
            const int mask=x?3:12,align=t.boundsAlignment&mask;const double over=std::abs(diff/scale);
            double tl=(align&(1|4))?0:((align&(2|8))?over:over*.5),br=over-tl;
            double &origin=x?ox:oy;if(flipped){if(!align)origin*=2;else if(align&(1|4))origin-=diff;std::swap(tl,br);}else origin=0;
            if(x){r.left+=int(std::round(tl));r.right+=int(std::round(br));}else{r.top+=int(std::round(tl));r.bottom+=int(std::round(br));}
        }
        ox+=width<0?width:0;oy+=height<0?height:0;
        ox+=ax(t.alignment,int(t.boundsWidth));oy+=ay(t.alignment,int(t.boundsHeight));
    }else{ox=ax(t.alignment,int(r.width*r.scaleX));oy=ay(t.alignment,int(r.height*r.scaleY));}
    r.width=std::max(1.0,w-r.left-r.right);r.height=std::max(1.0,h-r.top-r.bottom);
    r.x=r.width*r.scaleX*.5-ox;r.y=r.height*r.scaleY*.5-oy;return r;
}
}
''';s=s.replace('    auto aligned=[](int flags,int a,int b,int right,int left){return (flags&right)?double(a):((flags&left)?0.0:double(a/2));};\n    (void)aligned;\n','');p.write_text(s,encoding='utf-8')
p=Path('src/model.hpp');s=p.read_text(encoding='utf-8-sig').replace('    Plane plane{},defaultPlane{};','    Plane plane{},defaultPlane{};\n    SourceTransform importTransform{};double importWidth=0,importHeight=0;bool followSourceSize=false;');p.write_text(s,encoding='utf-8')
p=Path('src/model.cpp');s=p.read_text(encoding='utf-8-sig');needle='        l.hasDefault=boolean(item,'
addition='''        l.followSourceSize=boolean(item,"follow_source_size",false);l.importWidth=number(item,"import_width",0,0,100000);l.importHeight=number(item,"import_height",0,0,100000);
        auto&t=l.importTransform;t.scaleX=number(item,"source_sx",1,-100,100);t.scaleY=number(item,"source_sy",1,-100,100);t.alignment=int(number(item,"source_alignment",5,0,15));t.boundsAlignment=int(number(item,"bounds_alignment",0,0,15));t.cropToBounds=boolean(item,"crop_to_bounds",false);t.boundsWidth=l.boundsWidth;t.boundsHeight=l.boundsHeight;t.boundsType=l.boundsType;t.left=l.cropLeft;t.right=l.cropRight;t.top=l.cropTop;t.bottom=l.cropBottom;
''';s=s.replace(needle,addition+needle)
needle='        obs_data_set_bool(item,"has_default"'
addition='''        obs_data_set_bool(item,"follow_source_size",l.followSourceSize);obs_data_set_double(item,"import_width",l.importWidth);obs_data_set_double(item,"import_height",l.importHeight);obs_data_set_double(item,"source_sx",l.importTransform.scaleX);obs_data_set_double(item,"source_sy",l.importTransform.scaleY);obs_data_set_int(item,"source_alignment",l.importTransform.alignment);obs_data_set_int(item,"bounds_alignment",l.importTransform.boundsAlignment);obs_data_set_bool(item,"crop_to_bounds",l.importTransform.cropToBounds);
''';s=s.replace(needle,addition+needle);p.write_text(s,encoding='utf-8')
p=Path('src/dock_actions.cpp');s=p.read_text(encoding='utf-8-sig');start=s.index('        const float cw=');end=s.index('        const double depth=',start)
s=s[:start]+'''        l.boundsWidth=transform.bounds.x;l.boundsHeight=transform.bounds.y;l.boundsType=int(transform.bounds_type);
        l.followSourceSize=true;l.importWidth=w;l.importHeight=h;auto&t=l.importTransform;
        t.scaleX=transform.scale.x;t.scaleY=transform.scale.y;t.boundsWidth=l.boundsWidth;t.boundsHeight=l.boundsHeight;t.boundsType=l.boundsType;t.alignment=int(transform.alignment);t.boundsAlignment=int(transform.bounds_alignment);t.cropToBounds=transform.crop_to_bounds;
        t.left=crop.left;t.right=crop.right;t.top=crop.top;t.bottom=crop.bottom;
        const auto layout=sourceLayout(w,h,t);const auto offset=rotate({layout.x,layout.y,0},{0,0,transform.rot});
        const double cw=layout.width,ch=layout.height,scaleX=layout.scaleX,scaleY=layout.scaleY;
        const double screenX=transform.pos.x+offset.x,screenY=transform.pos.y+offset.y;
'''+s[end:];p.write_text(s,encoding='utf-8')
p=Path('src/stage.cpp');s=p.read_text(encoding='utf-8-sig');s=s.replace('size_t count=0;\n    for', 'size_t count=0;std::array<Layer,32> drawLayers{};\n    for');s=s.replace('        sourceWidths[i]=w;', '''        drawLayers[i]=l;
        if(l.followSourceSize&&l.importWidth>0&&l.importHeight>0){
            const auto current=sourceLayout(w,h,l.importTransform),ref=sourceLayout(l.importWidth,l.importHeight,l.importTransform);
            auto &d=drawLayers[i];auto&p=d.plane;
            if(std::abs(ref.scaleX)>.00001&&std::abs(ref.scaleY)>.00001){
                p.position=add(p.position,rotate({(current.x-ref.x)/ref.scaleX*p.scaleX,(current.y-ref.y)/ref.scaleY*p.scaleY,0},p.rotation));
                p.scaleX*=current.scaleX/ref.scaleX;p.scaleY*=current.scaleY/ref.scaleY;
            }
            d.cropLeft=current.left;d.cropRight=current.right;d.cropTop=current.top;d.cropBottom=current.bottom;
        }
        const auto &dl=drawLayers[i];
        sourceWidths[i]=w;''');s=s.replace('int(w)-l.cropLeft-l.cropRight','int(w)-dl.cropLeft-dl.cropRight').replace('int(h)-l.cropTop-l.cropBottom','int(h)-dl.cropTop-dl.cropBottom').replace('planes[i]=effectivePlane(l);\n        if(l.boundsType','planes[i]=effectivePlane(dl);\n        if(!l.followSourceSize&&l.boundsType');s=s.replace('if(std::abs(da-db)<.01)','if(da==db)');s=s.replace('const auto&l=m.layers[i];const auto&p=planes[i];','const auto&l=drawLayers[i];const auto&p=planes[i];');s=s.replace('cropParam(s,"caster_crop",m.layers[caster]', 'cropParam(s,"caster_crop",drawLayers[caster]');p.write_text(s,encoding='utf-8')
p=Path('src/dock_v2.cpp');s=p.read_text(encoding='utf-8-sig').replace('changed=sourceList->item(i)->text()!=entries[size_t(i)].name;', 'changed=sourceList->item(i)->text()!=entries[size_t(i)].name||sourceList->item(i)->data(Qt::UserRole).toLongLong()!=entries[size_t(i)].id;');p.write_text(s,encoding='utf-8')
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8-sig').replace('*static_cast<obs_source_t**>(p)=obs_source_get_ref(source);','{auto**out=static_cast<obs_source_t**>(p);if(!*out)*out=obs_source_get_ref(source);}');p.write_text(s,encoding='utf-8')
p=Path('tests/core_tests.cpp');s=p.read_text(encoding='utf-8-sig');needle='    std::cout<<checks-failed';s=s.replace(needle,'''    SourceTransform transform;auto pending=sourceLayout(1920,1080,transform),ready=sourceLayout(640,480,transform);
    require(close(pending.x-ready.x,640)&&close(pending.y-ready.y,300),"pending source dimension change preserves top-left anchor via center correction");
    transform.boundsType=2;transform.boundsWidth=1000;transform.boundsHeight=1000;transform.boundsAlignment=5;
    auto bounded=sourceLayout(640,360,transform);
    require(close(bounded.x,500)&&close(bounded.y,281.25),"inner bounds top alignment keeps actual material center");
    transform.boundsType=3;transform.cropToBounds=true;transform.boundsAlignment=0;
    auto cropped=sourceLayout(640,360,transform);
    require(cropped.left>0&&cropped.right>0&&close(cropped.height,360),"outer bounds crops alpha image to receiver rectangle");
'''+needle);p.write_text(s,encoding='utf-8')
