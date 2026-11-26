"""Compare three filters on one cubemap against rays from the posed luminous strip.

Use a player near-source capture built with --reference-geometry to retain the
impact host as a camera receiver. Gameplay captures omit the issuing casing.
"""
import argparse
import ast
import ctypes as C
import json
from pathlib import Path
import re

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import shadow_stencil_compare as sc
from shadow_grenade_soft_build import receiver_experiment


GRENADE_MATERIAL = r'weapons\plasma grenade\shaders\pg '


def reference_shader():
    src = (sc.ROOT/'tools/fixtures/shadow_geometry_visibility.comp').read_text()
    src = src.replace('u < 0.0 || u > 1.0', 'u < -0.000002 || u > 1.000002')
    src = src.replace('v < 0.0 || u + v > 1.0', 'v < -0.000002 || u + v > 1.000002')
    return src


def disk_reference(ctx, triangles, positions, samples):
    """Historical receiver-facing disk, retained for the earlier width/leak replays."""
    i = np.arange(samples)
    angle = i * (np.pi * (3 - np.sqrt(5)))
    radius = 0.01 * np.sqrt((i + .5) / samples)
    offsets = np.c_[radius*np.cos(angle), radius*np.sin(angle), np.zeros(samples)]
    src = reference_shader().replace('void main()', '''
vec3 emitter_point(vec3 receiver, vec2 offset) {
    vec3 axis=normalize(receiver);
    vec3 ref=abs(axis.z) < 0.85 ? vec3(0,0,1) : vec3(0,1,0);
    vec3 u=normalize(cross(ref,axis));
    return u*offset.x + cross(axis,u)*offset.y;
}
void main()''')
    src = src.replace('if (!blocked(sources[sample_index].xyz, receivers[pixel].xyz)) ++visible;',
                      '''{
        vec3 receiver=receivers[pixel].xyz;
        vec3 emitter=emitter_point(receiver,sources[sample_index].xy);
        if (!blocked(emitter,receiver-0.0005*normalize(receiver-emitter))) ++visible;
    }''')
    return sc.ray_run(ctx, dict(triangles=triangles, receivers=positions, sources=offsets,
                      full=np.zeros((1,len(positions)))), 1, 1, shader_source=src)[0].ravel()


def luminous_samples(cap, tick, count):
    """Stratify by emitting triangle area. Both barycentrics and faces follow the pose."""
    light_mesh, light, _ = sc.captured(cap, tick, 0, material=GRENADE_MATERIAL + 'light')
    casing, _, _ = sc.captured(cap, tick, 0, material=GRENADE_MATERIAL + 'body')
    faces = light_mesh.astype(np.float64) - light[:3]
    edges = faces[:, 1:] - faces[:, :1]
    normal = np.cross(edges[:, 0], edges[:, 1])
    double_area = np.linalg.norm(normal, axis=1)
    normal /= double_area[:, None]
    # The captured luminous strip alternates winding. Its convex outer rim
    # surrounds the casing centre, which is stable under rigid rotation.
    inside = casing.reshape(-1, 3).astype(np.float64).mean(axis=0) - light[:3]
    orientation = np.sum(normal * (faces.mean(axis=1) - inside), axis=1)
    if np.any(np.abs(orientation) < 1e-6):
        raise ValueError('Ambiguous outward normal on grenade luminous strip')
    normal *= np.sign(orientation)[:, None]
    # Use triangle-local stratification so increasing the count also fills
    # each strip segment. Every sample carries equal emitting area.
    cdf = np.r_[0., np.cumsum(double_area)]
    strata = (np.arange(count) + .5) * (cdf[-1] / count)
    face = np.searchsorted(cdf[1:], strata)
    u = np.sqrt((strata - cdf[face]) / double_area[face])
    v = ((np.arange(count) + .5) * ((np.sqrt(5.) - 1.) / 2.)) % 1.
    points = faces[face, 0] + u[:, None] * ((1-v[:, None])*edges[face, 0] + v[:, None]*edges[face, 1])
    return points, normal[face]


def surface_reference(ctx, triangles, receivers, points, normals):
    """Visibility of a uniformly radiant outward-facing strip through all opaque casters."""
    sources = np.stack((points, normals), axis=1).reshape(-1, 3)
    src = reference_shader()
    # Use the scalar reference entry point. The data buffer holds pairs of
    # positions and normals. Only eligible blockers enter the reference BVH.
    src = src[:src.index('void main()')] + """
void main() {
    int pixel = first_receiver + int(gl_GlobalInvocationID.x);
    if (pixel >= receiver_count) return;
    vec3 receiver = receivers[pixel].xyz;
    float visible = 0.0, total = 0.0;
    for (int i=0; i<source_count; i+=2) {
        vec3 point = sources[i].xyz, normal = sources[i+1].xyz;
        vec3 delta = receiver-point;
        float d2 = dot(delta,delta);
        if (d2 <= 1.e-12) continue;
        vec3 direction = delta*inversesqrt(d2);
        float weight = max(dot(normal,direction),0.0)/d2;
        if (weight <= 0.0) continue;
        total += weight;
        if (!blocked(point+normal*0.000002,receiver-direction*min(0.0005,0.25*sqrt(d2))))
            visible += weight;
    }
    visibility[output_offset+pixel] = total > 0.0 ? visible/total : -1.0;
}
"""
    return sc.ray_run(ctx, dict(triangles=triangles, receivers=receivers, sources=sources,
                      full=np.zeros((1,len(receivers)))), 1, 1, shader_source=src)[0].ravel()


def grenade_geometry(cap, tick, settled):
    """Return camera receivers, per-light blockers, source position and capture metadata."""
    report_path = cap/'report.json'
    if report_path.is_file():
        report = json.loads(report_path.read_text())
        if (report['max_selected'] != 1 or not report['shadow_active_samples'] or
                report['shadow_incomplete_samples'] or report['shadow_shader_failures']):
            raise ValueError('Expected a complete single-grenade shadow capture')
    log = (cap/'stderr.log').read_text(errors='replace')
    scene = re.search(r'SHADOW SCENE created player subject (-?\d+) weapon (-?\d+)', log)
    parent = re.search(rf'NEAR grenade tick {tick} id (-?\d+) parent (-?\d+)', log)
    if not scene or not parent or int(parent[2]) != (-1 if settled else int(scene[1])):
        raise ValueError('Expected a ground grenade or a grenade attached to the captured player')
    receivers, light, info = sc.captured(cap, tick, 0)
    owner_path = cap/f'shadow-owners-{tick}-0.bin'
    if owner_path.is_file():
        owners = np.fromfile(owner_path, '<i4')
        casing, host = int(parent[1]), int(parent[2])
        if len(owners) != info['draws'] or casing not in owners or int(scene[1]) not in owners or int(scene[2]) not in owners:
            raise ValueError('Reference draw identities do not match the captured scene')
        excluded = (owners == casing)
        if not settled:
            excluded |= owners == host
        blockers, _, _ = sc.captured(cap, tick, 0, omit_draws=set(np.flatnonzero(excluded)))
        return receivers, blockers, light, info
    # Archived host-excluded captures can still measure external receivers.
    if not settled:
        # Caster IDs are sampled by the fixture at ticks 100 and 130.
        for sample in (100, 130):
            objects = re.findall(rf'SHADOW SCENE caster {sample} object (-?\d+) views \d+', log)
            if scene[1] in objects or scene[2] not in objects:
                raise ValueError('Use a host-excluded capture with the held weapon still casting')
    shell = np.concatenate([sc.captured(cap, tick, 0, material=GRENADE_MATERIAL + part)[0]
                            for part in ('body', 'button', 'light')])
    # Match posed triangles exactly. Keep the emitter mesh for sampling its luminous strip.
    def rows(array):
        return np.ascontiguousarray(array).reshape(-1, 9).view('V36').ravel()
    casing = np.isin(rows(receivers), rows(shell))
    if casing.sum() != len(shell):
        raise ValueError('Expected one complete posed grenade casing in this fixture')
    return receivers, receivers[~casing], light, info


def measure(cap, tick, settled, clip, output, width, height):
    receiver_geometry, triangles, light, info = grenade_geometry(cap, tick, settled)
    matrix = clip.copy()
    matrix[2] = 2*matrix[2]-matrix[3]
    matrix[:,3] += matrix[:,:3] @ light[:3]
    receiver_geometry -= light[:3]
    triangles -= light[:3]
    light[:3] = 0
    ctx = sc.Context()
    replay = None
    try:
        replay = sc.Replay(ctx,width,height)
        def upload(t):
            points=t.reshape(-1,3)
            return replay.upload(np.c_[points,np.ones(len(points))])
        mesh=upload(receiver_geometry)
        low,high=triangles.min(1),triangles.max(1)
        replay.face_meshes=[]
        for face in range(6):
            axis=face//2
            depth=-low[:,axis] if face%2 else high[:,axis]
            keep=depth>=sc.shadow_near()
            for other in range(3):
                if axis!=other:
                    keep &= (depth+high[:,other]>=0)&(depth-low[:,other]>=0)
            replay.face_meshes.append(upload(triangles[keep]))
        replay.begin()
        replay.draw(replay.positions,mesh,matrix)
        rgba=replay.read()
        valid=(rgba[:,:,3]>.5)&(np.linalg.norm(rgba[:,:,:3],axis=2)<light[3])
        source=receiver_experiment((sc.ROOT/'port/linux/src/modern_shadows_gl.c').read_text())
        body=source.split('void modern_shadow_emit(',1)[1].split('void modern_shadow_uniforms_initialize',1)[0]
        body = (sc.ROOT/'port/linux/src/modern_sun_shader.inl').read_text() + body
        shader=''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',body))
        program=replay.program('#version 330 core\nlayout(location=0)in vec4 position;uniform mat4 mvp;out vec3 world;void main(){world=position.xyz;gl_Position=mvp*position;}',
            '#version 330 core\nin vec3 world;out vec4 color;\n'+shader+'\nvoid main(){float v=modern_visibility(world-modern_light.xyz);color=vec4(v,v,v,1);}')
        draw=replay.draw
        bits=C.c_int()
        ctx.gl('GetIntegerv',None,sc.U,C.POINTER(sc.I))(0x0D50,C.byref(bits))
        assert 0<bits.value<21
        def record(prog,geometry,mvp):
            if prog==replay.cube_program and settled:
                ctx.gl('Enable',None,sc.U)(0x8037)
                ctx.gl('PolygonOffset',None,sc.F,sc.F)(-.5/(1<<bits.value),0)
            elif prog!=replay.cube_program:
                ctx.gl('Disable',None,sc.U)(0x8037)
            draw(prog,geometry,mvp)
        replay.draw=record
        replay.shadowmap(mesh,light,matrix,1024)  # Only depth submission for all three.
        baseline=replay.read()[:,:,0]
        replay.draw=draw
        def receiver(mode):
            replay.begin()
            ctx.gl('UseProgram',None,sc.U)(program)
            replay.uniform(program,'modern_soft',mode)
            replay.uniform(program,'modern_depth',0)
            replay.uniform(program,'modern_light',light)
            draw(program,mesh,matrix)
        images=[]
        for mode in range(3):
            receiver(mode)
            images.append(replay.read()[:,:,0])
        assert np.array_equal(images[0][valid],baseline[valid]), 'Hard control changed'
        receivers=rgba[:,:,:3][valid]
        reference=np.zeros(valid.shape,np.float32)
        points,normals=luminous_samples(cap,tick,256)
        reference[valid]=surface_reference(ctx,triangles,receivers,points,normals)
        # Back-facing receivers get no direct emission from this strip.
        # Visibility is undefined there; exclude them from the error metric.
        no_direct_emission = int((valid & (reference < 0)).sum())
        valid &= reference >= 0
        edge=valid&(reference>0)&(reference<1)
        transition=valid&(reference>.1)&(reference<.9)
        stats=[]
        for values in images:
            error=np.abs(values-reference)
            stats.append(dict(mae=float(error[valid].mean()),partial_mae=float(error[edge].mean()),
                              partial_pixels=int((valid&(values>0)&(values<1)).sum()),
                              transition_coverage=float(((values[transition]>.1)&(values[transition]<.9)).mean()),
                              deep_leaks=int((valid&(reference==0)&(values>.5)).sum())))
        times=[[] for _ in images]
        for order in ((0,1,2),(2,1,0),(1,0,2),(2,0,1)):
            for mode in order:
                times[mode]+=replay.timings(lambda:receiver(mode),8)['gpu_ms']
        for stat,samples in zip(stats,times):
            stat['gpu_median_ms']=float(np.median(samples))
        label='ground' if settled else 'attached'
        sheet=Image.new('RGB',(width*2,(height+32)*2),'#18212a')
        font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',19) if Path('C:/Windows/Fonts/segoeui.ttf').exists() else ImageFont.load_default()
        for index,(values,title) in enumerate(zip([images[0],images[2],images[1],reference],
                ['Hard 1024','Experimental Soft','Existing Soft control','256-ray luminous surface'])):
            x,y=width*(index%2),(height+32)*(index//2)
            image=np.where(valid,50+180*np.clip(values,0,1),23).astype(np.uint8)
            sheet.paste(Image.fromarray(image),(x,y+32))
            ImageDraw.Draw(sheet).text((x+6,y+3),f'{title}   /   {label}, tick {tick}',font=font,fill='white')
        sheet.save(output/f'{label}-{tick}.png')
        return dict(scene=label,tick=tick,renderer=ctx.info,receivers=int(valid.sum()),
                    reference='posed pg light, own casing and impact host excluded',
                    includes_host_receivers=settled or (cap/f'shadow-owners-{tick}-0.bin').exists(), surface_samples=len(points),
                    no_direct_emission=no_direct_emission, area_edge_pixels=int(edge.sum()),omitted_alpha=info['omitted_alpha_triangles'],
                    hard=stats[0],existing_soft=stats[1],candidate=stats[2])
    finally:
        if replay: replay.close()
        ctx.close()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ground','attached','projection-log','output']:
        p.add_argument('--'+key,required=True,type=Path, help='Single-grenade host-rule capture with casing geometry available' if key in ('ground','attached') else None)
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    proj=args.projection_log.read_text()
    camera=r'SHADOW SCENE created .*? camera ([-.\d ]+) forward ([-.\d ]+)'
    captured_camera=re.search(camera,proj).groups()
    clip=np.array([np.fromstring(s,sep=' ') for s in re.findall(r'GROUND clip [0-3] ([-+.\deE ]+)',proj)[:4]])
    assert clip.shape==(4,4) and np.allclose(clip[3,:3],np.fromstring(captured_camera[1],sep=' '),atol=.00001)
    rows=[]
    for cap,ground in ((args.ground,True),(args.attached,False)):
        assert re.search(camera,(cap/'stderr.log').read_text()).groups()==captured_camera
        for tick in (115,130):
            row=measure(cap,tick,ground,clip,args.output,640,360)
            print(json.dumps(row),flush=True)
            rows.append(row)
    (args.output/'comparison.json').write_text(json.dumps(rows,indent=2))


if __name__=='__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
