from pathlib import Path
p=Path("src/dock_actions.cpp");s=p.read_text(encoding="utf-8-sig")
s=s.replace('if(force||uuid!=sceneUuid||ids!=lastStageIds){','if(force||uuid!=sceneUuid||ids!=lastStageIds){')
s=s.replace('loading=false;loadStage();\n    }\n}', 'loading=false;loadStage();\n    }\n    refreshSources();\n}',1)
start=s.index('    struct Context {StageModel *model;bool selected;')
end=s.index('\n}\nvoid SpatialDock::restoreLayer',start)
s=s[:start]+'''    size_t total=0;obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*item,void*p){auto*s=obs_sceneitem_get_source(item);if(std::string(obs_source_get_unversioned_id(s))!=stageId)++*static_cast<size_t*>(p);return true;},&total);
    struct Context {StageModel *model;bool selected;size_t added=0,skipped=0,index=0,total=0;std::string sceneUuid;};
    Context ctx{&model,selected,0,0,0,total,obs_source_get_uuid(sceneSource)};
    obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*item,void*opaque){
        auto&ctx=*static_cast<Context*>(opaque);auto*source=obs_sceneitem_get_source(item);const std::string id=obs_source_get_unversioned_id(source);if(id==stageId)return true;
        const size_t rank=ctx.index++;if(ctx.selected&&!obs_sceneitem_selected(item))return true;
        if(id=="scene"||id=="group"||obs_source_get_type(source)!=OBS_SOURCE_TYPE_INPUT||!(obs_source_get_output_flags(source)&OBS_SOURCE_VIDEO)){++ctx.skipped;return true;}
        const std::string uuid=obs_source_get_uuid(source);const auto itemId=obs_sceneitem_get_id(item);
        for(const auto&l:ctx.model->layers)if(l.sourceUuid==uuid&&l.itemId==itemId&&l.sceneUuid==ctx.sceneUuid)return true;
        if(ctx.model->layers.size()>=32){++ctx.skipped;return true;}
        obs_transform_info transform{};obs_sceneitem_get_info2(item,&transform);obs_sceneitem_crop crop{};obs_sceneitem_get_crop(item,&crop);
        const uint32_t actualW=obs_source_get_width(source),actualH=obs_source_get_height(source);
        // A hidden or not-yet-ready WGC source can report 0x0. Keep the association
        // and activate it through the stage; capture starts when a window becomes available.
        const uint32_t w=actualW?actualW:ctx.model->width,h=actualH?actualH:ctx.model->height;
        Layer l;l.sourceUuid=uuid;l.sceneUuid=ctx.sceneUuid;l.itemId=itemId;l.label=obs_source_get_name(source);l.originalVisible=obs_sceneitem_visible(item);
        l.cropLeft=crop.left;l.cropRight=crop.right;l.cropTop=crop.top;l.cropBottom=crop.bottom;
        const float cw=float(std::max(1,int(w)-crop.left-crop.right)),ch=float(std::max(1,int(h)-crop.top-crop.bottom));
        matrix4 matrix;obs_sceneitem_get_draw_transform(item,&matrix);
        const double sx=std::hypot(matrix.x.x,matrix.x.y)*(transform.scale.x<0?-1:1);
        const double sy=std::hypot(matrix.y.x,matrix.y.y)*(transform.scale.y<0?-1:1);
        vec3 center,position;vec3_set(&center,cw*.5f,ch*.5f,0);vec3_transform(&position,&center,&matrix);
        // For pending sources OBS has no draw matrix yet: use declared bounds/position.
        const double scaleX=actualW?sx:double(transform.scale.x),scaleY=actualH?sy:double(transform.scale.y);
        const double screenX=actualW?position.x:transform.pos.x+cw*scaleX*.5,screenY=actualH?position.y:transform.pos.y+ch*scaleY*.5;
        const double depth=double(ctx.total-rank-1)*180;
        l.plane=importPlane(screenX,screenY,cw,ch,scaleX,scaleY,transform.rot,depth,ctx.model->camera,ctx.model->width,ctx.model->height);
        l.defaultPlane=l.plane;l.hasDefault=true;l.depthOrder=int(rank);
        ctx.model->layers.push_back(l);obs_sceneitem_set_visible(item,false);++ctx.added;return true;
    },&ctx);
    obs_source_release(sceneSource);syncLayers();syncV2();commit();refreshSources();message(QString::fromUtf8("已导入 %1 项；跳过 %2 项。支持窗口捕捉、裁剪、边界与镜像；未就绪源将自动等待。").arg(ctx.added).arg(ctx.skipped));'''+s[end:]
s=s.replace('model.light=incoming.light;model.fx=incoming.fx;model.loop=incoming.loop;', '''model.light=incoming.light;model.extraLights=incoming.extraLights;model.fx=incoming.fx;model.loop=incoming.loop;model.target=incoming.target;model.lookAtTarget=incoming.lookAtTarget;model.animationSpeed=incoming.animationSpeed;model.animationDuration=incoming.animationDuration;model.focusDistance=incoming.focusDistance;model.aperture=incoming.aperture;''')
old='model.layers[size_t(i)].plane=sanitized.layers[0].plane;model.layers[size_t(i)].opacity=sanitized.layers[0].opacity;'
new='''auto &current=model.layers[size_t(i)];const auto identity=current;current=sanitized.layers[0];current.sourceUuid=identity.sourceUuid;current.sceneUuid=identity.sceneUuid;current.itemId=identity.itemId;current.label=identity.label;current.originalVisible=identity.originalVisible;current.visible=identity.visible;'''
s=s.replace(old,new)
s=s.replace('for(const auto&l:model.layers)restoreLayer(l);model.layers.clear();syncLayers();commit();','for(const auto&l:model.layers)restoreLayer(l);model.layers.clear();syncLayers();syncV2();commit();')
s=s.replace('model.layers.erase(model.layers.begin()+row);syncLayers();commit();','model.layers.erase(model.layers.begin()+row);syncLayers();syncV2();commit();')
p.write_text(s,encoding="utf-8")
