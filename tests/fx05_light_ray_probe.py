"""Compile the real FX05 shader ray helpers as scalar C++ and check their mapping.

Run with the bundled Python. Outputs stay under work/; OBS and build/ are untouched.
--shader can point at a saved pre-change fx05.body to reproduce the center failure.
This is a math regression probe; GPU compilation and rendered UI QA remain separate.
"""
import argparse
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--shader", type=pathlib.Path, default=ROOT / "data/effects/fx05.body")
parser.add_argument("--output", type=pathlib.Path, default=ROOT / "work/v3.4-blurfix-20261111/lighting-ray-test")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
shader = args.shader.read_text(encoding="utf-8").split("float fx05_attenuation", 1)[0]
# The baseline effects call fx05_ray and ignore independent center/size inputs.
call = "fx05_controlled_ray(uv,scale,center,size)" if "fx05_controlled_ray" in shader else "fx05_ray(uv,scale)"
prefix = r'''
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "fx_lighting.hpp"
struct float2 { float x,y; float2(float a,float b):x(a),y(b){} };
struct float3 { float x,y,z; float3():x(0),y(0),z(0){} float3(float a,float b,float c):x(a),y(b),z(c){} };
float3 operator+(float3 a,float3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
float3 operator-(float3 a,float3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float3 operator*(float3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float3 operator*(float a,float3 b){return b*a;}
float3 operator/(float3 a,float b){return {a.x/b,a.y/b,a.z/b};}
float dot(float3 a,float3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float length(float3 a){return std::sqrt(dot(a,a));}
float max(float a,float b){return std::max(a,b);}
float3 layer_position{10,20,30},layer_right{640,0,0},layer_down{0,360,0},layer_normal{0,0,-1},lamp_position{300,90,-386};
float2 layer_uv_center{.5f,.5f};
void check(bool ok,const char*label){if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}std::cout<<"PASS "<<label<<'\n';}
bool near(float3 a,float3 b){return length(a-b)<.002f;}
'''
tests = r'''
int main(){
 const float2 uv{.23f,.79f};
 auto original=underTest(uv,1,{0,0},1);
 check(near(original,fx05_ray(uv,1)),"zero offsets preserve configured lamp rays");
 auto right=underTest(uv,1,{.5f,0},1);
 auto down=underTest(uv,1,{0,.5f},1);
 check(near(right-original,{320,0,0})&&near(down-original,{0,180,0}),"positive X and Y offsets independently move light right and down");
 auto wide=underTest(uv,1,{.5f,0},2);
 check(near(wide,{right.x*.5f,right.y*.5f,-416}),"doubling size halves radial distance while preserving lamp depth");
 auto scaled=underTest(uv,2,{.5f,0},1);
 check(near(scaled-fx05_ray(uv,2),{640,0,0}),"offsets respect the existing physical surface scale");
 lamp_position={-3000,4500,-386};
 check(near(underTest(uv,1,{0,0},1),fx05_ray(uv,1))&&!near(underTest(uv,1,{0,0},1),original),"configured lamp movement still changes material lighting");
 auto minimum=underTest(uv,1,{.5f,0},0);
 check(near(minimum,underTest(uv,1,{.5f,0},.01f)),"zero size is safely limited to the positive slider minimum");
 layer_right={0,0,640};layer_down={0,360,0};layer_normal={-1,0,0};layer_uv_center={.35f,.6f};lamp_position={-406,4000,800};
 check(near(underTest(uv,2,{0,0},1),fx05_ray(uv,2)),"rotated and cropped layers retain their original projected lamp center");
 check(near(underTest(uv,2,{.25f,-.5f},1)-fx05_ray(uv,2),{0,-360,320}),"signed UV offsets follow the rotated layer basis");
 layer_right={640,0,0};layer_down={0,360,0};layer_normal={0,0,-1};layer_uv_center={.5f,.5f};
 const auto local=spatial::resolveFxLighting(false,{}, {10,20,30},{640,0,0},{0,360,0},{0,0,-1});
 lamp_position={float(local.position.x),float(local.position.y),float(local.position.z)};
 for(float scale:{.25f,1.f,2.f})for(float2 point:{float2{0,0},float2{.5f,.5f},float2{1,1}})
  check(near(underTest(point,scale,{0,0},1),fx05_ray(point,scale)),"new defaults preserve local-rig rays at every surface scale");
}
'''
(out / "ray-test.cpp").write_text(prefix + shader + "\nfloat3 underTest(float2 uv,float scale,float2 center,float size){return " + call + ";}\n" + tests, encoding="utf-8")
batch = '@echo off\ncall "' + str(ROOT / "msvc/setup_x64.bat") + '"\n'
batch += 'cd /d "' + str(out) + '"\n'
batch += 'cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /utf-8 /DNOMINMAX /I"' + str(ROOT / "src") + '" ray-test.cpp /Fe:ray-test.exe\n'
batch += 'if errorlevel 1 exit /b 2\nray-test.exe\n'
(out / "run.cmd").write_text(batch, encoding="utf-8")
result = subprocess.run(["cmd", "/c", str(out / "run.cmd")], cwd=out, capture_output=True, text=True)
print(result.stdout, end="")
print(result.stderr, end="", file=sys.stderr)
(out / "result.log").write_text(result.stdout + result.stderr, encoding="utf-8")
sys.exit(result.returncode)
