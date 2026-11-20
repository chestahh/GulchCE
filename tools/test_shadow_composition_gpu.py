"""Run actual model-lighting and visibility GLSL with independent light masks.

Explicit Windows GPU check: SHADOW_COMPOSITION_GPU_TEST=1. Synthetic pixel
evidence complements the actual engine's staged framebuffer captures.
"""
import ctypes as C
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np
import pytest

from tools.harness import function, structure
from tools.modern_shadow_geometry_gpu import Context
from tools.shadow_stencil_compare import Replay, U, I, P, F

ROOT = Path(__file__).resolve().parents[1]
pytestmark = pytest.mark.skipif(os.name != 'nt' or os.getenv('SHADOW_COMPOSITION_GPU_TEST') != '1',
                                reason='Explicit Windows GL composition check')


@pytest.fixture(scope='module')
def emitted(tmp_path_factory):
    folder = tmp_path_factory.mktemp('composition-shaders')
    header = (ROOT / 'port/linux/src/xgpu.h').read_text()
    plasma = (ROOT / 'port/linux/src/plasma_model_shader.c').read_text()
    code = '''#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
typedef int BOOL;
#define XGPU_MODEL_LIGHT_COUNT 12
'''
    code += structure(header, 'xgpu_text')
    code += '''
void xgpu_text_append(struct xgpu_text *text, const char *format, ...) {
    va_list args; (void)text;
    va_start(args, format); vprintf(format, args); va_end(args);
}
'''
    code += function(plasma, 'plasma_model_emit_uniform')
    code += function(plasma, 'plasma_model_emit_pixel')
    code += function((ROOT / 'port/linux/src/nv2a_psh.c').read_text(), 'model_lighting')
    code += function((ROOT / 'port/linux/src/modern_shadows_gl.c').read_text(), 'modern_shadow_emit')
    code += '''
int main(void) {
    struct xgpu_text text = {0};
    modern_shadow_emit(&text);
    model_lighting(&text, 1, 1, 1);
    return 0;
}
'''
    source = folder / 'emit.c'
    source.write_text(code)
    executable = folder / 'emit.exe'
    subprocess.run([shutil.which('clang'), '-m32', '-std=gnu89', '-O1', '-fuse-ld=lld',
                    str(source), '-o', str(executable)], check=True, capture_output=True)
    return subprocess.check_output([str(executable)], text=True)


@pytest.mark.parametrize('soft', [0, 1])
@pytest.mark.parametrize('blocked', [(False, False), (True, False), (False, True), (True, True)])
@pytest.mark.parametrize('distant_blocked', [None, (False, False), (True, False), (False, True), (True, True)])
def test_each_light_shadows_only_its_own_contribution(emitted, soft, blocked, distant_blocked):
    context = Context()
    replay = None
    try:
        replay = Replay(context, 32, 32)
        g = context.gl
        vertex = '''#version 330 core
out vec3 xWorldPosition; out vec4 xWorldNormal;
void main() {
    vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2-1;
    xWorldPosition=vec3(p,0); xWorldNormal=vec4(0,0,1,1);
    gl_Position=vec4(p,0,1);
}
'''
        fragment = '#version 330 core\n' + emitted + '\nout vec4 result; void main(){result=vec4(model_lighting(),1);}\n'
        program = replay.program(vertex, fragment)
        lights = np.zeros((12, 4), np.float32)
        for start, rgb in ((1, (.3, 0, 0)), (4, (0, .25, 0))):
            lights[start] = [0, 0, 2, 0]
            lights[start + 2] = [*rgb, 1]
        lights[11, :3] = [.1, .12, .14]
        for slot in (0, 1):
            texture = replay.new('Textures')
            g('ActiveTexture', None, U)(0x84C0 + slot)
            g('BindTexture', None, U, U)(0x8513, texture)
            # A blocker one unit from the source, or an empty map. The receiver
            # is two units away; use the production perspective-depth encoding.
            depth = .005 * (4 / 1 - 1) / (4 - .005) if blocked[slot] else 0
            pixels = np.full((16, 16), depth, np.float32)
            for face in range(6):
                g('TexImage2D', None, U, I, I, I, I, I, U, U, P)(
                    0x8515 + face, 0, 0x822E, 16, 16, 0, 0x1903, 0x1406, pixels.ctypes.data)
            for parameter in (0x2800, 0x2801):
                g('TexParameteri', None, U, U, I)(0x8513, parameter, 0x2600)
        distant = np.array([[.15, .10, .05], [.08, .11, .03]], np.float32)
        if distant_blocked is not None:
            lights[7, :3] = lights[9, :3] = [0, 0, -1]
            lights[8, :3], lights[10, :3] = distant
            for slot in (0, 1):
                texture = replay.new('Textures')
                g('ActiveTexture', None, U)(0x84C0 + 11 + slot)
                g('BindTexture', None, U, U)(0x0DE1, texture)
                depth = np.full((16, 16), .75 if distant_blocked[slot] else 0, np.float32)
                g('TexImage2D', None, U, I, I, I, I, I, U, U, P)(0x0DE1, 0, 0x822E, 16, 16, 0, 0x1903, 0x1406, depth.ctypes.data)
                for parameter in (0x2800, 0x2801):
                    g('TexParameteri', None, U, U, I)(0x0DE1, parameter, 0x2600)
        replay.begin()
        g('Disable', None, U)(0x0B71)
        g('UseProgram', None, U)(program)
        for name, value in {'modern_depth': 0, 'modern_depth_other': 1,
                            'modern_environment_depth0': 11, 'modern_environment_depth1': 12,
                            'modern_environment_mask': 0 if distant_blocked is None else 3,
                            'modern_environment_soft': soft,
                            'modern_light': [0, 0, 2, 4], 'modern_light_other': [0, 0, 2, 4],
                            'modern_slots': [1, 1], 'modern_soft': soft,
                            'plasma_model_slots': [1, 1], 'plasma_occ_enabled': [0, 0],
                            'plasma_occ_texture0': 2, 'plasma_occ_texture1': 3}.items():
            if isinstance(value, list) and len(value) == 2:
                location = g('GetUniformLocation', I, U, C.c_char_p)(program, name.encode())
                g('Uniform2f', None, I, F, F)(location, *value)
            else:
                replay.uniform(program, name, value)
        matrix = np.stack([np.eye(4, dtype=np.float32)] * 2)
        location = g('GetUniformLocation', I, U, C.c_char_p)(program, b'modern_environment_frames')
        g('UniformMatrix4fv', None, I, I, U, P)(location, 2, 0, matrix.ctypes.data)
        location = g('GetUniformLocation', I, U, C.c_char_p)(program, b'modern_environment_radius')
        g('Uniform1f', None, I, F)(location, 1)
        location = g('GetUniformLocation', I, U, C.c_char_p)(program, b'model_lights')
        g('Uniform4fv', None, I, I, P)(location, 12, lights.ctypes.data)
        g('BindVertexArray', None, U)(replay.vao)
        g('DrawArrays', None, U, I, I)(4, 0, 3)
        pixels = replay.read()[15:17, 15:17, :3]
        expected = np.array([.1 + (0 if blocked[0] else .3),
                             .12 + (0 if blocked[1] else .25), .14])
        distant_sum = np.zeros(3)
        if distant_blocked is not None:
            for slot in (0, 1):
                if not distant_blocked[slot]: distant_sum += distant[slot]
        expected += distant_sum
        assert np.allclose(pixels, expected, atol=.0003), (pixels, expected)
        # A receiver-owner exemption disables only its slot's visibility map.
        location = g('GetUniformLocation', I, U, C.c_char_p)(program, b'modern_slots')
        g('Uniform2f', None, I, F, F)(location, 0, 0)
        g('DrawArrays', None, U, I, I)(4, 0, 3)
        assert np.allclose(replay.read()[15:17, 15:17, :3], np.array([.4, .37, .14]) + distant_sum, atol=.0003)
    finally:
        if replay:
            replay.close()
        context.close()
