#include <graphics/graphics.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
#include "../src/fx_shader.hpp"
static std::string read(const std::string&p){std::ifstream f(p,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
int main(int argc,char**argv){
 if(argc<4)return 2;
 graphics_t*graphics=nullptr;
 if(gs_create(&graphics,argc>4?argv[4]:"libobs-d3d11.dll",0)!=GS_SUCCESS)return 3;
 gs_enter_context(graphics);
 const std::string root=argv[1],group=argv[2];
 auto common=read(root+"/fx_common.body"),body=read(root+"/fx"+group+".body");
 if(!spatial::specializeFxShader(common,body,std::stoi(argv[3])))return 4;
 auto footer=read(root+"/fx_footer.body");auto technique=footer.find("technique CaptureHistory");if(technique!=std::string::npos)footer.resize(technique);
 const auto code=common+body+footer;
 fprintf(stderr,"Compiling group %s child %s\n",argv[2],argv[3]);fflush(stderr);
 char*error=nullptr;auto*effect=gs_effect_create(code.c_str(),"isolated-spatial-fx",&error);
 fprintf(stderr,"Compiled=%d error=%s\n",effect!=nullptr,error?error:"");fflush(stderr);
 gs_effect_destroy(effect);gs_leave_context();gs_destroy(graphics);return effect?0:1;
}
