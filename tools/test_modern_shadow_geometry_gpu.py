"""Asset-free checks of the real diagnostic BVH packer; GPU checks are separate."""
import numpy as np
import pytest

from tools.modern_shadow_geometry_gpu import build_tree, vectors


def unpack(triangles):
    tree, mesh = build_tree(triangles)
    nodes = np.frombuffer(tree, dtype=[('low', '<f4', 4), ('high', '<f4', 4), ('range', '<i4', 4)])
    packed = np.frombuffer(mesh, '<f4').reshape(-1, 3, 4)[..., :3]
    return nodes, np.stack((packed[:, 0], packed[:, 0]+packed[:, 1], packed[:, 0]+packed[:, 2]), axis=1)


@pytest.mark.parametrize('count', [1, 8, 9, 31, 4155])
def test_tree_preserves_geometry_and_contains_descendants(count):
    triangles = np.random.default_rng(71).uniform(-3, 3, (count, 3, 3)).astype(np.float32)
    nodes, posed = unpack(triangles)
    # Unique first vertex identifies the original triangle after spatial sorting.
    by_origin = {tuple(t[0]): t for t in triangles}
    for triangle in posed:
        assert np.allclose(triangle, by_origin[tuple(triangle[0])], atol=1.e-6)
    spans = []
    for index, node in enumerate(nodes):
        start, length, escape, _ = node['range']
        assert index < escape <= len(nodes)
        if length:
            assert 1 <= length <= 8 and escape == index+1
            assert 0 <= start < start+length <= count
            spans.extend(range(start, start+length))
            descendants = posed[start:start+length]
        else:
            assert escape > index+2
            assert nodes[index+1]['range'][2] < escape
            leaves = [n for n in nodes[index+1:escape] if n['range'][1]]
            descendants = np.concatenate([posed[n['range'][0]:n['range'][0]+n['range'][1]] for n in leaves])
        assert np.all(descendants >= node['low'][:3]-1.e-6)
        assert np.all(descendants <= node['high'][:3]+1.e-6)
    assert sorted(spans) == list(range(count))
    assert nodes[0]['range'][2] == len(nodes)


@pytest.mark.parametrize('triangles', [[], [[[0, 0, 0]]], np.full((1, 3, 3), np.nan)])
def test_malformed_geometry_rejected(triangles):
    with pytest.raises(ValueError):
        build_tree(triangles)


def test_vector_packing_matches_std430_stride():
    original = np.array([[1, 2, 3], [4, 5, 6]])
    packed = np.frombuffer(vectors(original), '<f4').reshape(-1, 4)
    assert np.array_equal(packed[:, :3], original)
    assert np.array_equal(packed[:, 3], [0, 0])
    with pytest.raises(ValueError):
        vectors([[0, np.inf, 0]])
