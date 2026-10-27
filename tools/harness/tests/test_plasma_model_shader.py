"""Translate the real stock programs with the feature's shader additions."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run, structure


def test_stock_programs():
    header = read('port/linux/src/xgpu.h')
    code = structure(header, 'xgpu_text') + '\n'
    code += structure(header, 'nv2a_vertex_lighting') + '\n'
    code += read('port/linux/include/plasma_model_shader.h')
    for path in ['port/linux/src/plasma_model_shader.c', 'port/linux/src/nv2a_vsh.c']:
        code += re.sub(r'^#include.*$', '', read(path), flags=re.M)
    words = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+',
        re.sub(r'/\*.*?\*/', '', read('source/rasterizer/xbox/rasterizer_xbox_vertex_shaders_data.inc'), flags=re.S))]
    programs = []
    for index, word in enumerate(words):
        if word & 0xffff == 0x2078 and 0 < word >> 16 <= 136:
            programs.append(words[index+1:index+1+4*(word >> 16)])
    code += '\nstatic const unsigned long programs[][544] = {\n' + ',\n'.join(
        '{' + ','.join(str(w)+'UL' for w in p) + '}' for p in programs) + '};\n'
    code += 'static const unsigned long lengths[] = {' + ','.join(str(len(p)//4) for p in programs) + '};\n'
    status, output = run(build('plasma_model_shader', (('under_test.inc', code),)), 'programs')
    assert status == 0, output
