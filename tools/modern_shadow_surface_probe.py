"""Offline Jackal surface-emission reference and capture-information test.

Uses local posed-geometry captures. It does not alter or launch the game.
The reduced geometry contains WHOLE triangles hit by one or two cubemaps,
so it is more informative than those depth samples alone.
It is a diagnostic comparison, not an implementation of the published shader.
Only the captured opaque Jackal geometry is tested, not the complete scene.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import time

import numpy as np
from PIL import Image, ImageDraw


def load_capture(root, frame):
    raw = (root/'shadow-meshes.bin').read_bytes()
    offset, meshes = 0, {}
    while offset < len(raw):
        if len(raw) - offset < 12:
            raise ValueError('Truncated captured mesh header')
        index, nv, ni = struct.unpack_from('<3i', raw, offset); offset += 12
        if not (0 <= index < 2048 and 0 < nv < 1000000 and 0 < ni < 3000000 and ni % 3 == 0):
            raise ValueError('Invalid captured mesh dimensions')
        if offset + nv*32 + ni*2 > len(raw): raise ValueError('Truncated mesh')
        v = np.frombuffer(raw, '<f4', nv*8, offset).reshape(nv,8); offset += nv*32
        ix = np.frombuffer(raw, '<u2', ni, offset).reshape(-1,3); offset += ni*2
        if ix.max() >= nv or not np.isfinite(v).all(): raise ValueError('Invalid captured mesh')
        if index in meshes:
            raise ValueError('Duplicate captured mesh index')
        meshes[index] = v, ix
    log = (root/'stderr.log').read_text(errors='replace')
    names = {int(m)-1:s for m,s in re.findall(r'SHADOW PART model \S+ part \d+ mesh (\d+) region \d+ geometry \d+ shader (.*)',log)}
    surface_meshes = {int(m)-1 for m,s in re.findall(r'SHADOW SURFACE mesh (\d+) shader (.*)',log) if 'jackal' in s and 'shield' in s}
    if len(surface_meshes) != 1: raise ValueError(f'Expected one emitting shield mesh: {surface_meshes}')
    raw = (root/f'shadow-draws-{frame}-0.bin').read_bytes()
    if len(raw) < 20:
        raise ValueError('Truncated draw header')
    count, = struct.unpack_from('<i',raw,16)
    if not (0 < count <= 256 and len(raw) == 20+count*4144): raise ValueError('Invalid draw capture')
    poses, triangles, labels = [], [], []
    def pose_mesh(mesh, pose):
        v, ix = meshes[mesh]
        p = np.c_[v[:,:3],np.ones(len(v))]
        indices = v[:,3:5]
        if not ((indices >= 0).all() and (indices < 64).all()
                and (indices == np.floor(indices)).all()):
            raise ValueError('Invalid node index')
        if not ((v[:,5] >= 0).all() and (v[:,5] <= 1).all()):
            raise ValueError('Invalid node weight')
        transformed = sum(np.einsum('nij,nj->ni',pose[v[:,k].astype(int)],p)[:,:3]*w
                          for k,w in ((3,v[:,5,None]),(4,1-v[:,5,None])))
        return transformed[ix]
    for row in range(count):
        start = 20+row*4144
        mesh, nodes, views = struct.unpack_from('<3i',raw,start)
        pose = np.frombuffer(raw,'<f4',1024,start+12).reshape(64,4,4).transpose(0,2,1)
        if not 0 < nodes <= 64 or not np.isfinite(pose).all():
            raise ValueError('Invalid node count or pose')
        if 'jackal' in names.get(mesh,''):
            triangles.append(pose_mesh(mesh,pose)); poses.append(pose); labels.append(names[mesh])
    if not triangles: raise ValueError('No Jackal caster triangles')
    if any(not np.array_equal(p,poses[0]) for p in poses): raise ValueError('Jackal meshes disagree on pose')
    # Re-pose the immutable emitting mesh with this EXACT draw's body matrices.
    # The old surface dump was at another frame and must not supply its pose.
    surface = pose_mesh(next(iter(surface_meshes)),poses[0])
    area = np.linalg.norm(np.cross(surface[:,1]-surface[:,0],surface[:,2]-surface[:,0]),axis=1)
    good = area > 1.e-12; surface,area=surface[good],area[good]
    if not len(surface):
        raise ValueError('Emitting surface has no area')
    center = np.sum(surface.mean(1)*area[:,None],axis=0)/area.sum()
    line = next(line for line in log.splitlines() if 'SHADOW SCENE wall' in line)
    wall = np.array([float(x) for x in re.findall(r'-?\d+\.\d+',line)])
    if wall.shape != (6,) or not np.isfinite(wall).all() or np.linalg.norm(wall[3:]) < 1.e-8:
        raise ValueError('Invalid captured wall plane')
    provenance = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                  (root/'shadow-meshes.bin',root/f'shadow-draws-{frame}-0.bin',root/'stderr.log')}
    return np.concatenate(triangles)-center, surface-center, area, wall, center, labels, provenance


def sequence(count, base):
    values = np.arange(1,count+1); result = np.zeros(count); scale=1/base
    while values.any():
        result += (values % base)*scale; values //= base; scale /= base
    return result


def sampled_surface(surface, area, count):
    chosen = np.searchsorted(np.cumsum(area)/area.sum(),(np.arange(count)+.5)/count)
    root = np.sqrt(sequence(count,2)); other = sequence(count,3)
    weights = np.c_[1-root,root*(1-other),root*other]
    return np.sum(surface[chosen]*weights[:,:,None],axis=1)


def query(exe, out, name, triangles, receivers, sources, nearest=False):
    incoming, outgoing = out/(name+'.bin'),out/(name+'.raw')
    receivers = np.asarray(receivers).reshape(-1,3)
    with incoming.open('wb') as f:
        f.write(struct.pack('<3i',len(triangles),len(receivers),len(sources)))
        for data in (triangles,receivers,sources): f.write(np.asarray(data,dtype='<f4').tobytes())
    start = time.perf_counter()
    subprocess.run([str(exe.resolve()), 'nearest' if nearest else 'visibility',
                    str(incoming), str(outgoing)], check=True, timeout=900)
    result = np.fromfile(outgoing,dtype='<i4' if nearest else '<f4')
    if len(result) != len(receivers): raise ValueError('Truncated oracle result')
    incoming.unlink();outgoing.unlink()  # Only this invocation's generated files.
    return result, time.perf_counter()-start


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--oracle',type=Path,required=True)
    parser.add_argument('--frame',type=int,choices=[120,180],default=180)
    parser.add_argument('--gap',type=float,default=.7)
    parser.add_argument('--view-origins',choices=['center','diameter'],default='center')
    parser.add_argument('--receiver',choices=['wall','side-a','side-b'],default='wall')
    parser.add_argument('--samples',type=int,choices=[256,512,1024],default=512)
    parser.add_argument('--cube-size',type=int,choices=[256,512,1024],default=1024)
    args=parser.parse_args()
    if not np.isfinite(args.gap) or args.gap <= 0:
        parser.error('--gap must be finite and positive')
    args.output.mkdir(parents=True,exist_ok=False)
    triangles,surface,area,wall,center,labels,provenance=load_capture(args.capture,args.frame)
    sources=sampled_surface(surface,area,args.samples)
    size=args.cube_size
    s,t=np.meshgrid((np.arange(size)+.5)*2/size-1,(np.arange(size)+.5)*2/size-1)
    one=np.ones_like(s)
    origins=np.zeros((1,3))
    if args.view_origins=='diameter':
        vertices=np.unique(surface.reshape(-1,3),axis=0)
        pairs=np.sum((vertices[:,None]-vertices[None,:])**2,axis=-1)
        a,b=np.unravel_index(pairs.argmax(),pairs.shape)
        origins=vertices[[a,b]]
    visible=set();timing=0
    for origin_index,origin in enumerate(origins):
        for face,d in enumerate(((one,-t,-s),(-one,-t,s),(s,one,t),(s,-one,-t),(s,-t,one),(-s,-t,-one))):
            ids,elapsed=query(args.oracle,args.output,f'view-{origin_index}-face-{face}',triangles,np.stack(d,axis=-1)*100+origin,origin[None],True)
            visible.update(int(x) for x in np.unique(ids) if x>=0);timing+=elapsed
    reduced=triangles[sorted(visible)]
    if not len(reduced): raise ValueError('Captures see no geometry')
    width,height=256,192
    x,y=np.meshgrid((np.arange(width)+.5)*4/width-2, 2.6-(np.arange(height)+.5)*2.6/height)
    normal=wall[3:];normal/=np.linalg.norm(normal);right=np.cross(normal,(0,0,1))
    receivers=wall[:3]-normal*args.gap-center+right*x[:,:,None]+np.array((0,0,1))*y[:,:,None]
    if args.receiver != 'wall':
        _,_,basis=np.linalg.svd(surface.reshape(-1,3),full_matrices=False)
        direction=basis[2]
        if np.dot(direction,normal)<0: direction=-direction
        vertical=basis[0]
        if vertical[2]<0: vertical=-vertical
        horizontal=np.cross(direction,vertical)
        sx,sy=np.meshgrid((np.arange(width)+.5)*2.4/width-1.2,1.2-(np.arange(height)+.5)*2.4/height)
        receivers=direction*(.8 if args.receiver=='side-a' else -.8)+horizontal*sx[:,:,None]+vertical*sy[:,:,None]
    full,full_time=query(args.oracle,args.output,'full',triangles,receivers,sources)
    optimistic,reduced_time=query(args.oracle,args.output,'reduced',reduced,receivers,sources)
    full=full.reshape(height,width);optimistic=optimistic.reshape(height,width)
    error=optimistic-full
    assert error.min()>=-1.e-6, 'Removing triangles cannot add shadow'
    image=Image.new('RGB',(width*3,height+36),'#171b24');draw=ImageDraw.Draw(image)
    for index,(name,data) in enumerate((('Full geometry / surface emission',full),(f'{len(origins)} capture(s): whole triangles',optimistic),('Lost shadow (red)',error))):
        pixels=np.uint8(np.clip(data,0,1)*255)
        panel=Image.fromarray(pixels).convert('RGB') if index<2 else Image.fromarray(np.stack((pixels,np.zeros_like(pixels),np.zeros_like(pixels)),axis=-1))
        image.paste(panel,(index*width,36));draw.text((index*width+5,8),name,fill='white')
    image.resize((1536,456)).save(args.output/'comparison.png')
    np.savez_compressed(args.output/'reference.npz',triangles=triangles,surface=surface,sources=sources,receivers=receivers,
                        center=center,full=full,reconstructed=optimistic,visible_triangles=np.array(sorted(visible)))
    relevant=full<.95
    report=dict(capture=str(args.capture),frame=args.frame,samples=args.samples,cube_size=size,gap=args.gap,
                source='uniform, two-sided shield mesh; visibility only; no texture alpha/radiometric weighting',
                view_origins=origins.tolist(), view_method=args.view_origins, receiver=args.receiver,
                same_pose=True,triangles=len(triangles),capture_visible_triangles=len(reduced),labels=labels,provenance=provenance,
                tools={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in (Path(__file__), args.oracle)},
                scope='opaque Jackal geometry only; virtual receivers; no game radiometry, texture alpha or GPU implementation',
                mean_visibility_error=float(error.mean()),max_visibility_error=float(error.max()),
                fraction_error_over_10_percent=float(np.mean(error>.1)),
                shadow_region_mean_error=float(error[relevant].mean()) if relevant.any() else 0,
                reference_shadow_fraction=float(relevant.mean()),oracle_seconds=dict(depth=timing,full=full_time,reduced=reduced_time))
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2),flush=True)


if __name__=='__main__': main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
