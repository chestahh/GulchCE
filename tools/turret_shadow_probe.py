"""Capture-build-only finite-source shadow experiment; never a production hook.

Keep one lighting contribution and average independent visibility samples.
This approximates an extended emitter's visibility, not its near-field BRDF.
The source disk dimensions are explicit experimental inputs, not tag-derived.
"""
from pathlib import Path


PATHS = ['port/linux/game/muzzle_lighting.c', 'port/linux/game/modern_shadows.c',
         'port/linux/src/modern_shadows_gl.c', 'port/linux/include/modern_shadows.h']


def replace(text, before, after, count=1):
    assert text.count(before) == count, before
    return text.replace(before, after)


def install(root):
    root = Path(root)
    path = root/PATHS[0]
    text = path.read_text()
    text = replace(text, 'real start, radius, duration;', 'real start, radius, duration;\n\tfloat probe_axis[3], probe_up[3];')
    text = replace(text, 'forward = marker.matrix.forward;', 'forward = marker.matrix.forward;\n\t\tup = marker.matrix.up;')
    text = replace(text, '\tif (profile->forward_offset != 0.0f)',
                   '\tmemcpy(pulse->probe_axis, &forward, sizeof(pulse->probe_axis));\n'
                   '\tmemcpy(pulse->probe_up, &up, sizeof(pulse->probe_up));\n\tif (profile->forward_offset != 0.0f)')
    text += '''
int turret_probe_basis(long rasterizer, float axis[3], float up[3])
{
    int i;
    for (i=0;i<MUZZLE_LIGHTS;i++)
        if (pulses[i].active && pulses[i].rasterizer==rasterizer && muzzle_profiles[pulses[i].profile].vehicle_occlusion)
        {
            memcpy(axis,pulses[i].probe_axis,3*sizeof(float));
            memcpy(up,pulses[i].probe_up,3*sizeof(float));
            return 1;
        }
    return 0;
}
'''
    path.write_text(text)
    path = root/PATHS[1]
    text = path.read_text()
    text = 'int turret_probe_basis(long, float *, float *);\nvoid turret_probe_source(const float *, const float *);\n' + text
    text = replace(text, '\t\tfloat radius;', '\t\tfloat radius, axis[3], up[3];')
    text = replace(text, '\t\tif (!modern_shadow_begin(slot,',
                   '\t\tif (turret_probe_basis(index,axis,up)) turret_probe_source(axis,up);\n'
                   '\t\telse turret_probe_source(NULL,NULL);\n\t\tif (!modern_shadow_begin(slot,')
    # Conservative source disk bound, while the actual attenuation stays unchanged.
    text = replace(text, 'world_casters(bsp, &light->position, radius)', 'world_casters(bsp, &light->position, radius + .5f)')
    text = replace(text, 'object_casters(&light->position, radius,', 'object_casters(&light->position, radius + .5f,')
    path.write_text(text)
    path = root/PATHS[3]
    text = replace(path.read_text(), 'int light, other_light, slots, filter, extent;',
                   'int light, other_light, slots, filter, extent, offsets, other_offsets;')
    path.write_text(text)
    path = root/PATHS[2]
    text = path.read_text()
    text = replace(text, '#ifndef HALO_GLES', '#ifndef HALO_GLES\n#include <stdlib.h>\n#include <SDL3/SDL.h>', 1)
    text = replace(text, 'float light[4];', 'float light[4], offsets[4][4];\n\tint samples;')
    at = 'extern long halo_shadow_resolution_scale(void);'
    text = replace(text, at, at + '\n' + (root/'tools/fixtures/turret_shadow_probe.inc').read_text())
    text = replace(text, '\tint face;\n\tglGenTextures', '\tglGenTextures')
    text = text.replace('GL_TEXTURE_CUBE_MAP,', 'GL_TEXTURE_CUBE_MAP_ARRAY,')
    text = replace(text, '''\tfor (face = 0; face < 6; face++)
\t\tglTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT32F,
\t\t\tshadow_size, shadow_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);''',
                   '\tglTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY,0,GL_DEPTH_COMPONENT32F,shadow_size,shadow_size,6*probe_capacity(),0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);')
    text = replace(text, '\tmap->valid = 0;\n\tdraw_count = 0;', '\tmap->valid = 0;\n\tprobe_offsets(map);\n\tdraw_count = 0;')
    text = text.replace('glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, map->depth_cube[0], 0);',
                        'probe_layer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, map->depth_cube[0], 0, 0);')
    text = text.replace('glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, map->depth_cube[1], 0);',
                        'probe_layer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, map->depth_cube[1], 0, 0);')
    text = replace(text, '\tfaces = mesh_faces(&meshes[mesh - 1], matrices, node_count, map->light);',
                   '\tfaces = probe_faces(&meshes[mesh - 1], matrices, node_count);')
    text = replace(text, '\tint face, draw, pass;', '\tint face, draw, pass, sample;\n\tfloat source[4];')
    text = replace(text, '\t\tfor (pass = 0; pass < 1 + map->view_map; pass++)',
                   '\t\tfor (sample = 0; sample < map->samples; sample++)\n\t\tfor (pass = 0; pass < 1 + map->view_map; pass++)')
    text = replace(text, '\t\t\tglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, map->depth_cube[pass], 0);',
                   '\t\t\tint axis;\n\t\t\tmemcpy(source,map->light,sizeof(source));\n'
                   '\t\t\tfor(axis=0;axis<3;axis++) source[axis]+=map->offsets[sample][axis];\n'
                   '\t\t\tglUniform4fv(light_uniform,1,source);\n'
                   '\t\t\tprobe_layer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, map->depth_cube[pass], 0, sample*6+face);')
    text = text.replace('samplerCube ', 'samplerCubeArray ')
    text = replace(text, 'vec3 receiver, vec3 normal) {', 'vec3 receiver, vec3 normal, int layer) {')
    text = replace(text, 'texture(depth,q)', 'texture(depth,vec4(q,layer))')
    text = replace(text, 'vec3 d, bool contributes) {', 'vec3 d, bool contributes, int layer) {')
    for before in ['d,d,n)', 'direction,d,n)', 'direction+u,d,n)', 'direction+v,d,n)', 'direction+u+v,d,n)']:
        text = replace(text, before, before[:-1]+',layer)')
    start = text.index('\t\t"float modern_visibility(vec3 d)')
    end = text.index('\n}', start)
    text = text[:start] + '''\t\t"uniform vec4 modern_offsets[4], modern_offsets_other[4];\\n"
        "float modern_sum(samplerCubeArray depth,vec4 light,vec3 d,bool contributes,vec4 offsets[4]) {\\n"
        " float sum=0.0; int count=int(offsets[0].w);\\n"
        " for(int i=0;i<count;i++) sum+=modern_visibility_map(depth,light,d-offsets[i].xyz,contributes,i);\\n"
        " return sum/float(count); }\\n"
        "float modern_world(vec3 d,vec4 a) { return modern_sum(modern_depth,modern_light,d,any(notEqual(a,vec4(0))),modern_offsets); }\\n"
        "float modern_model(int slot,vec3 p) { if(modern_slots[slot]<0.5) return 1.0;\\n"
        " return slot==0?modern_sum(modern_depth,modern_light,p-modern_light.xyz,true,modern_offsets):modern_sum(modern_depth_other,modern_light_other,p-modern_light_other.xyz,true,modern_offsets_other); }\\n");''' + text[end:]
    text = replace(text, '\tuniforms->light = glGetUniformLocation',
                   '\tuniforms->offsets=glGetUniformLocation(linked,"modern_offsets[0]");\n'
                   '\tuniforms->other_offsets=glGetUniformLocation(linked,"modern_offsets_other[0]");\n\tuniforms->light = glGetUniformLocation')
    text = replace(text, '\t\tenabled[slot] = 1;',
                   '\t\tenabled[slot] = 1;\n\t\tglUniform4fv(slot ? uniforms->other_offsets : uniforms->offsets,4,&receiver->offsets[0][0]);')
    path.write_text(text)


def quiet_logs(text):
    """Keep shader failures visible; avoid per-frame diagnostic I/O during timing."""
    text = text.replace('platform_log("', 'turret_probe_log("')
    return '''#include <stdlib.h>
void platform_log(const char *, ...);
static int turret_probe_quiet(void) { static int mode=-1; if(mode<0) mode=getenv("HALO_TURRET_TIMING")!=NULL; return mode; }
#define turret_probe_log(format, ...) do { if(!turret_probe_quiet() || (format)[0]=='G') platform_log(format, ##__VA_ARGS__); } while(0)
''' + text
