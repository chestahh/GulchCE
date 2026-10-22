"""Scene readback tests use the production implementation with a delayed GPU."""
import os
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]
BACKEND = ROOT / 'port/linux/src/vulkan'


def test_honeycomb_state_and_interface(tmp_path):
    from harness import function
    cc = shutil.which('clang')
    if not cc:
        pytest.skip('clang required')
    game = (BACKEND / 'vulkan_game.c').read_text()
    renderer = (ROOT / 'port/linux/src/d3d8_gl.c').read_text()
    code = r'''
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "vulkan_scene.h"
static struct { struct halo_vk_uniforms uniforms; } game;
static int halo_vulkan_scene_active;
static float shield_honeycomb_value[3][4];
#define HALO_NATIVE_VULKAN 1
''' + function(game, 'halo_vk_game_honeycomb') + function(renderer, 'd3d8_gl_set_shield_honeycomb') + r'''
int main(void) {
    float difference[4]={.1f,.2f,.3f,0.f}, parallel[4]={.6f,.5f,.4f,.8f};
    char *vertex, *pixel;
    d3d8_gl_set_shield_honeycomb(87.5f,7.f,difference,parallel,1.f);
    assert(game.uniforms.shield_honeycomb[0][3]==0.f);
    halo_vulkan_scene_active=1;
    d3d8_gl_set_shield_honeycomb(225.f,8.f,difference,parallel,3.f);
    assert(game.uniforms.shield_honeycomb[0][3]==225.f);
    assert(game.uniforms.shield_honeycomb[0][0]==3.f);
    assert(game.uniforms.shield_honeycomb[1][3]==8.f);
    assert(!memcmp(game.uniforms.shield_honeycomb[1],difference,3*sizeof(float)));
    assert(!memcmp(game.uniforms.shield_honeycomb[2],parallel,sizeof(parallel)));
    d3d8_gl_set_shield_honeycomb(0.f,0.f,NULL,NULL,3.f);
    assert(game.uniforms.shield_honeycomb[0][3]==0.f);
    vertex=halo_vk_shader_interface("out vec2 shield_uv;\n",0);
    pixel=halo_vk_shader_interface("in vec2 shield_uv;\nuniform vec4 shield_honeycomb[3];\n",1);
    assert(vertex && pixel);
    assert(strstr(vertex,"layout(location=9) out vec2 shield_uv;"));
    assert(strstr(pixel,"layout(location=9) in vec2 shield_uv;"));
    assert(strstr(pixel,"vec4 shield_honeycomb[3];\n};"));
    assert(!strstr(pixel,"uniform vec4 shield_honeycomb"));
    free(vertex); free(pixel);
    return 0;
}
'''
    source = tmp_path / 'honeycomb.c'
    source.write_text(code)
    output = tmp_path / ('honeycomb.exe' if os.name == 'nt' else 'honeycomb')
    flags = ['--target=i686-pc-windows-msvc', '-fuse-ld=lld'] if os.name == 'nt' else ['-m32']
    subprocess.run([cc,*flags,'-std=gnu89','-I'+str(BACKEND),str(source),str(BACKEND/'vulkan_shader.c'),'-o',str(output)],check=True,capture_output=True)
    subprocess.run([str(output)],check=True,capture_output=True)

@pytest.mark.parametrize('fault', [None, 'early', 'partial', 'wait', 'old'])
def test_scene_completion(tmp_path, fault):
    cc = shutil.which('clang')
    if not cc:
        pytest.skip('clang required')
    code = (BACKEND / 'vulkan_scene.c').read_text()
    changes = {
        'early': ('if (result == VK_NOT_READY)', 'if (0)'),
        'partial': ('if (!results[j * 2 + 1])', 'if (0)'),
        'wait': ('VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT', 'VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | VK_QUERY_RESULT_WAIT_BIT'),
        'old': ('if (b->serial > scene.results[b->index].serial)', 'if (1)'),
    }
    if fault:
        before, after = changes[fault]
        assert before in code
        code = code.replace(before, after, 1)
    (tmp_path / 'scene_under_test.c').write_text(code)
    stub = tmp_path / 'SDL3/SDL.h'
    stub.parent.mkdir()
    stub.write_text('typedef struct SDL_SharedObject SDL_SharedObject;\nconst char *SDL_GetBasePath(void);\nconst char *SDL_GetError(void);\nSDL_SharedObject *SDL_LoadObject(const char *);\nvoid *SDL_LoadFunction(SDL_SharedObject *,const char *);\nvoid SDL_UnloadObject(SDL_SharedObject *);\n')
    output = tmp_path / ('scene.exe' if os.name == 'nt' else 'scene')
    flags = ['-std=gnu89', '-Werror', '-D_CRT_SECURE_NO_WARNINGS', '-Wno-missing-field-initializers', '-I'+str(tmp_path), '-I'+str(BACKEND), '-I'+str(ROOT/'port/third_party/vulkan_headers/include')]
    flags += ['--target=i686-pc-windows-msvc', '-fuse-ld=lld'] if os.name == 'nt' else ['-m32']
    subprocess.run([cc,*flags,str(BACKEND/'tests/scene_completion.c'),str(BACKEND/'vulkan_shader.c'),'-o',str(output)],check=True,capture_output=True)
    result = subprocess.run([str(output)],capture_output=True,timeout=15)
    assert (result.returncode==0)==(fault is None),result.stderr.decode(errors='replace')
