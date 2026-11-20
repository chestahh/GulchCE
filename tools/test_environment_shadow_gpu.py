"""Exercise the shipped environmental GLSL on a hidden Windows GL context.
Set SHADOW_ENV_GPU_TEST=1; these are synthetic correctness checks, not FPS evidence.
"""
import ast
import ctypes as C
import os
from pathlib import Path
import re
import numpy as np
import pytest
from tools.modern_shadow_geometry_gpu import Context
from tools.shadow_stencil_compare import Replay, U, I, P, F

ROOT=Path(__file__).resolve().parents[1]
pytestmark=pytest.mark.skipif(os.name!='nt' or os.getenv('SHADOW_ENV_GPU_TEST')!='1',reason='Explicit Windows GL test')

def shader(name, source='modern_environment_gl.inl'):
    text=(ROOT/'port/linux/src'/source).read_text()
    body=re.search(r'static const char '+name+r'\[\]\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);',text)[1]
    return ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',body))

@pytest.mark.parametrize('depth_stencil',[False,True])
@pytest.mark.parametrize('reverse',[0,1])
@pytest.mark.parametrize('soft',[0,1])
@pytest.mark.parametrize('sun',[False,True])
def test_directional_receiver_depth_and_empty_rays(reverse,soft,depth_stencil,sun):
    c=Context(); r=None
    try:
        r=Replay(c,64,64); g=c.gl
        r.program(shader('caster_vertex'),shader('caster_fragment'))
        fragment=shader('receiver_fragment')
        if sun: fragment=fragment.replace('#version 330 core', '#version 330 core\n#define SUN',1)
        fullscreen=r.program(shader('receiver_vertex'),fragment)
        volume=r.program(shader('sun_vertex'),fragment) if sun else None
        p=fullscreen
        projection=np.zeros((4,4),np.float32)
        projection[0,0]=projection[1,1]=1
        projection[2,2]=-100/(100-.1)
        projection[2,3]=-100*.1/(100-.1)
        projection[3,2]=-1
        camera=np.eye(4,dtype=np.float32);camera[2,3]=3
        frame=np.diag([1,-1,-1,1]).astype(np.float32)
        textures=[r.new('Textures') for _ in range(3 if sun else 2)]
        for unit,tex in enumerate(textures):
            g('ActiveTexture',None,U)(0x84C0+unit);g('BindTexture',None,U,U)(0x0DE1,tex)
            for param in (0x2801,0x2800):g('TexParameteri',None,U,U,I)(0x0DE1,param,0x2600)
        def draw(z,empty=False,roof=False):
            world_z=z;view_z=world_z-camera[2,3]
            clip=projection@np.array([0,0,view_z,1]);d=clip[2]/clip[3]
            data=np.full((64,64),1-d if reverse else d,np.float32)
            shadow=np.zeros((64,64),np.float32)
            if not empty:shadow[16:48,16:48]=.5
            arrays=[data,shadow]
            if sun: arrays.append(np.full_like(data, .9 if roof else .5+.5*z))
            for unit,array in enumerate(arrays):
                g('ActiveTexture',None,U)(0x84C0+unit);g('BindTexture',None,U,U)(0x0DE1,textures[unit])
                if depth_stencil:
                    packed=np.zeros((64,64,2),np.float32);packed[:,:,0]=array
                    g('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x0DE1,0,0x8CAD,64,64,0,0x84F9,0x8DAD,packed.ctypes.data)
                else:
                    g('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x0DE1,0,0x822E,64,64,0,0x1903,0x1406,array.ctypes.data)
            r.begin();g('ClipControl',None,U,U)(0x8CA2,0x935F)
            g('Disable',None,U)(0x0B71);g('UseProgram',None,U)(p)
            for name,value in {'scene_depth':0,'shadow_depth':1,'camera':camera,'projection':projection,
                               'frame':frame,'viewport':[0,0,64,64],'color':[.5,.5,.5,1],
                               'reversed_depth':reverse,'soft':soft}.items():r.uniform(p,name,value)
            if sun: r.uniform(p,'static_depth',2)
            if volume and p==volume:
                g('Enable',None,U)(0x864F)  # GL_DEPTH_CLAMP on the exit faces
                g('Enable',None,U)(0x0B71)
                g('ClearDepth',None,C.c_double)(float(data[0,0]))
                g('Clear',None,U)(0x100)
                g('DepthMask',None,U)(0)
                g('DepthFunc',None,U)(0x0203 if reverse else 0x0206)
                r.uniform(p,'reach_min',[-.55,-.55,-.1,0])
                r.uniform(p,'reach_max',[.55,.55,1,0])
            location=g('GetUniformLocation',I,U,C.c_char_p)(p,b'radius')
            g('Uniform1f',None,I,F)(location,1)
            g('BindVertexArray',None,U)(r.vao);g('DrawArrays',None,U,I,I)(4,0,36 if volume and p==volume else 3)
            return r.read()
        behind=draw(-.5)
        if volume:
            p=volume
            assert np.array_equal(draw(-.5),behind), 'Bounded sun projection must match the full-screen reference'
            camera[2,3]=0
            p=fullscreen
            inside=draw(-.5)
            p=volume
            assert np.array_equal(draw(-.5),inside), 'Camera crossing into the receiver volume'
            camera[2,3]=3
        assert np.allclose(behind[31:33,31:33,:3],.5 if sun else .5625,atol=.002)
        if sun:
            # Offset the shadow texel grid from the screen pixel grid.
            frame[0,3] += .013
            edge = draw(-.5)[:,:,0]
            fractional = (edge > .502) & (edge < .998)
            assert bool(fractional.any()) == bool(soft)
            frame[0,3] -= .013
        if sun: assert np.allclose(draw(-.5,roof=True)[:,:,:3],1), "Roof already blocks direct sunlight"
        assert np.allclose(behind[:4,:,:3],1)
        assert np.allclose(draw(.25)[:,:,:3],1), 'Surface in front of caster must remain lit'
        assert np.allclose(draw(-.5,True)[:,:,:3],1), 'Empty rays must never occlude'
    finally:
        if r:r.close()
        c.close()


@pytest.mark.parametrize('soft', [0, 1])
def test_spot_sources_are_independent(soft):
    """Actual receiver shader. A blocker in a baked ray cannot remove its light twice."""
    context=Context()
    replay=None
    try:
        replay=Replay(context,32,32)
        gl=context.gl
        fragment=('#version 330 core\n'+shader('spot_shader','modern_spots_gl.inl')+
            'out vec4 result; void main(){vec3 p=vec3((gl_FragCoord.xy-16.)/32.,.5);'
            'result=vec4(fixture_visibility(p,vec3(0,0,1)),beam_visibility(p));}')
        program=replay.program(shader('receiver_vertex'),fragment)
        texture=replay.new('Textures')
        gl('ActiveTexture',None,U)(0x84c0)
        gl('BindTexture',None,U,U)(0x8c1a,texture)
        for param in (0x2800,0x2801):
            gl('TexParameteri',None,U,U,I)(0x8c1a,param,0x2600)
        layers=np.zeros((5,32,32),np.float32)
        layers[0,:,:16]=.75
        layers[1,:,16:]=.75
        layers[4,16:,:]=.75
        def draw(mask):
            gl('TexImage3D',None,U,I,I,I,I,I,I,U,U,P)(0x8c1a,0,0x822e,32,32,5,0,0x1903,0x1406,layers.ctypes.data)
            replay.begin()
            gl('Disable',None,U)(0x0b71)
            gl('UseProgram',None,U)(program)
            for name,value in [('spot_mask',mask),('spot_soft',soft),('spot_maps',0)]:
                replay.uniform(program,name,value)
            for i in range(3):
                replay.uniform(program,f'spot_frames[{i}]',np.eye(4))
                replay.uniform(program,f'spot_origins[{i}]',[0,0,4,1])
                replay.uniform(program,f'spot_colors[{i}]',[.4 if i==0 else 0,.4 if i==1 else 0,0,16])
            gl('BindVertexArray',None,U)(replay.vao)
            gl('DrawArrays',None,U,I,I)(4,0,3)
            return replay.read()
        both=draw(3)
        assert both[16,8,0]<.85 and both[16,8,1]==1
        assert both[16,24,1]<.85 and both[16,24,0]==1
        assert np.allclose(both[:,:,2],1)
        assert np.allclose(np.minimum(draw(1)[:,:,:3],draw(2)[:,:,:3]),both[:,:,:3])
        assert set(np.unique(both[[6,25],16,3]))=={0,1}
        layers[2:4]=.8
        assert np.allclose(draw(3)[:,:,:3],1)
        layers[:]=0
        assert np.allclose(draw(3),1)
    finally:
        if replay: replay.close()
        context.close()


@pytest.mark.parametrize('spot', [False, True])
def test_caster_skinning_and_animated_cutout(spot):
    """Read actual rasterized depth, including two bone weights and UV reversal."""
    c=Context();r=None
    try:
        r=Replay(c,64,64);g=c.gl
        p=r.program(shader('caster_vertex'),shader('caster_fragment'))
        vertices=np.array([[-.5,-.5,0,0,1,.75,0,0],[.5,-.5,0,0,1,.75,1,0],
                           [.5,.5,0,0,1,.75,1,1],[-.5,-.5,0,0,1,.75,0,0],
                           [.5,.5,0,0,1,.75,1,1],[-.5,.5,0,0,1,.75,0,1]],np.float32)
        buffer=c.buffer(0,vertices.tobytes())
        texture=r.new('Textures');g('ActiveTexture',None,U)(0x84C0)
        g('BindTexture',None,U,U)(0x0DE1,texture)
        alpha=np.array([[[255,255,255,127],[255,255,255,128]]],np.uint8)
        g('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x0DE1,0,0x8058,2,1,0,0x1908,0x1401,alpha.ctypes.data)
        for param in (0x2801,0x2800):g('TexParameteri',None,U,U,I)(0x0DE1,param,0x2600)
        bones=np.array([np.eye(4),np.eye(4)],np.float32);bones[0,0,3]=.4;bones[1,0,3]=-.4
        frame=np.eye(4,dtype=np.float32);frame[0,3]=.1
        def draw(cutout,reverse):
            r.begin();g('ClipControl',None,U,U)(0x8CA1,0x935F)
            g('Enable',None,U)(0x0B71);g('DepthFunc',None,U)(0x0204)
            g('DepthMask',None,U)(1);g('ClearDepth',None,C.c_double)(0)
            g('Clear',None,U)(0x100);g('Disable',None,U)(0x0B44)
            g('UseProgram',None,U)(p)
            caster_frame=frame
            if spot:
                # Reverse-Z perspective with w=1+z. At the test plane,
                # projection and texel coverage agree with the reference.
                caster_frame=np.eye(4,dtype=np.float32)
                caster_frame[0,3]=-.1
                caster_frame[3,2]=1
                caster_frame[2]=[0,0,-.05,.5]
            r.uniform(p,'frame',caster_frame);r.uniform(p,'alpha_map',0);r.uniform(p,'alpha_test',cutout)
            loc=lambda name:g('GetUniformLocation',I,U,C.c_char_p)(p,name.encode())
            g('Uniform1f',None,I,F)(loc('radius'),-1 if spot else 1)
            g('UniformMatrix4fv',None,I,I,U,P)(loc('bones'),2,1,bones.ctypes.data)
            uv=np.array([-1,0,0,1,0,1,0,0] if reverse else [1,0,0,0,0,1,0,0],np.float32)
            g('Uniform4fv',None,I,I,P)(loc('uv_transform'),2,uv.ctypes.data)
            g('BindVertexArray',None,U)(r.vao);g('BindBuffer',None,U,U)(0x8892,buffer)
            for slot,size,offset in [(0,3,0),(1,2,12),(2,1,20),(3,2,24)]:
                g('EnableVertexAttribArray',None,U)(slot)
                g('VertexAttribPointer',None,U,I,U,U,I,P)(slot,size,0x1406,0,32,C.c_void_p(offset))
            g('DrawArrays',None,U,I,I)(4,0,6)
            depth=np.empty((64,64),np.float32)
            g('ReadPixels',None,I,I,I,I,U,U,P)(0,0,64,64,0x1902,0x1406,depth.ctypes.data)
            c.check();return depth
        cut=draw(1,False);flipped=draw(1,True);opaque=draw(0,False)
        assert cut[32,26]==0 and abs(cut[32,42]-.5)<1e-5
        assert flipped[32,42]==0 and abs(flipped[32,26]-.5)<1e-5
        assert abs(opaque[32,26]-.5)<1e-5 and abs(opaque[32,42]-.5)<1e-5
        # Weighted translation .2 minus frame translation .1 moves the quad.
        assert opaque[32,18]==0 and opaque[32,50]>.49 and opaque[32,52]==0
    finally:
        if r:r.close()
        c.close()
