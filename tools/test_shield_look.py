"""Compile the actual shield eligibility and draw scope against a small world."""
import os
import io
from pathlib import Path
import re
import shutil
import subprocess
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[1]


def test_shield_modules_in_shared_platform_graphs(tmp_path, monkeypatch):
    from tools import android_build, ninja_syntax, web_build

    monkeypatch.chdir(ROOT)
    ndk = tmp_path / "ndk"
    (ndk / "toolchains/llvm/prebuilt/windows-x86_64").mkdir(parents=True)
    monkeypatch.setenv("ANDROID_NDK_HOST_TAG", "windows-x86_64")
    monkeypatch.setattr(android_build, "fetch_third_party", lambda: None)
    sln = SimpleNamespace(build_dir=Path("build"), web_emcc="/standin/emcc",
                          android_ndk=str(ndk), port_release=False,
                          port_profile=False, compiler_launcher=None)
    for generate, rule in [(web_build.generate_web_build, "web_cc"),
                           (android_build.generate_android_build, "android_guest_cc")]:
        out = io.StringIO()
        generate(ninja_syntax.Writer(out), sln)
        text = re.sub(r" \$\n +", " ", out.getvalue()).replace("\\", "/")
        for source in ["port/linux/game/shield_look.c", "port/linux/src/shield_look_gl.c"]:
            assert re.search(r"^build [^\n]*: " + rule + " " + re.escape(source) + r"[ \n]",
                             text, re.M), source


@pytest.mark.parametrize("gles", [False, True])
def test_generated_shader_on_gpu(tmp_path, gles):
    compiler = shutil.which("clang")
    if not compiler:
        pytest.skip("clang required")
    flags = [compiler, "-std=gnu11", "-O1", "-fuse-ld=lld",
             "-I" + str(ROOT / "port/linux/src"), "-iquote" + str(ROOT / "port/linux/include")]
    env = dict(os.environ)
    if os.name == "nt":
        sdls = sorted((ROOT / "build/windows/third_party").glob("SDL3-*/include"))
        if not sdls:
            pytest.skip("built SDL3 required for hidden GPU test")
        sdl = sdls[-1].parent
        flags += ["--target=i686-pc-windows-msvc", "-I" + str(sdl / "include")]
        libraries = [str(sdl / "lib/x86/SDL3.lib")]
        env["PATH"] = str(sdl / "lib/x86") + os.pathsep + env["PATH"]
    else:
        if not shutil.which("pkg-config"):
            pytest.skip("SDL3 development files required")
        result = subprocess.run(["pkg-config", "--cflags", "--libs", "sdl3"], capture_output=True, text=True)
        if result.returncode:
            pytest.skip("SDL3 development files required")
        libraries = result.stdout.split() + ["-lm"]
        # The test never opens a visible window. On Linux use the caller's
        # virtual display, as the repository's automated rendering tests do.
        if not env.get("DISPLAY"):
            pytest.skip("a virtual display is required")

    from harness import function
    draw = function((ROOT / "source/rasterizer/xbox/rasterizer_xbox_plasma_energy.c").read_text(),
                    "rasterizer_plasma_energy_draw")
    setup = draw[draw.index("pixel_shader.texture_modes ="):draw.index("rasterizer_set_pixel_shader(&pixel_shader);")]
    for name, state in {"texture_modes": "key.texture_modes",
                        "combiner_count": "key.combiner_state[D3DRS_PSCOMBINERCOUNT]",
                        "final_combiner_inputs_abcd": "key.combiner_state[D3DRS_PSFINALCOMBINERINPUTSABCD]",
                        "final_combiner_inputs_efg": "key.combiner_state[D3DRS_PSFINALCOMBINERINPUTSEFG]"}.items():
        setup = setup.replace("pixel_shader." + name, state)
    for name, state in {"alpha_inputs": "PSALPHAINPUTS0", "rgb_inputs": "PSRGBINPUTS0",
                        "alpha_outputs": "PSALPHAOUTPUTS0", "rgb_outputs": "PSRGBOUTPUTS0"}.items():
        setup = re.sub(r"pixel_shader\." + name + r"\[(\d+)\]",
                       r"key.combiner_state[D3DRS_" + state + r"+\1]", setup)
    enums = (ROOT / "port/include/xdk/xdk_pdb.h").read_text()
    declarations = ""
    for name in ["_D3DRENDERSTATETYPE", "_D3DCMPFUNC", "_D3DFOGMODE"]:
        start = enums.index("enum " + name + " {")
        end = enums.index("};", start) + 2
        declarations += enums[start:end] + "\n"
    code = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
typedef unsigned long DWORD,D3DCOLOR;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define __HALO_LINUX_PLATFORM_H
''' + declarations + r'''
#include "xgpu.h"
#define GL_DEFINE(name) __typeof__(name) name;
GL_FUNCTIONS(GL_DEFINE)
#undef GL_DEFINE
int config_boolean(const char *name){return 0;}
const char *config_string(const char *name){return "";}
void mcc_environment_shader_emit(struct xgpu_text *text){}
'''
    if gles:
        # Generate the production ES path using desktop GL entry points;
        # the driver compiles/links its actual GLSL ES 3.00 sources.
        code += '#define HALO_GLES 1\nstruct {const char *shading_language;} xgpu_capabilities={"300 es"};\n'
        header = (ROOT / "port/linux/src/xgpu.h").read_text()
        code += "#undef XGPU_PIXEL_UNIFORMS_ES\n"
        code += re.search(r"^#define XGPU_PIXEL_UNIFORMS_ES .+", header, re.M).group(0) + "\n"
    for source in ["xgpu_text.c", "plasma_model_shader.c", "shield_look_gl.c", "nv2a_psh.c"]:
        code += (ROOT / "port/linux/src" / source).read_text() + "\n"
    version = "#version 300 es\nprecision highp float;\n" if gles else "#version 450 core\n"
    code += r'''
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static GLuint compile(GLenum type,const char *source){
 GLuint shader=glCreateShader(type);GLint ok;char log[8192];
 glShaderSource(shader,1,&source,NULL);glCompileShader(shader);glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
 if(!ok){glGetShaderInfoLog(shader,sizeof(log),NULL,log);fprintf(stderr,"%s\n%s\n",log,source);exit(1);}return shader;
}
static unsigned long render(GLuint program,float time,float hit,float recharge,float depleted,float brightness){
 unsigned char pixels[16*16*4];unsigned long total=0;int i;
 float state[4]={time,hit,recharge,depleted},color[4]={brightness,brightness,brightness,1},rim[4]={.2f,1,.8f,0};
 glUseProgram(program);glUniform4fv(glGetUniformLocation(program,"shield_look_state"),1,state);
 glUniform4fv(glGetUniformLocation(program,"shield_look_color"),1,color);
 glUniform4fv(glGetUniformLocation(program,"shield_look_rim"),1,rim);
 glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
 glReadPixels(0,0,16,16,GL_RGBA,GL_UNSIGNED_BYTE,pixels);CHECK(glGetError()==GL_NO_ERROR);
 for(i=0;i<16*16;i++)total+=pixels[i*4];return total;
}
int main(void){
 SDL_Window *window;SDL_GLContext context;struct nv2a_pixel_shader_key key={0};
 GLuint vertex,fragment,program,vao;GLint ok;char log[8192];char *source;
 unsigned long healthy,hit,dark,depleted,recharge;
 if(!SDL_Init(SDL_INIT_VIDEO))return 77;
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,5);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 window=SDL_CreateWindow("Shield shader regression",16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
 if(!window)return 77;context=SDL_GL_CreateContext(window);if(!context)return 77;
#define LOAD(name) name=(__typeof__(name))SDL_GL_GetProcAddress(#name);CHECK(name);
 GL_FUNCTIONS(LOAD)
#undef LOAD
 SETUP
 key.sampler_type[0]=key.sampler_type[1]=_xgpu_sampler_3d;
 source=nv2a_pixel_shader_to_glsl(&key);CHECK(!strstr(source,"shield_look_state"));free(source);
 key.enhanced_shield=1;source=nv2a_pixel_shader_to_glsl(&key);CHECK(strstr(source,"shield_look_state"));
 fragment=compile(GL_FRAGMENT_SHADER,source);free(source);
 vertex=compile(GL_VERTEX_SHADER,VERTEX_SOURCE);
 program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);glLinkProgram(program);
 glGetProgramiv(program,GL_LINK_STATUS,&ok);
 if(!ok){glGetProgramInfoLog(program,sizeof(log),NULL,log);fprintf(stderr,"%s\n",log);return 1;}
 glGenVertexArrays(1,&vao);glBindVertexArray(vao);glViewport(0,0,16,16);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDisable(GL_DITHER);
 healthy=render(program,3,0,0,0,1);hit=render(program,3,.8f,0,0,1);
 dark=render(program,3,.8f,0,0,.12f);depleted=render(program,3,0,0,1,1);
 recharge=render(program,3,0,.75f,0,1);
 CHECK(healthy==0&&hit>0&&depleted>0&&recharge>0);CHECK(dark>0&&dark<hit/4);
 CHECK(render(program,4,0,0,1,1)!=depleted);
 glDeleteShader(vertex);glDeleteShader(fragment);
 SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
'''
    vertex = version + (
        "out vec4 xD0,xD1,xB0,xB1,xT0,xT1,xT2,xT3;out float xFog;out vec3 xShieldPosition;"
        "void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0,1);"
        "xD0=vec4(1,.7,.1,.8);xD1=xB0=xB1=xT0=xT1=xT2=xT3=vec4(0);xFog=1.0;"
        "xShieldPosition=vec3(p*.17,.1);}")
    import json
    code = code.replace(" SETUP", setup).replace("VERTEX_SOURCE", json.dumps(vertex))
    source = tmp_path / "shield_gpu.c"
    binary = tmp_path / ("shield_gpu.exe" if os.name == "nt" else "shield_gpu")
    source.write_text(code)
    result = subprocess.run([*flags, str(source), *libraries, "-o", str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(binary)], env=env, capture_output=True, text=True, timeout=30)
    if result.returncode == 77:
        pytest.skip("hidden OpenGL 4.5 context unavailable")
    assert result.returncode == 0, result.stderr


def test_enhanced_shield_scope(tmp_path):
    compiler = shutil.which("clang")
    if not compiler:
        pytest.skip("clang required")
    feature = (ROOT / "port/linux/game/shield_look.c").read_text()
    renderer = (ROOT / "port/linux/src/shield_look_gl.c").read_text()
    # Compile the real modules together; only their engine/GL-facing types
    # and services are replaced. This also exercises the cached setting.
    feature = "\n".join(line for line in feature.splitlines() if not line.startswith("#include"))
    renderer = renderer[:renderer.index("void shield_look_emit")]
    renderer = "\n".join(line for line in renderer.splitlines() if not line.startswith("#include"))
    arms = (ROOT / "source/interface/first_person_weapons.c").read_text()
    start = arms.index("if (viewmodel_shield_is_visible() || shield_look_enabled())")
    end = arms.index("{", start) + 1
    depth = 1
    while depth:
        depth += (arms[end] == "{") - (arms[end] == "}")
        end += 1
    arms = arms[start:end]
    code = r'''
#include <stdio.h>
#include <string.h>
typedef int boolean;
#define NONE -1
#define TRUE 1
#define PIN(x,a,b) ((x)<(a)?(a):(x)>(b)?(b):(x))
#define TEST_FLAG(x,b) ((x)&(1u<<(b)))
enum {_object_type_biped, _object_type_vehicle, _object_type_weapon,
      _object_type_scenery, _object_invisible_bit=8, _object_dead_bit=9,
      _object_shield_depleted_bit=10, _object_shield_charging_bit=11};
enum {_object_mask_unit=3};
struct shader {struct {int type;} base;};
struct object_data {int type;unsigned flags,damage_flags;float maximum_shield_vitality,
 shield_vitality;float outgoing_function_values[4],outgoing_change_colors[12];long parent_object_index;};
struct object_datum {long definition_index;struct object_data object;};
struct unit_datum {long definition_index;struct object_data object;struct {float active_camouflage;} unit;};
struct object_definition {struct {struct {long index;} modifier_shader;} object;};
static struct unit_datum units[4];
static struct shader shield={{10}},other={{10}},nonplasma={{1}};
static struct object_definition definition={{{2}}};
static unsigned long version;
static const char *setting="classic";
static unsigned long config_changes(void){return version;}
static const char *config_string(const char *name){return setting;}
static struct object_datum *object_try_and_get(long id){return id>=0&&id<4?(struct object_datum*)&units[id]:NULL;}
static struct object_definition *object_definition_get(long id){return id==0?&definition:NULL;}
static struct shader *shader_definition_get(long id){return id==2?&shield:NULL;}
static int arms_visible;
static int viewmodel_shield_is_visible(void){return arms_visible;}
static int shader_type_is_valid_for_modifier(int type){return type==10||type==1;}
#define object_get object_try_and_get
#define shield_color_colors(index,colors) (colors)
void shield_look_end(void);
void shield_look_set(float const *,float const *,float const *);
''' + renderer + feature + r'''
static int attach_arms(long unit_index){
 struct unit_datum *unit=&units[unit_index];
 struct {struct shader *modifier_shader;struct {float const *colors,*values;} modifier_animation;} model_effect={0};
''' + arms + r'''
 return model_effect.modifier_shader!=NULL;
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 float color[3]={1,.65f,.07f}, white[3]={1,1,1}, copied[4]={0};
 float const *v=units[0].object.outgoing_function_values,*u;
 struct unit_datum saved;
 units[0].object.maximum_shield_vitality=1;
 units[0].object.shield_vitality=.25f;
 units[0].object.damage_flags=1u<<_object_shield_depleted_bit;
 units[1].object.type=_object_type_weapon;units[1].object.parent_object_index=0;
 saved=units[0];
 CHECK(!attach_arms(0));arms_visible=1;CHECK(attach_arms(0));arms_visible=0;
 CHECK(!shield_look_visible(0,&shield,v,0));
 shield_look_begin(0,&shield,v,0,3,.8f,color,white,.2f,1);CHECK(!shield_look_active());
 setting="enhanced";version++;
 CHECK(attach_arms(0));CHECK(!arms_visible);
 units[0].object.maximum_shield_vitality=0;CHECK(!attach_arms(0));units[0]=saved;
 shield.base.type=1;CHECK(!attach_arms(0));
 /* Classic ARM SHIELDS still accepts the same authored modifier types. */
 arms_visible=1;CHECK(attach_arms(0));arms_visible=0;shield.base.type=10;
 CHECK(shield_look_eligible(0,&shield,v,0));
 CHECK(shield_look_visible(0,&shield,v,0));
 CHECK(shield_look_visible(1,&shield,v,1));
 CHECK(!shield_look_visible(1,&shield,v,0));
 CHECK(!shield_look_visible(1,&shield,units[1].object.outgoing_function_values,1));
 CHECK(!shield_look_visible(0,&shield,copied,0));
 CHECK(!shield_look_visible(0,&other,v,0));
 CHECK(!shield_look_visible(0,&nonplasma,v,0));
 CHECK(!shield_look_visible(-1,&shield,v,0));
 CHECK(!shield_look_visible(0,&shield,NULL,0));
 units[0].object.maximum_shield_vitality=0;CHECK(!shield_look_visible(0,&shield,v,0));units[0]=saved;
 units[0].object.type=_object_type_scenery;CHECK(!shield_look_visible(0,&shield,v,0));units[0]=saved;
 units[0].object.flags=1u<<_object_invisible_bit;CHECK(!shield_look_visible(0,&shield,v,0));units[0]=saved;
 units[0].object.damage_flags|=1u<<_object_dead_bit;CHECK(!shield_look_visible(0,&shield,v,0));units[0]=saved;
 units[0].unit.active_camouflage=1;CHECK(!shield_look_visible(0,&shield,v,0));units[0]=saved;
 shield_look_begin(0,&shield,v,0,3,.8f,color,white,.2f,1);CHECK(shield_look_active());
 u=shield_look_uniforms();CHECK(u[0]==3&&u[1]==.8f&&u[2]==0&&u[3]==1);
 CHECK(u[4]==1&&u[5]==.65f&&u[6]==.07f&&u[7]==1&&u[11]==0);
 shield_look_end();CHECK(!shield_look_active());
 units[0].object.damage_flags=1u<<_object_shield_charging_bit;
 units[0].unit.active_camouflage=.25f;
 shield_look_begin(1,&shield,v,1,5,0,color,white,.2f,1);CHECK(shield_look_active());
 CHECK(u[0]==5&&u[1]==0&&u[2]==.75f&&u[3]==0&&u[7]==.75f&&u[11]==1);
 /* An ordinary plasma draw must not inherit a preceding shield scope. */
 shield_look_begin(0,&other,v,0,5,1,color,white,0,1);CHECK(!shield_look_active());
 units[0].object.damage_flags=0;CHECK(!shield_look_visible(0,&shield,v,0));
 CHECK(shield_look_eligible(0,&shield,v,0));
 shield_look_begin(0,&shield,v,0,8,2,color,white,0,1);CHECK(shield_look_active()&&u[1]==1);
 setting="classic";version++;
 shield_look_begin(0,&shield,v,0,8,1,color,white,0,1);CHECK(!shield_look_active());
 CHECK(!shield_look_visible(0,&shield,v,0));
 setting="invalid";version++;CHECK(!shield_look_enabled());
 setting="enhanced";version++;
 /* A recycled/deleted parent or an unrelated modifier cannot inherit state. */
 units[1].object.parent_object_index=-1;CHECK(!shield_look_visible(1,&shield,v,1));
 units[0].definition_index=1;shield_look_begin(0,&shield,v,0,9,1,color,white,0,1);CHECK(!shield_look_active());
 return 0;
}
'''
    source = tmp_path / "shield_look.c"
    source.write_text(code)
    binary = tmp_path / ("shield_look.exe" if os.name == "nt" else "shield_look")
    target = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld", "-D_CRT_SECURE_NO_WARNINGS"] if os.name == "nt" else []
    result = subprocess.run([compiler, *target, "-std=gnu89", str(source), "-o", str(binary)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
