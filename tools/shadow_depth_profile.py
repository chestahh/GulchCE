"""Diagnostic-only local-depth attribution; installed by the timing builder.

Kinds 6/7/8/9 are setup+clear, world, world-space models and viewmodel.
Queries retain original draw order and never overlap the whole-pass query.
All modified production files belong to the builder's restoration set.
"""
import re
from harness import function


def replace_once(text, before, after):
    assert text.count(before) == 1, before
    return text.replace(before, after, 1)


def install(root):
    path = root / 'port/linux/game/modern_shadows.c'
    text = path.read_text()
    for name, world in [('world_casters_clipped', 1), ('model_caster', 0)]:
        body = function(text, name)
        anchor = '\tint lightmap, index;' if world else '\tint node, region;'
        updated = replace_once(body, anchor, anchor + f'\n\tlighting_shadow_geometry({world});')
        text = replace_once(text, body, updated)
    path.write_text('void lighting_shadow_geometry(int);\n' + text)

    path = root / 'port/linux/src/modern_shadows_gl.c'
    text = path.read_text()
    text = replace_once(text, '} draws[SHADOW_DRAWS];', '\tint profile_kind;\n} draws[SHADOW_DRAWS];')
    text = replace_once(text, '\tdraws[draw_count].mesh = mesh - 1;',
                        '\tdraws[draw_count].profile_kind = lighting_shadow_world ? 7 : views == MODERN_SHADOW_VIEW ? 9 : 8;\n'
                        '\tdraws[draw_count].mesh = mesh - 1;')
    body = function(text, 'lighting_timed_shadow_end')
    declaration = re.search(r'\tint face, draw, pass[^\n]+;', body)[0]
    updated = replace_once(body, declaration, declaration[:-1] + ', depth_kind;')
    updated = replace_once(updated, '\tif (!failed)\n\t{', '\tif (!failed)\n\t{\n\t\tlighting_gpu_depth_begin(6);')
    updated = replace_once(updated, '\t\tfor (pass = 0;', '\t\tlighting_gpu_shadow_end();\n\t\tfor (pass = 0;')
    line = re.search(r'(?m)^([ \t]*)(?:glFramebufferTexture2D[^\n]*POSITIVE_X \+ face|finite_layer\([^\n]*sample\*6\+face)[^\n]*', updated)
    assert line, 'Missing per-face depth attachment'
    indent = line[1].split('\n')[-1]
    updated = replace_once(updated, line[0], indent + 'depth_kind = 6;\n' +
                           indent + 'lighting_gpu_depth_begin(6);\n' + line[0])
    anchor = re.search(r'(?m)^[ \t]*if \(!\(draws\[draw\].faces[^\n]+continue;', updated)[0]
    pad = anchor[:len(anchor)-len(anchor.lstrip())]
    updated = replace_once(updated, anchor, anchor + '\n' + pad +
        'if (depth_kind != draws[draw].profile_kind)\n' + pad + '{\n' + pad +
        '\tlighting_gpu_shadow_end();\n' + pad + '\tdepth_kind = draws[draw].profile_kind;\n' +
        pad + '\tlighting_gpu_depth_begin(depth_kind);\n' + pad + '}')
    anchor = pad + 'glDrawElements(GL_TRIANGLES, meshes[mesh].count, GL_UNSIGNED_SHORT, NULL);'
    updated = replace_once(updated, anchor, pad + 'lighting_gpu_depth_draw(meshes[mesh].count / 3);\n' + anchor)
    anchor = anchor + '\n' + pad[:-1] + '}'
    updated = replace_once(updated, anchor, anchor + '\n' + pad[:-1] + 'lighting_gpu_shadow_end();')
    copy = re.search(r'(?m)^([ \t]*)glCopyImageSubData\([^;]+;', updated)
    if copy:
        pad = copy[1]
        updated = replace_once(updated, copy[0], pad + '{\n' + pad +
            '\tlighting_gpu_depth_begin(6);\n\t' + copy[0] + '\n' + pad +
            '\tlighting_gpu_shadow_end();\n' + pad + '}')
    elif '/* finite copy begin */' in updated:
        updated = replace_once(updated, '/* finite copy begin */', 'lighting_gpu_depth_begin(6);')
        updated = replace_once(updated, '/* finite copy end */', 'lighting_gpu_shadow_end();')
    text = replace_once(text, body, updated)
    wrapper = function(text, 'modern_shadow_end')
    # Split queries replace the outer timer, retaining its CPU phase timer.
    text = replace_once(text, wrapper, wrapper.replace('\tlighting_gpu_shadow_begin();\n', '')
                        .replace('\tlighting_gpu_shadow_end();\n', ''))
    path.write_text('static int lighting_shadow_world;\n'
                   'void lighting_shadow_geometry(int world) { lighting_shadow_world = world; }\n'
                   'void lighting_gpu_depth_begin(int);\nvoid lighting_gpu_depth_draw(int);\n' + text)


def install_reuse_control(root):
    path = root / 'port/linux/src/modern_shadows_gl.c'
    text = path.read_text()
    anchor = '\tshared = map->view_map;'
    text = replace_once(text, anchor, anchor + '\n\t{ static int mode = -1;\n'
        '\t  if(mode < 0) { const char *p=getenv("HALO_BENCH_DEPTH_REUSE"); mode=p && !strcmp(p,"legacy") ? 1 : p && !strcmp(p,"alternate") ? 2 : 0; }\n'
        '\t  if(mode==1) shared=0;\n'
        '\t  if(mode==2) { int part=(int)(lighting_performance_seconds()/2.0)%4; if(part==1 || part==2) shared=0; }\n\t}\n')
    path.write_text('#include <stdlib.h>\ndouble lighting_performance_seconds(void);\n' + text)

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
