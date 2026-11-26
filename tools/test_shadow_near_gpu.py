"""Production depth/visibility roundtrip for casters near point emitters."""
import ast
import ctypes as C
import os
from pathlib import Path
import re

import numpy as np
import pytest
from tools.shadow_stencil_compare import Context, Replay, U, I, P

ROOT = Path(__file__).resolve().parents[1]
pytestmark = pytest.mark.skipif(os.name != 'nt' or os.getenv('SHADOW_ENV_GPU_TEST') != '1',
                                reason='Explicit hidden Windows GL test')


@pytest.mark.parametrize('radius,distance', [(8., .0008), (8., .0024), (8., .1),
                                          (8., 2.), (8., 7.5), (64., 63.5)])
@pytest.mark.parametrize('cutout', [False, True])
@pytest.mark.parametrize('soft', [0, 1])
def test_close_caster_depth_and_visibility(radius, distance, cutout, soft):
    source = (ROOT/'port/linux/src/modern_shadows_gl.c').read_text()
    def shader(name):
        body = re.search(r'static const char '+name+r'\[\]\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', source)[1]
        return ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', body))
    emitted = source.split('void modern_shadow_emit(', 1)[1].split('void modern_shadow_uniforms_initialize', 1)[0]
    emitted = (ROOT/'port/linux/src/modern_sun_shader.inl').read_text() + emitted
    visibility = ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', emitted))
    c = Context(); r = None
    try:
        r = Replay(c, 32, 32); g = c.gl
        depth_program = r.program(shader('vertex'), shader('fragment'))
        compare = r.program('#version 330 core\nlayout(location=0)in vec4 position;'
                            'uniform vec4 point;out vec3 world;'
                            'void main(){world=point.xyz;gl_Position=vec4(position.xy,0,1);}',
                            '#version 330 core\nin vec3 world;out vec4 color;\n'+visibility+
                            '\nvoid main(){float v=modern_visibility(world-modern_light.xyz);color=vec4(v,v,v,1);}')
        g('BindTexture',None,U,U)(0x8513,r.cube)
        for face in range(6):
            g('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x8515+face,0,0x8CAC,32,32,0,0x1902,0x1406,None)
        for param,value in ((0x2800,0x2600),(0x2801,0x2600),(0x2802,0x812F),(0x2803,0x812F),(0x8072,0x812F)):
            g('TexParameteri',None,U,U,I)(0x8513,param,value)
        alpha = r.new('Textures');g('ActiveTexture',None,U)(0x84C0)
        g('BindTexture',None,U,U)(0x0DE1,alpha)
        pixels = np.array([255,255,255,0 if cutout else 255],np.uint8)
        g('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x0DE1,0,0x8058,1,1,0,0x1908,0x1401,pixels.ctypes.data)
        for param in (0x2800,0x2801):g('TexParameteri',None,U,U,I)(0x0DE1,param,0x2600)
        quad = np.array([[-1,-1],[1,-1],[1,1],[-1,-1],[1,1],[-1,1]],np.float32)
        screen = r.upload(np.c_[quad,np.zeros(6),np.ones(6)])
        bone = np.eye(4,dtype=np.float32)
        for face in range(6):
            axis, sign = face//2, -1 if face&1 else 1
            vertices = np.zeros((6,8),np.float32)
            vertices[:,axis]=sign*distance
            others=[a for a in range(3) if a!=axis]
            vertices[:,others]=quad*distance
            vertices[:,5]=1
            buffer=c.buffer(0,vertices.tobytes())
            g('BindFramebuffer',None,U,U)(0x8D40,r.cube_fbo)
            g('FramebufferTexture2D',None,U,U,U,U,I)(0x8D40,0x8D00,0x8515+face,r.cube,0)
            g('DrawBuffer',None,U)(0);g('ReadBuffer',None,U)(0)
            assert g('CheckFramebufferStatus',U,U)(0x8D40)==0x8CD5
            g('Viewport',None,I,I,I,I)(0,0,32,32)
            g('ClipControl',None,U,U)(0x8CA1,0x935F)
            g('Enable',None,U)(0x0B71);g('DepthFunc',None,U)(0x0204)
            g('DepthMask',None,U)(1);g('ClearDepth',None,C.c_double)(0)
            g('Disable',None,U)(0x0B44);g('Clear',None,U)(0x100)
            g('UseProgram',None,U)(depth_program)
            r.uniform(depth_program,'bones',bone)
            r.uniform(depth_program,'light',[0,0,0,radius]);r.uniform(depth_program,'face',face)
            r.uniform(depth_program,'alpha_map',0);r.uniform(depth_program,'alpha_test',1)
            g('BindVertexArray',None,U)(r.vao);g('BindBuffer',None,U,U)(0x8892,buffer)
            for slot,count,offset in [(0,3,0),(1,2,12),(2,1,20),(3,2,24)]:
                g('EnableVertexAttribArray',None,U)(slot)
                g('VertexAttribPointer',None,U,I,U,U,I,P)(slot,count,0x1406,0,32,C.c_void_p(offset))
            g('DrawArrays',None,U,I,I)(4,0,6)
            for receiver,expected in [(distance*.5,1),(min(radius-.1,distance+max(.002,distance*.5)),int(cutout))]:
                r.begin();g('UseProgram',None,U)(compare)
                point=np.zeros(4,np.float32);point[axis]=sign*receiver
                r.uniform(compare,'point',point);r.uniform(compare,'modern_light',[0,0,0,radius])
                r.uniform(compare,'modern_soft',soft);r.uniform(compare,'modern_depth',0)
                g('ActiveTexture',None,U)(0x84C0);g('BindTexture',None,U,U)(0x8513,r.cube)
                r.draw(compare,screen,np.eye(4))
                assert r.read()[16,16,0] == pytest.approx(expected,abs=1e-6), (face,distance,receiver,cutout)
    finally:
        if r:r.close()
        c.close()
