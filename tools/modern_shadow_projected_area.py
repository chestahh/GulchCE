"""Projected-source coverage experiment; no engine integration.

Integrates the union of projected blocker intervals along strips on each
actual emitting triangle. The remaining quadrature is across strips, not
hundreds of independent point-source ray tests at every receiver.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image, ImageDraw

if __package__:
    from .modern_shadow_geometry_gpu import Context, build_tree
else:
    from modern_shadow_geometry_gpu import Context, build_tree

SHADER = Path(__file__).parent / 'fixtures/shadow_projected_area.comp'


def emitting_strips(surface, count):
    """Equal-area quadrature in each facet, retaining curvature and area weights."""
    surface = np.asarray(surface, dtype=np.float64)
    if surface.ndim != 3 or surface.shape[1:] != (3, 3) or not np.isfinite(surface).all():
        raise ValueError('Expected finite emitting triangles')
    if not 1 <= count <= 32:
        raise ValueError('Expected 1..32 strips per emitting triangle')
    area = np.linalg.norm(np.cross(surface[:, 1]-surface[:, 0], surface[:, 2]-surface[:, 0]), axis=1) / 2
    valid = area > 1.e-12
    surface, area = surface[valid], area[valid]
    if not len(surface):
        raise ValueError('Emitting surface has no area')
    lines = np.zeros((len(surface)*count, 2, 4), dtype=np.float32)
    root = np.sqrt((np.arange(count)+.5)/count)
    lines[:, 0, :3] = (surface[:, None, 0] + root[None, :, None]*(surface[:, None, 1]-surface[:, None, 0])).reshape(-1, 3)
    lines[:, 1, :3] = (surface[:, None, 0] + root[None, :, None]*(surface[:, None, 2]-surface[:, None, 0])).reshape(-1, 3)
    lines[:, 0, 3] = np.repeat(area / (area.sum()*count), count)
    return lines


def projected_intervals(receiver, start, end, triangles):
    """Project opaque triangles onto a finite emitting line using half-spaces.

    Three planes through the receiver bound each triangle's shadow cone. A
    fourth plane requires the emitting point to lie beyond the occluder.
    Intersecting those linear inequalities clips [0,1] to the blocked interval.
    """
    receiver, start, end, triangles = [np.asarray(x, dtype=np.float64) for x in (receiver, start, end, triangles)]
    relative = triangles - receiver
    a, b, c = relative[:, 0], relative[:, 1], relative[:, 2]
    determinant = np.einsum('ij,ij->i', a, np.cross(b, c))
    normals = np.stack((np.cross(b, c), np.cross(c, a), np.cross(a, b)), axis=1)
    normals *= np.sign(determinant)[:, None, None]
    lengths = np.linalg.norm(normals, axis=2)
    normals /= np.where(lengths > 0, lengths, 1)[:, :, None]
    plane = np.cross(b-a, c-a)
    plane_length = np.linalg.norm(plane, axis=1)
    plane /= np.where(plane_length > 0, plane_length, 1)[:, None]
    receiver_side = np.einsum('ij,ij->i', plane, receiver-triangles[:, 0])
    plane *= -np.sign(receiver_side)[:, None]
    values = np.concatenate((np.einsum('nij,j->ni', normals, start-receiver),
                             (np.einsum('ij,ij->i', plane, start-triangles[:, 0])-1.e-6)[:, None]), axis=1)
    slopes = np.concatenate((np.einsum('nij,j->ni', normals, end-start),
                             (plane @ (end-start))[:, None]), axis=1)
    low, high = np.zeros(len(triangles)), np.ones(len(triangles))
    valid = (plane_length > 1.e-12) & (np.abs(receiver_side) > 1.e-6) & (lengths.min(axis=1) > 1.e-12)
    for axis in range(4):
        value, slope = values[:, axis], slopes[:, axis]
        positive, negative = slope > 1.e-12, slope < -1.e-12
        cut = np.divide(-value, slope, out=np.zeros_like(value), where=positive | negative)
        low = np.where(positive, np.maximum(low, cut), low)
        high = np.where(negative, np.minimum(high, cut), high)
        valid &= positive | negative | (value >= 0)
    valid &= high > low
    return np.stack((low[valid], high[valid]), axis=1)


def interval_union_length(intervals):
    if not len(intervals):
        return 0.0
    ordered = sorted(intervals, key=lambda pair: pair[0])
    low, high = ordered[0]
    total = 0.0
    for a, b in ordered[1:]:
        if a > high:
            total += high-low
            low, high = a, b
        else:
            high = max(high, b)
    return float(total+high-low)


def cpu_visibility(receiver, lines, triangles):
    return sum(float(line[0, 3])*(1-interval_union_length(
        projected_intervals(receiver, line[0, :3], line[1, :3], triangles))) for line in lines)


def gpu_visibility(context, triangles, lines, receivers, copies=1, repeats=3, capacity=32):
    begin = time.perf_counter()
    tree, mesh = build_tree(triangles)
    old = np.frombuffer(mesh, '<f4').reshape(-1,3,4)
    packed = np.zeros((len(old),4,4), dtype=np.float32)
    packed[:,:3] = old
    normal = np.cross(old[:,1,:3], old[:,2,:3])
    magnitude = np.linalg.norm(normal,axis=1)
    normal = np.divide(normal,magnitude[:,None],out=np.zeros_like(normal),where=magnitude[:,None]>1.e-12)
    packed[:,3,:3] = normal
    packed[:,3,3] = np.sum(normal*old[:,0,:3],axis=1)
    receivers = np.asarray(receivers,dtype=np.float32).reshape(-1,3)
    points = np.zeros((len(receivers),4),dtype=np.float32)
    points[:,:3] = receivers
    preparation_ms = (time.perf_counter()-begin)*1000
    gl = context.gl
    u, i = C.c_uint, C.c_int
    program = context.compile(SHADER.read_text().replace('#version 430 core',
                              f'#version 430 core\n#define INTERVAL_CAPACITY {capacity}'))
    gl('UseProgram',None,u)(program)
    names = ('receiver_count','strip_count','first_receiver','output_offset')
    locations = {name:gl('GetUniformLocation',i,u,C.c_char_p)(program,name.encode()) for name in names}
    if min(locations.values())<0:
        raise RuntimeError('Missing projected-coverage uniform')
    uniform = gl('Uniform1i',None,i,i)
    dispatch = gl('DispatchCompute',None,u,u,u)
    barrier = gl('MemoryBarrier',None,u)
    payloads = (tree,packed.tobytes(),lines.tobytes(),points.tobytes(),
                bytes(len(points)*copies*4),bytes(len(points)*copies*4))
    objects = [context.buffer(binding,payload) for binding,payload in enumerate(payloads)]
    uniform(locations['receiver_count'],len(points))
    uniform(locations['strip_count'],len(lines))
    queries = (u*2)()
    gl('GenQueries',None,i,C.POINTER(u))(2,queries)
    context.queries.extend(queries)
    stamp = gl('QueryCounter',None,u,u)
    query = gl('GetQueryObjectui64v',None,u,u,C.POINTER(C.c_uint64))
    timings = []
    for iteration in range(repeats+2):
        stamp(queries[0],0x8E28)
        for copy in range(copies):
            uniform(locations['output_offset'],copy*len(points))
            for first in range(0,len(points),256):
                uniform(locations['first_receiver'],first)
                dispatch(min(256,len(points)-first),1,1)
        barrier(0x200)
        stamp(queries[1],0x8E28)
        start,end = C.c_uint64(),C.c_uint64()
        query(queries[0],0x8866,C.byref(start))
        query(queries[1],0x8866,C.byref(end))
        if iteration>=2:
            timings.append((end.value-start.value)/1.e6)
    output = np.empty((copies,len(points)),dtype=np.float32)
    gl('BindBuffer',None,u,u)(0x90D2,objects[4])
    gl('GetBufferSubData',None,u,C.c_ssize_t,C.c_ssize_t,C.c_void_p)(0x90D2,0,output.nbytes,output.ctypes.data)
    peaks=np.empty(output.shape,dtype=np.uint32)
    gl('BindBuffer',None,u,u)(0x90D2,objects[5])
    gl('GetBufferSubData',None,u,C.c_ssize_t,C.c_ssize_t,C.c_void_p)(0x90D2,0,peaks.nbytes,peaks.ctypes.data)
    context.check()
    if output.min() < -10000:
        raise OverflowError(f'Projected interval storage exhausted (capacity {capacity}); no incomplete result accepted')
    if not np.isfinite(output).all() or output.min() < -1.e-5 or output.max() > 1.00001:
        raise RuntimeError('Invalid projected coverage')
    return output, dict(gpu_ms=timings,gpu_median_ms=float(np.median(timings)),
                        geometry_bytes=len(tree)+packed.nbytes,total_buffer_bytes=sum(map(len,payloads)),
                        preparation_python_ms=preparation_ms,interval_capacity=capacity,peak_intervals=int(peaks.max()))


def verify_analytic(context):
    def rectangle(x0,x1,z):
        return np.array([[[x0,-2,z],[x1,-2,z],[x1,2,z]],[[x0,-2,z],[x1,2,z],[x0,2,z]]],dtype=float)
    line=np.array([[[-1,0,0,1],[1,0,0,0]]],dtype=np.float32)
    for side in (-1,1):
        mesh=np.concatenate((rectangle(-.4,.1,side),rectangle(-.1,.4,side),rectangle(-.4,.1,side)))
        result,_=gpu_visibility(context,mesh,line,[[0,0,2*side],[0,0,0]],repeats=1)
        if not np.allclose(result,[[.2,1]],atol=2.e-6):
            raise RuntimeError(f'GPU overlap/both-side coverage failed: {result}')
    # Exercise BVH pruning and non-axis-aligned overlapping projections.
    rng=np.random.default_rng(902)
    mesh=rng.uniform(-.8,.8,(32,3,3));mesh[:,:,2]+=1
    lines=np.zeros((8,2,4),dtype=np.float32)
    lines[:,:,:3]=rng.uniform(-1,1,(8,2,3));lines[:,:,2]=0
    lines[:,0,3]=1/8
    points=rng.uniform(-2,2,(12,3));points[:,2]=2.5
    expected=np.array([cpu_visibility(p,lines,mesh) for p in points])
    result,_=gpu_visibility(context,mesh,lines,points,repeats=1)
    if not np.allclose(result[0],expected,atol=2.e-5,rtol=0):
        raise RuntimeError('GPU randomized projection/beam traversal failed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--strips',type=int,choices=(1,2,4,8,16,32),default=2)
    parser.add_argument('--stride',type=int,choices=(1,2,4,8,16),default=1)
    parser.add_argument('--copies',type=int,choices=(1,8),default=1)
    parser.add_argument('--repeats',type=int,choices=range(1,11),default=3)
    parser.add_argument('--capacity',type=int,choices=(8,16,32),default=32,
                        help='diagnostic storage capacity; exhausted storage fails the run')
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    with np.load(args.reference,allow_pickle=False) as archive:
        data={key:archive[key] for key in ('surface','triangles','receivers','full')}
    if (data['full'].ndim!=2 or not 0<data['full'].size<=1048576
            or data['receivers'].shape!=(*data['full'].shape,3)
            or not np.isfinite(data['receivers']).all() or not np.isfinite(data['full']).all()
            or data['full'].min()<0 or data['full'].max()>1):
        raise ValueError('Invalid captured receiver/reference grid')
    lines=emitting_strips(data['surface'],args.strips)
    receivers=data['receivers'][::args.stride,::args.stride]
    reference=data['full'][::args.stride,::args.stride]
    context=Context()
    try:
        verify_analytic(context)
        output,report=gpu_visibility(context,data['triangles'],lines,receivers,args.copies,args.repeats,args.capacity)
        output=output.reshape(args.copies,*reference.shape)
        error=np.abs(output-reference)
        report.update(context.info)
        report.update(strips_per_facet=args.strips,total_strips=len(lines),copies=args.copies,
                      receiver_pixels=reference.size,receiver_stride=args.stride,
                      mean_error=float(error.mean()),max_error=float(error.max()),
                      p99_error=float(np.quantile(error,.99)),fraction_error_over_05=float(np.mean(error>.05)),
                      scope='Opaque local geometry; area quadrature on actual curved facets; no runtime integration',
                      analytic_gpu_checks_passed=True,
                      provenance={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in
                                  (args.reference,SHADER,Path(__file__),Path(__file__).with_name('modern_shadow_geometry_gpu.py'))})
        np.savez_compressed(args.output/'result.npz',visibility=output)
        w,h=reference.shape[1],reference.shape[0]
        image=Image.new('RGB',(w*3,h+30),'#171b24')
        draw=ImageDraw.Draw(image)
        for index,(label,values) in enumerate((('Ray reference',reference),('Projected-area coverage',output[0]),('Absolute error (red)',error[0]))):
            values=np.uint8(np.clip(values,0,1)*255)
            panel=Image.fromarray(values).convert('RGB') if index<2 else Image.fromarray(np.stack((values,np.zeros_like(values),np.zeros_like(values)),axis=-1))
            image.paste(panel,(index*w,30));draw.text((index*w+3,7),label,fill='white')
        image.resize((max(768,w*6),max(222,(h+30)*2))).save(args.output/'comparison.png')
        (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2),flush=True)
    except Exception as error:
        failure=dict(kind=type(error).__name__,message=str(error),settings=vars(args),device=context.info)
        (args.output/'failure.json').write_text(json.dumps(failure,default=str,indent=2)+'\n')
        raise
    finally:
        context.close()


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
