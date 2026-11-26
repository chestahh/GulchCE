import numpy as np
import pytest

from tools.modern_shadow_projected_area import emitting_strips, projected_intervals, interval_union_length, cpu_visibility


def rectangle(x0, x1, z):
    return np.array([[[x0,-2,z],[x1,-2,z],[x1,2,z]],
                     [[x0,-2,z],[x1,2,z],[x0,2,z]]],dtype=float)


@pytest.mark.parametrize('side', [-1, 1])
def test_half_covered_line_both_sides(side):
    intervals = projected_intervals([0,0,side*2], [-1,0,0], [1,0,0], rectangle(-.25,.25,side))
    assert interval_union_length(intervals) == pytest.approx(.5)


def test_overlapping_and_duplicate_blockers_count_once():
    mesh=np.concatenate([rectangle(-.4,.1,1),rectangle(-.1,.4,1),rectangle(-.4,.1,1)])
    intervals=projected_intervals([0,0,2],[-1,0,0],[1,0,0],mesh)
    assert interval_union_length(intervals)==pytest.approx(.8)


@pytest.mark.parametrize('z', [-1, 0, 2, 3])
def test_occluders_outside_open_receiver_source_segment(z):
    assert len(projected_intervals([0,0,2],[-1,0,0],[1,0,0],rectangle(-2,2,z)))==0


def test_triangle_crossing_emitter_plane_is_clipped():
    mesh=np.array([[[-.5,-1,-.5],[.5,-1,.5],[0,1,.5]]])
    intervals=projected_intervals([0,0,2],[-1,0,0],[1,0,0],mesh)
    t=np.linspace(.00005,.99995,10000)
    points=np.c_[2*t-1,np.zeros_like(t),np.zeros_like(t)]
    # Independent finite-ray intersections with the input triangle.
    origin=np.array([0,0,2]);direction=points-origin
    e1,e2=mesh[0,1]-mesh[0,0],mesh[0,2]-mesh[0,0]
    h=np.cross(direction,e2);det=h@e1;offset=origin-mesh[0,0]
    u=h@offset/det;q=np.cross(offset,e1);v=direction@q/det;distance=np.dot(e2,q)/det
    hits=(u>=0)&(v>=0)&(u+v<=1)&(distance>0)&(distance<1)
    assert interval_union_length(intervals)==pytest.approx(hits.mean(),abs=.0002)


def test_curved_facets_retain_pose_and_total_area():
    surface=np.array([[[0,0,0],[1,0,0],[0,1,0]],[[0,0,0],[0,0,2],[1,0,0]]],dtype=float)
    lines=emitting_strips(surface,4)
    assert lines.shape==(8,2,4)
    assert lines[:,0,3].sum()==pytest.approx(1)
    assert lines[:4,0,3].sum()==pytest.approx(1/3)
    assert (lines[:4,:,:3][:,:,2]==0).all()
    assert (lines[4:,:,:3][:,:,1]==0).all()
    shifted=emitting_strips(surface+[2,3,4],4)
    assert np.allclose(shifted[:,:,:3],lines[:,:,:3]+[2,3,4])


def test_partial_area_known_rectangle_answer():
    surface=np.array([[[-1,-1,0],[1,-1,0],[1,1,0]],[[-1,-1,0],[1,1,0],[-1,1,0]]],dtype=float)
    result=cpu_visibility([0,0,2],emitting_strips(surface,32),rectangle(-.25,.25,1))
    assert result==pytest.approx(.5,abs=.01)


def test_disjoint_intervals_do_not_fill_gaps():
    assert interval_union_length([(0,.1),(.3,.4),(.7,1),(.35,.45)])==pytest.approx(.55)


def test_projection_winding_independent():
    mesh=rectangle(-.25,.25,1)
    a=projected_intervals([0,0,2],[-1,0,0],[1,0,0],mesh)
    b=projected_intervals([0,0,2],[-1,0,0],[1,0,0],mesh[:,::-1])
    assert interval_union_length(a)==pytest.approx(interval_union_length(b))


@pytest.mark.parametrize('surface,count', [(np.zeros((1,3,3)),2),(np.ones((1,2,3)),2),
                                          (np.full((1,3,3),np.nan),2),(np.ones((1,3,3)),0)])
def test_invalid_emitter_rejected(surface,count):
    with pytest.raises(ValueError):
        emitting_strips(surface,count)


def test_projected_union_matches_independent_dense_finite_rays():
    rng=np.random.default_rng(19)
    mesh=rng.uniform(-.8,.8,(30,3,3));mesh[:,:,2]+=1
    receiver=np.array([.1,.2,2.5])
    start,end=np.array([-1.,-.2,0]),np.array([1.,.2,0])
    samples=(np.arange(20000)+.5)/20000
    direction=start+samples[:,None]*(end-start)-receiver
    blocked=np.zeros(len(samples),dtype=bool)
    for triangle in mesh:
        e1,e2=triangle[1]-triangle[0],triangle[2]-triangle[0]
        h=np.cross(direction,e2);det=h@e1
        inv=np.divide(1,det,out=np.zeros_like(det),where=np.abs(det)>1e-12)
        offset=receiver-triangle[0];u=(h@offset)*inv
        q=np.cross(offset,e1);v=(direction@q)*inv;t=np.dot(e2,q)*inv
        blocked|=(u>=0)&(v>=0)&(u+v<=1)&(t>1e-6)&(t<1-1e-6)
    projected=interval_union_length(projected_intervals(receiver,start,end,mesh))
    assert projected==pytest.approx(blocked.mean(),abs=.0015)
    reversed_line=interval_union_length(projected_intervals(receiver,end,start,mesh))
    assert projected==pytest.approx(reversed_line)

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
