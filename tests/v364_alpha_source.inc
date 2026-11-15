// QA-only sources exercise OBS's two source blending contracts on the real GPU.
struct AlphaFixture {gs_texture_t *texture=nullptr;bool premultiplied=false;};
const char*alphaFixtureName(void*){return "Spatial QA alpha fixture";}
void*alphaFixtureCreate(obs_data_t*settings,obs_source_t*){
 auto*s=new AlphaFixture;s->premultiplied=obs_data_get_bool(settings,"premultiplied");
 std::vector<uint8_t> pixels(256*256*4);const uint8_t alpha[]={0,16,64,128,255};
 for(size_t y=0;y<256;++y)for(size_t x=0;x<256;++x){const auto a=alpha[std::min(size_t(4),x/52)];auto*p=&pixels[(y*256+x)*4];p[0]=204;p[1]=102;p[2]=51;p[3]=a;if(s->premultiplied)for(int c=0;c<3;++c)p[c]=uint8_t((int(p[c])*a+127)/255);}
 const uint8_t*data=pixels.data();obs_enter_graphics();s->texture=gs_texture_create(256,256,GS_RGBA,1,&data,0);obs_leave_graphics();return s;
}
void alphaFixtureDestroy(void*data){auto*s=static_cast<AlphaFixture*>(data);obs_enter_graphics();gs_texture_destroy(s->texture);obs_leave_graphics();delete s;}
void alphaFixtureRender(void*data,gs_effect_t*){auto*s=static_cast<AlphaFixture*>(data);auto*effect=obs_get_base_effect(OBS_EFFECT_DEFAULT);gs_effect_set_texture(gs_effect_get_param_by_name(effect,"image"),s->texture);gs_blend_state_push();if(s->premultiplied)gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);while(gs_effect_loop(effect,"Draw"))gs_draw_sprite(s->texture,0,256,256);gs_blend_state_pop();}
void registerAlphaFixture(){obs_source_info info{};info.id="spatial_qa_alpha_fixture";info.type=OBS_SOURCE_TYPE_INPUT;info.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;info.get_name=alphaFixtureName;info.create=alphaFixtureCreate;info.destroy=alphaFixtureDestroy;info.video_render=alphaFixtureRender;info.get_width=[](void*)->uint32_t{return 256;};info.get_height=[](void*)->uint32_t{return 256;};obs_register_source(&info);}
