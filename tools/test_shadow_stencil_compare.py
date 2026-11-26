"""Diagnostic correctness, not a production renderer regression suite."""
import os
import numpy as np
import pytest
from tools.shadow_stencil_compare import volume, camera_matrix, Replay, Context, ray_reference, triangle_distance, captured


def rectangle(z, size):
    return np.array([[[-size,-size,z],[size,-size,z],[size,size,z]],
                     [[-size,-size,z],[size,size,z],[-size,size,z]]], np.float32)


def test_open_mesh_gets_closed_volume_without_diagonal_side():
    vertices,stats=volume(rectangle(0,1),[0,0,2])
    assert stats['retained_side_edges']==4
    assert stats['volume_triangles']==12
    assert set(vertices[:,3])=={0,1}
    # Each directed edge in homogeneous coordinates has a reverse partner.
    edges={}
    for triangle in vertices.reshape(-1,3,4):
        for a,b in zip(triangle,np.roll(triangle,-1,axis=0)):
            key=(tuple(a),tuple(b));edges[key]=edges.get(key,0)+1
    assert all(count==edges.get((b,a),0) for (a,b),count in edges.items())


def test_triangle_winding_does_not_change_two_sided_shadow():
    t=rectangle(0,1)
    a,_=volume(t,[0,0,2]);b,_=volume(t[:,::-1],[0,0,2])
    def faces(v):return sorted(tuple(sorted(map(tuple,t))) for t in v.reshape(-1,3,4))
    assert faces(a)==faces(b)


def test_invalid_geometry_rejected():
    with pytest.raises(ValueError):volume(np.full((1,3,3),np.nan),[0,0,0])
    with pytest.raises(ValueError):volume([], [0,0,0])


@pytest.mark.skipif(os.name!='nt' or not os.environ.get('SHADOW_COMPARE_GPU_TEST'),reason='Explicit hidden Windows GL check')
@pytest.mark.parametrize('eye',[[0.,-4.,3.],[0.,0.,.5]])
def test_stencil_matches_independent_rays_including_camera_inside(eye):
    t=np.concatenate([rectangle(0,2),rectangle(1,.3)])
    light=np.array([0,0,2],np.float32)
    vertices,_=volume(t,light)
    c=Context();r=None
    try:
        r=Replay(c,160,120)
        mesh=r.upload(np.c_[t.reshape(-1,3),np.ones(len(t)*3)])
        extrusion=r.upload(vertices)
        matrix=camera_matrix(np.array(eye),np.zeros(3),4/3)
        r.begin();r.draw(r.positions,mesh,matrix);positions=r.read()
        valid=positions[:,:,3]>.5
        receivers=positions[:,:,:3][valid]
        delta=receivers-light
        receivers-=delta/np.linalg.norm(delta,axis=1)[:,None]*.0005
        truth,_=ray_reference(c,dict(triangles=t,sources=light[None],receivers=receivers,full=np.zeros((1,len(receivers)))),1,1)
        r.stencil(mesh,extrusion,matrix)
        mask=r.read()[:,:,0][valid]>.5
        mismatch=mask!=(truth.reshape(-1)>.5)
        points=positions[:,:,:3][valid]
        # The card at z=1 projects to a +/-0.6 square on z=0. GL snaps
        # rasterized edges to subpixel coordinates; rays do not. Permit only
        # that analytic boundary, never holes along the shared diagonal.
        boundary=(abs(points[:,2])<1e-6) & (abs(np.max(abs(points[:,:2]),axis=1)-.6)<.0002)
        assert not np.any(mismatch & ~boundary)
        assert np.any(~mask)
    finally:
        if r:r.close()
        c.close()


def test_source_distance_handles_face_edge_and_degenerate_triangle():
    triangles=np.array([[[0,0,0],[1,0,0],[0,1,0]],
                        [[0,0,0],[0,0,0],[0,0,0]]],dtype=float)
    assert triangle_distance(triangles,np.array([.2,.2,1.])) == pytest.approx([1.,np.sqrt(1.08)])
    assert triangle_distance(triangles,np.array([2.,0.,0.])) == pytest.approx([1.,2.])


def test_truncated_mesh_capture_fails_closed(tmp_path):
    (tmp_path/'stderr.log').write_text('')
    (tmp_path/'shadow-meshes.bin').write_bytes(b'bad')
    with pytest.raises(ValueError,match='Truncated mesh header'):
        captured(tmp_path,180,0)


@pytest.mark.skipif(os.name!='nt' or not os.environ.get('SHADOW_COMPARE_GPU_TEST'),reason='Explicit hidden Windows GL check')
@pytest.mark.parametrize('height',[.5,2.5])
def test_receiver_boundary_from_either_camera_side(height):
    # Visibility is independent of which side the camera views. The BRDF
    # decides whether a back-facing receiver contributes light separately.
    t=rectangle(1,.3);light=np.array([0,0,2],np.float32)
    vertices,_=volume(t,light)
    c=Context();r=None
    try:
        r=Replay(c,160,120)
        mesh=r.upload(np.c_[t.reshape(-1,3),np.ones(len(t)*3)])
        extrusion=r.upload(vertices)
        matrix=camera_matrix(np.array([0.,0.,height]),np.array([0.,0.,1.]),4/3)
        r.begin();r.draw(r.positions,mesh,matrix);valid=r.read()[:,:,3]>.5
        r.stencil(mesh,extrusion,matrix)
        assert valid.any() and np.all(r.read()[:,:,0][valid]>.5)
    finally:
        if r:r.close()
        c.close()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
