"""Scene readback tests use the production implementation with a delayed GPU."""
import os
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]
BACKEND = ROOT / 'port/linux/src/vulkan'

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
