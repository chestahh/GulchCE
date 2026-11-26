"""Real GL depth-copy equivalence; opt in with SHADOW_ENV_GPU_TEST=1.

The dispatch harness tests the production queue. This complementary check
tests exact depth composition with the shipped skinning/cutout shaders.
"""
import ast
import ctypes as C
import os
from pathlib import Path
import re

import numpy as np
import pytest
from tools.modern_shadow_geometry_gpu import Context
from tools.shadow_stencil_compare import Replay, U, I, P

ROOT = Path(__file__).resolve().parents[1]
pytestmark = pytest.mark.skipif(os.name != 'nt' or os.getenv('SHADOW_ENV_GPU_TEST') != '1',
                                reason='Explicit hidden Windows GL test')


@pytest.mark.parametrize('size', [32, 128])
@pytest.mark.parametrize('cutout', [0, 1])
def test_shared_depth_preserves_both_receivers(size, cutout):
    c = Context(); r = None
    try:
        r = Replay(c, size, size); g = c.gl
        source = (ROOT/'port/linux/src/modern_shadows_gl.c').read_text()
        def shader(name):
            body = re.search(r'static const char '+name+r'\[\]\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', source)[1]
            return ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', body))
        program = r.program(shader('vertex'), shader('fragment'))
        cubes = [r.new('Textures') for _ in range(4)]
        for cube in cubes:
            g('BindTexture', None, U, U)(0x8513, cube)
            for face in range(6):
                g('TexImage2D', None, U,I,I,I,I,I,U,U,P)(0x8515+face,0,0x8CAC,size,size,0,0x1902,0x1406,None)
            for param in (0x2801, 0x2800): g('TexParameteri', None,U,U,I)(0x8513,param,0x2600)
        alpha = r.new('Textures'); g('ActiveTexture',None,U)(0x84C0)
        g('BindTexture',None,U,U)(0x0DE1,alpha)
        pixels = np.array([[[255,255,255,127],[255,255,255,128]]],np.uint8)
        g('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x0DE1,0,0x8058,2,1,0,0x1908,0x1401,pixels.ctypes.data)
        for param in (0x2801,0x2800): g('TexParameteri',None,U,U,I)(0x0DE1,param,0x2600)
        g('BindFramebuffer',None,U,U)(0x8D40,r.cube_fbo)
        g('DrawBuffer',None,U)(0); g('ReadBuffer',None,U)(0)
        g('Viewport',None,I,I,I,I)(0,0,size,size)
        g('ClipControl',None,U,U)(0x8CA1,0x935F)
        g('Enable',None,U)(0x0B71); g('DepthFunc',None,U)(0x0204)
        g('DepthMask',None,U)(1); g('ClearDepth',None,C.c_double)(0)
        g('Disable',None,U)(0x0B44); g('UseProgram',None,U)(program)
        r.uniform(program,'alpha_map',0)
        loc = lambda name: g('GetUniformLocation',I,U,C.c_char_p)(program,name.encode())
        light = np.array([0,0,0,8],np.float32); uv = np.array([-1,0,0,1,0,1,0,0],np.float32)
        bones = np.array([np.eye(4),np.eye(4)],np.float32)
        bones[0,0,3]=.12; bones[1,0,3]=-.08
        g('Uniform4fv',None,I,I,P)(loc('light'),1,light.ctypes.data)
        g('Uniform4fv',None,I,I,P)(loc('uv_transform'),2,uv.ctypes.data)
        g('UniformMatrix4fv',None,I,I,U,P)(loc('bones'),2,1,bones.ctypes.data)
        buffers = []
        # One shared, one body-only and one viewmodel-only quad, in every face.
        for role, (center, depth) in enumerate(((0,3),(-.6,2),(.6,2.2))):
            v = np.array([[-1,-1,depth,0,1,.75,0,0],[1,-1,depth,0,1,.75,1,0],
                          [1,1,depth,0,1,.25,1,1],[-1,-1,depth,0,1,.75,0,0],
                          [1,1,depth,0,1,.25,1,1],[-1,1,depth,0,1,.25,0,1]],np.float32)
            v[:,0] += center
            parts=[]
            for face in range(6):
                part=v.copy(); x,y,z=v[:,:3].T
                part[:,:3]=np.array(((z,-y,-x),(-z,-y,x),(x,z,y),(x,-z,-y),(x,-y,z),(-x,-y,-z))[face]).T
                parts.append(part)
            vertices=np.concatenate(parts)
            buffers.append(c.buffer(0,vertices.tobytes()))
        def draw(cube, roles, clear):
            for face in range(6):
                g('FramebufferTexture2D',None,U,U,U,U,I)(0x8D40,0x8D00,0x8515+face,cube,0)
                assert g('CheckFramebufferStatus',U,U)(0x8D40)==0x8CD5
                if clear: g('Clear',None,U)(0x100)
                r.uniform(program,'face',face)
                for role in roles:
                    r.uniform(program,'alpha_test',cutout if role==0 else 0)
                    g('BindVertexArray',None,U)(r.vao);g('BindBuffer',None,U,U)(0x8892,buffers[role])
                    for slot,count,offset in [(0,3,0),(1,2,12),(2,1,20),(3,2,24)]:
                        g('EnableVertexAttribArray',None,U)(slot)
                        g('VertexAttribPointer',None,U,I,U,U,I,P)(slot,count,0x1406,0,32,C.c_void_p(offset))
                    g('DrawArrays',None,U,I,I)(4,0,36)
        draw(cubes[0], [1,0], True); draw(cubes[1], [2,0], True)
        draw(cubes[2], [0], True)
        g('CopyImageSubData',None,U,U,I,I,I,I,U,U,I,I,I,I,I,I,I)(
            cubes[2],0x8513,0,0,0,0,cubes[3],0x8513,0,0,0,0,size,size,6)
        draw(cubes[2], [1], False); draw(cubes[3], [2], False)
        results=[]
        for cube in cubes:
            faces=[]
            for face in range(6):
                g('FramebufferTexture2D',None,U,U,U,U,I)(0x8D40,0x8D00,0x8515+face,cube,0)
                depth=np.empty((size,size),np.float32)
                g('ReadPixels',None,I,I,I,I,U,U,P)(0,0,size,size,0x1902,0x1406,depth.ctypes.data)
                faces.append(depth)
            results.append(np.array(faces))
        c.check()
        assert np.count_nonzero(results[0]) > size*size
        assert not np.array_equal(results[0],results[1]), 'Receiver-private geometry must differ'
        assert np.array_equal(results[0],results[2])
        assert np.array_equal(results[1],results[3])
    finally:
        if r: r.close()
        c.close()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
