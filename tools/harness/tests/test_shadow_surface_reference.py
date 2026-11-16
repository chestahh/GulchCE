"""Validate the offline geometry oracle before using its rejection evidence."""
import struct
import subprocess

import numpy as np
from tools.harness import build, read


def execute(tmp_path, triangles, receivers, sources, mode):
    exe = build('shadow_surface_reference', (('under_test.inc', read('tools/fixtures/shadow_surface_reference.c')),))
    incoming, outgoing = tmp_path/'input.bin', tmp_path/'output.bin'
    data = struct.pack('<3i', len(triangles),len(receivers),len(sources))
    data += b''.join(np.asarray(v,dtype='<f4').tobytes() for v in (triangles,receivers,sources))
    incoming.write_bytes(data)
    result=subprocess.run([str(exe),mode,str(incoming),str(outgoing)],capture_output=True)
    assert result.returncode == 0, result.stderr
    return np.fromfile(outgoing,dtype='<i4' if mode=='nearest' else '<f4')


def test_nearest_keeps_input_identity_after_tree_sort(tmp_path):
    order=[9,2,13,4,15,6,11,8,1,10,7,12,3,14,5,16]
    triangles=[[[x,-2,z],[x+2,-2,z],[x,2,z]] for x,z in [(0,z) for z in order]]
    ids=execute(tmp_path,triangles,[[.2,0,20],[-30,0,20]],[[.2,0,0]],'nearest')
    assert list(ids)==[8,-1]


def test_area_visibility_integrates_sources_and_keeps_owner(tmp_path):
    triangles=[[[-.4,-2,1],[.4,-2,1],[.4,2,1]],[[-.4,-2,1],[.4,2,1],[-.4,2,1]]]
    sources=[[-2,0,0],[-.2,0,0],[.2,0,0],[2,0,0]]
    result=execute(tmp_path,triangles,[[0,0,2],[5,0,2]],sources,'visibility')
    assert np.allclose(result,[.5,1])


def test_near_occluder_is_not_lost_on_long_capture_rays(tmp_path):
    triangles=[[[-1,-1,.0001],[1,-1,.0001],[0,1,.0001]]]
    ids=execute(tmp_path,triangles,[[0,0,1],[0,0,100]],[[0,0,0]],'nearest')
    assert list(ids)==[0,0]


def test_only_geometry_inside_finite_segment_blocks(tmp_path):
    triangles=[[[-1,-1,2],[1,-1,2],[0,1,2]]]
    ids=execute(tmp_path,triangles,[[0,0,1],[0,0,-1],[0,0,0],[0,0,3]],
                [[0,0,0]],'nearest')
    assert list(ids)==[-1,-1,-1,0]


def test_reference_samples_follow_surface_area_and_pose():
    from tools.modern_shadow_surface_probe import sampled_surface

    surface=np.array([[[0,0,0],[1,0,0],[0,1,0]],
                      [[10,0,0],[12,0,0],[10,2,0]]],dtype=float)
    points=sampled_surface(surface,np.array([1.,4.]),1000)
    small=points[:,0]<5
    assert small.sum()==200
    assert (points[small,:2]>=0).all()
    assert (points[small,:2].sum(axis=1)<=1).all()
    large=points[~small,:2]-[10,0]
    assert (large>=0).all() and (large.sum(axis=1)<=2).all()
    shift=np.array([.3,-1,9])
    assert np.allclose(sampled_surface(surface+shift,np.array([1.,4.]),1000),points+shift)


def test_random_rays_match_independent_vectorized_intersection(tmp_path):
    rng=np.random.default_rng(417)
    triangles=rng.uniform(-1,1,(40,3,3));triangles[:,:,2]+=2
    receivers=rng.uniform(-3,3,(200,3));receivers[:,2]=5
    ids=execute(tmp_path,triangles,receivers,[[0,0,0]],'nearest')
    a=triangles[:,1]-triangles[:,0];b=triangles[:,2]-triangles[:,0]
    for point,index in zip(receivers,ids):
        h=np.cross(point,b);det=np.sum(a*h,axis=1)
        safe=np.abs(det)>1e-12
        inv=np.divide(1,det,out=np.zeros_like(det),where=safe)
        offset=-triangles[:,0];u=np.sum(offset*h,axis=1)*inv
        q=np.cross(offset,a);v=(q@point)*inv;t=np.sum(b*q,axis=1)*inv
        minimum=1e-6/np.linalg.norm(point)
        valid=safe&(u>=0)&(v>=0)&(u+v<=1)&(t>minimum)&(t<1-minimum)
        expected=int(np.where(valid,t,np.inf).argmin()) if valid.any() else -1
        assert index==expected


def test_rejects_truncated_and_nonfinite_inputs(tmp_path):
    exe=build('shadow_surface_reference', (('under_test.inc', read('tools/fixtures/shadow_surface_reference.c')),))
    incoming,outgoing=tmp_path/'bad.bin',tmp_path/'bad-output.bin'
    for data in (struct.pack('<3i',1,1,1),struct.pack('<3i',-1,1,1),
                 struct.pack('<3i',1,1,1)+np.full(15,np.nan,dtype='<f4').tobytes()):
        incoming.write_bytes(data)
        result=subprocess.run([str(exe),'visibility',str(incoming),str(outgoing)],capture_output=True)
        assert result.returncode!=0 and not outgoing.exists()
