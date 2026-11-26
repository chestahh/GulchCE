"""Build the bounded M4A single-grenade Soft capture. No experiments in regular builds."""
import argparse
import ast
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError('Grenade capture integration changed: ' + old[:64])
    return source.replace(old, new, 1)


def receiver_experiment(source):
    """Share the real comparison and reversed radial-depth decode with the blocker search."""
    start = source.index('\t\t"float modern_compare(')
    end = source.index('\t\t"float modern_visibility_map(', start)
    compare = ''.join(ast.literal_eval(line) for line in re.findall(r'"(?:[^"\\]|\\.)*"', source[start:end]))
    sample = replace_once(compare, 'float modern_compare(', 'vec2 grenade_depth_sample(')
    sample = replace_once(sample, 'vec3 normal) {', 'vec3 normal, bool offset_tap) {')
    sample = replace_once(sample, 'if(dot(normal,normal)>0.5', 'if(!offset_tap && dot(normal,normal)>0.5')
    sample = replace_once(sample, 'return 1.0;', 'return vec2(1.0, radius);')
    sample = replace_once(sample, 'return shadow_visible(z,stored);', '''
 if(offset_tap) return grenade_offset_sample(stored*radius, receiver, q, normal, numerator);
 return vec2(step(z,stored),stored*radius);''')
    wrapper = compare.split('{', 1)[0] + '{ return grenade_depth_sample(depth,radius,sample_direction,receiver,normal,false).x; }\n'
    declaration = 'vec2 grenade_offset_sample(float, vec3, vec3, vec3, float);\n'
    shader = declaration + sample + wrapper + (ROOT/'tools/fixtures/shadow_grenade_soft.glsl').read_text()
    literals = ''.join('\t\t' + json.dumps(line) + '\n' for line in shader.splitlines(keepends=True))
    source = source[:start] + literals + source[end:]
    marker = '\t\t" if(modern_soft==0) return modern_compare(depth,light.w,d,d,n);\\n"\n'
    # Keep the existing four-tap Soft path available as an explicit control (mode 1).
    return replace_once(source, marker, marker + '\t\t" if(modern_soft==2) return grenade_soft_visibility(depth,light,d,n);\\n"\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    out = parser.parse_args().output.resolve()
    if not out.is_relative_to(ROOT/'build') or out.exists():
        parser.error('Use a fresh output below build/')
    renderer = ROOT/'port/linux/src/modern_shadows_gl.c'
    attachment = ROOT/'port/linux/game/plasma_lighting.c'
    originals = {p:p.read_bytes() for p in (renderer, attachment)}
    try:
        # Only the controlled, single-grenade fixture may activate the candidate.
        # Hard and Soft use the same accepted source origin and ground map.
        src = receiver_experiment(renderer.read_text())
        marker = 'soft_filter = soft;'
        src = replace_once(src, marker, marker + '''
    if (soft && shadow_size == 1024 && getenv("HALO_SHADOW_NEAR_SOURCE") &&
        (!strcmp(getenv("HALO_SHADOW_NEAR_SOURCE"),"grenade") || !strcmp(getenv("HALO_SHADOW_NEAR_SOURCE"),"stuck")))
        soft_filter = 2;''')
        src = replace_once(src, 'ground_grenade && !soft &&', 'ground_grenade && (!soft || soft_filter == 2) &&')
        renderer.write_text('#include <stdlib.h>\n'+src)
        attachment.write_text(replace_once(attachment.read_text(), 'modern_shadows_hard_enabled();',
                                           'modern_shadows_enabled();'))
        subprocess.run([sys.executable, 'tools/shadow_near_source_build.py', '--defer-restored-build',
                        '--output', str(out)], cwd=ROOT, check=True)
    finally:
        for path, content in originals.items():
            path.write_bytes(content)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
