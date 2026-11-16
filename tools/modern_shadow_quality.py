"""Run the production receiver GLSL against analytic depth cubes on desktop GL.

Requires Windows, numpy and Pillow. No game assets or source
instrumentation; --record permits collecting a failing pre-fix baseline.
"""
import argparse
import ast
import ctypes as C
import hashlib
import json
from pathlib import Path
import re

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'port/linux/src/modern_shadows_gl.c')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--record', action='store_true')
    parser.add_argument('--size', type=int, choices=[512, 1024], default=1024)
    args = parser.parse_args()
    size = args.size
    args.output.mkdir(parents=True, exist_ok=True)
    user, gdi, ogl = C.WinDLL('user32'), C.WinDLL('gdi32'), C.WinDLL('opengl32')

    def api(library, name, result, *parameters):
        f = getattr(library, name)
        f.restype, f.argtypes = result, parameters
        return f

    window = api(user, 'CreateWindowExW', C.c_void_p, C.c_uint, C.c_wchar_p,
                 C.c_wchar_p, C.c_uint, C.c_int, C.c_int, C.c_int, C.c_int,
                 C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p)(
                     0, 'STATIC', 'Shadow quality regression', 0, 0, 0, 256, 256,
                     None, None, None, None)
    dc = api(user, 'GetDC', C.c_void_p, C.c_void_p)(window)
    # PIXELFORMATDESCRIPTOR: RGBA window surface, OpenGL, 32 color bits.
    descriptor = bytearray(40)
    import struct
    struct.pack_into('<HHI', descriptor, 0, 40, 1, 0x24)
    descriptor[9] = 32
    pixel_format = C.create_string_buffer(bytes(descriptor))
    index = api(gdi, 'ChoosePixelFormat', C.c_int, C.c_void_p, C.c_void_p)(dc,pixel_format)
    assert api(gdi, 'SetPixelFormat', C.c_bool, C.c_void_p, C.c_int, C.c_void_p)(dc,index,pixel_format)
    context = api(ogl, 'wglCreateContext', C.c_void_p, C.c_void_p)(dc)
    assert api(ogl, 'wglMakeCurrent', C.c_bool, C.c_void_p, C.c_void_p)(dc,context)
    proc = api(ogl, 'wglGetProcAddress', C.c_void_p, C.c_char_p)

    def gl(name, result, *parameters):
        name = 'gl'+name
        if hasattr(ogl, name):
            return api(ogl, name, result, *parameters)
        address = proc(name.encode())
        assert address, name
        return C.WINFUNCTYPE(result, *parameters)(address)

    u, i, f, p = C.c_uint, C.c_int, C.c_float, C.c_void_p
    shader = gl('CreateShader', u, u)
    shader_source = gl('ShaderSource', None, u, i, C.POINTER(C.c_char_p), p)
    compile_shader = gl('CompileShader', None, u)
    get_shader = gl('GetShaderiv', None, u, u, C.POINTER(i))
    get_log = gl('GetShaderInfoLog', None, u, i, p, C.c_char_p)

    def compile_stage(kind, source):
        obj = shader(kind)
        string = C.c_char_p(source.encode())
        shader_source(obj, 1, C.byref(string), None); compile_shader(obj)
        status = i(); get_shader(obj, 0x8B81, C.byref(status))
        log = C.create_string_buffer(16384); get_log(obj, len(log), None, log)
        assert status.value, log.value.decode()
        return obj

    source = args.source.read_text()
    reversed_depth = 'glDepthFunc(GL_GREATER)' in source
    body = source.split('void modern_shadow_emit(struct xgpu_text *text)', 1)[1].split('\nvoid modern_shadow_uniforms_initialize', 1)[0]
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    receiver = ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', body))
    vertex = '''#version 450 core
    out vec2 uv; void main() {
      uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;
      gl_Position=vec4(uv,0,1);
    }'''
    fragment = '#version 450 core\n' + receiver + '''
    in vec2 uv; out vec4 color;
    uniform vec3 center, along, up; uniform int model_slot, world_receiver;
    void main() { vec3 p=center+uv.x*along+uv.y*up;
      float visibility=model_slot<0?modern_visibility(p):modern_model(model_slot,p);
      if(world_receiver>0) visibility=modern_world(p,vec4(world_receiver==1?0.0:1.0));
      color=vec4(vec3(visibility),1); }
    '''
    def link(vertex, fragment):
        program = gl('CreateProgram', u)()
        attach = gl('AttachShader', None, u, u)
        for kind, text in ((0x8B31, vertex), (0x8B30, fragment)):
            obj = compile_stage(kind, text); attach(program, obj)
            gl('DeleteShader', None, u)(obj)
        gl('LinkProgram', None, u)(program)
        status = i(); gl('GetProgramiv', None, u, u, C.POINTER(i))(program, 0x8B82, C.byref(status))
        assert status.value, 'shadow shader link failed'
        return program

    program = link(vertex, fragment)
    gl('UseProgram', None, u)(program)
    location = gl('GetUniformLocation', i, u, C.c_char_p)
    def uniform(name): return location(program, name.encode())
    uniform3 = gl('Uniform3f', None, i, f, f, f)
    gl('Uniform4f', None, i, f, f, f, f)(uniform('modern_light'), 0, 0, 0, 100)
    uniform1 = gl('Uniform1i', None, i, i)
    uniform1(uniform('modern_depth'), 0)
    uniform1(uniform('model_slot'), -1)
    vao = u(); gl('GenVertexArrays', None, i, C.POINTER(u))(1, C.byref(vao))
    gl('BindVertexArray', None, u)(vao)
    cube = u(); gl('GenTextures', None, i, C.POINTER(u))(1, C.byref(cube))
    gl('BindTexture', None, u, u)(0x8513, cube)
    parameter = gl('TexParameteri', None, u, u, i)
    for key, value in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F),(0x8072,0x812F)):
        parameter(0x8513, key, value)
    upload = gl('TexImage2D', None, u, i, i, i, i, i, u, u, p)
    target = u(); gl('GenTextures', None, i, C.POINTER(u))(1, C.byref(target))
    gl('BindTexture', None, u, u)(0x0DE1,target)
    upload(0x0DE1,0,0x8058,256,256,0,0x1908,0x1401,None)
    framebuffer = u(); gl('GenFramebuffers',None,i,C.POINTER(u))(1,C.byref(framebuffer))
    gl('BindFramebuffer',None,u,u)(0x8D40,framebuffer)
    gl('FramebufferTexture2D',None,u,u,u,u,i)(0x8D40,0x8CE0,0x0DE1,target,0)
    assert gl('CheckFramebufferStatus',u,u)(0x8D40)==0x8CD5
    read = gl('ReadPixels', None, i, i, i, i, u, u, p)
    draw = gl('DrawArrays', None, u, i, i)
    gl('Viewport', None, i, i, i, i)(0, 0, 256, 256)
    s, t = np.meshgrid((np.arange(size)+.5)*2/size-1, (np.arange(size)+.5)*2/size-1)
    one = np.ones_like(s)
    rays = [np.stack(v,axis=-1) for v in ((one,-t,-s),(-one,-t,s),(s,one,t),(s,-one,-t),(s,-t,one),(-s,-t,-one))]

    def depth_plane(normal, offset, blocker=None):
        for face, ray in enumerate(rays):
            denominator = ray@normal
            scale = np.divide(offset, denominator, out=np.full_like(s, 1e6), where=abs(denominator)>1e-12)
            depth = np.where(scale>0, scale*np.linalg.norm(ray,axis=-1), 100)
            if blocker is not None:
                # Half-plane halfway to the front-facing receiver: a crisp
                # geometric edge, independent of the shader's sampling code.
                depth = np.where((ray@blocker>0)&(scale>0), depth*.5, depth)
            if reversed_depth:
                axis = depth / np.linalg.norm(ray, axis=-1)
                depth = np.where((scale > 0) & (axis < 100),
                                 .005 * (100-axis) / (axis*(100-.005)), 0)
            else:
                depth /= 100
            data = np.ascontiguousarray(np.clip(depth,0,1), dtype=np.float32)
            upload(0x8515+face,0,0x8CAC,size,size,0,0x1902,0x1406,data.ctypes.data)

    def capture(center, along, up, soft):
        for name, value in (('center',center),('along',along),('up',up)):
            uniform3(uniform(name), *value)
        uniform1(uniform('modern_soft'), soft)
        draw(0x0004,0,3)
        data = np.empty((256,256,4),dtype=np.uint8)
        read(0,0,256,256,0x1908,0x1401,data.ctypes.data)
        return data[...,0]

    metrics = {}
    for name, center, normal, along, up in (
        ('front', (0,0,5),(0,0,1),(2,0,0),(0,2,0)),
        ('seam', (5,0,5),(1,0,1),(1,0,-1),(0,2,0)),
        ('negative-seam', (-5,0,-5),(1,0,1),(1,0,-1),(0,2,0)),
        ('top-seam', (0,5,5),(0,1,1),(2,0,0),(0,1,-1)),
        ('bottom-seam', (0,-5,-5),(0,1,1),(2,0,0),(0,1,-1)),
        ('corner', (5,5,5),(1,1,1),(1,-1,0),(0,1,-1)),
        ('close', (0,0,.02),(0,0,1),(.01,0,0),(0,.01,0)),
        ('small-derivatives', (.5,0,.02),(0,0,1),(.01,0,0),(0,.01,0)),
        ('range-grazing', (70,0,.25),(0,0,1),(1,0,0),(0,1,0)),
        ('grazing', (5,0,.02),(0,0,1),(1,0,0),(0,1,0))):
        depth_plane(np.array(normal), np.dot(center,normal))
        for soft in (0,1):
            result = capture(center,along,up,soft)
            key = f'{name}-{soft}'
            metrics[key] = {'shadowed_fraction':float(np.mean(result<250)), 'minimum':int(result.min())}
            Image.fromarray(result).save(args.output/(key+'.png'))
    depth_plane(np.array((0,0,1)), 5, np.array((1,0,0)))
    # Traverse one edge at subtexel increments in a single image. Weighted
    # filtering should interpolate instead of taking quarter-intensity steps.
    # Keep the same subtexel sampling density at each cube resolution.
    edge_span = .05 * 512 / size
    edge = capture((0,0,5),(edge_span,0,0),(0,.01,0),1)
    Image.fromarray(edge).save(args.output/'soft-edge.png')
    metrics['soft-edge'] = {'largest_step':int(np.abs(np.diff(edge[128].astype(int))).max()),
                            'levels':int(len(np.unique(edge[128])))}
    # Hard remains binary, and both modes retain fully shadowed and lit
    # interiors. A shader returning white everywhere cannot pass this test.
    hard = capture((0,0,5),(edge_span,0,0),(0,.01,0),0)
    assert set(np.unique(hard)) == {0,255}
    assert edge[128,0] == 255 and edge[128,-1] == 0
    depth_plane(np.array((1,0,1)),10,np.array((0,1,0)))
    seam = capture((5,0,5),(1,0,-1),(0,.08,0),1)
    Image.fromarray(seam).save(args.output/'occluder-seam.png')
    metrics['occluder-seam'] = {'largest_step':int(np.abs(np.diff(seam.astype(int),axis=1)).max()),
                               'lit':int(seam[:8].min()), 'shadow':int(seam[-8:].max())}
    uniform1(uniform('world_receiver'),1)
    absent = capture((5,0,5),(1,0,-1),(0,.08,0),1)
    assert absent.min()==255, 'zero-contribution world pass must skip shadow filtering'
    uniform1(uniform('world_receiver'),2)
    present = capture((5,0,5),(1,0,-1),(0,.08,0),1)
    uniform1(uniform('world_receiver'),0)
    assert np.array_equal(present,capture((5,0,5),(1,0,-1),(0,.08,0),1)), 'world gate must preserve nonzero light'
    metrics['world-contribution'] = dict(absent_minimum=int(absent.min()),identical_nonzero=True)
    # The second model light must sample its own cube, not the first light's
    # depth. Exercise both runtime slot branches and disabling either slot.
    other = u(); gl('GenTextures',None,i,C.POINTER(u))(1,C.byref(other))
    gl('ActiveTexture',None,u)(0x84C1)
    gl('BindTexture',None,u,u)(0x8513,other)
    for key,value in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F),(0x8072,0x812F)):
        parameter(0x8513,key,value)
    clear = np.full((size,size),0 if reversed_depth else 1,dtype=np.float32)
    for face in range(6): upload(0x8515+face,0,0x8CAC,size,size,0,0x1902,0x1406,clear.ctypes.data)
    uniform1(uniform('modern_depth_other'),1)
    gl('Uniform4f',None,i,f,f,f,f)(uniform('modern_light_other'),0,0,0,100)
    gl('Uniform2f',None,i,f,f)(uniform('modern_slots'),1,1)
    uniform1(uniform('model_slot'),0)
    first = capture((5,0,5),(1,0,-1),(0,.08,0),1)
    uniform1(uniform('model_slot'),1)
    second = capture((5,0,5),(1,0,-1),(0,.08,0),1)
    gl('Uniform2f',None,i,f,f)(uniform('modern_slots'),0,1)
    uniform1(uniform('model_slot'),0)
    disabled = capture((5,0,5),(1,0,-1),(0,.08,0),1)
    metrics['model-pair'] = {'first_shadow':int(first[-8:].max()),
                             'second_lit':int(second.min()),'disabled_lit':int(disabled.min())}
    # Exercise the actual caster too: analytic uploads alone cannot detect
    # a mismatched projection, depth test, clear value or fragment encoding.
    stages = []
    for name in ('vertex', 'fragment'):
        definition = re.search(r'static const char '+name+r'\[\] =\s*((?:"(?:\\.|[^"\\])*"\s*)+);', source)
        assert definition, name
        stages.append(''.join(ast.literal_eval(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', definition[1])))
    caster = link(*stages)
    buffer = u(); gl('GenBuffers',None,i,C.POINTER(u))(1,C.byref(buffer))
    gl('BindBuffer',None,u,u)(0x8892,buffer)
    gl('EnableVertexAttribArray',None,u)(0)
    gl('VertexAttribPointer',None,u,i,u,C.c_ubyte,i,p)(0,3,0x1406,0,12,None)
    gl('VertexAttrib2f',None,u,f,f)(1,0,0)
    gl('VertexAttrib1f',None,u,f)(2,1)
    gl('VertexAttrib2f',None,u,f,f)(3,0,0)
    depth_fbo = u(); gl('GenFramebuffers',None,i,C.POINTER(u))(1,C.byref(depth_fbo))
    gl('ActiveTexture',None,u)(0x84C0)
    gl('BindTexture',None,u,u)(0x8513,cube)
    uniform1(uniform('model_slot'),-1)
    identity = np.eye(4,dtype=np.float32)
    for name, center, along, up in (
        ('front',(0,0,5),(2,0,0),(0,2,0)),
        ('back',(0,0,-5),(2,0,0),(0,2,0)),
        ('right',(5,0,0),(0,0,2),(0,2,0)),
        ('left',(-5,0,0),(0,0,2),(0,2,0)),
        ('top',(0,5,0),(2,0,0),(0,0,2)),
        ('bottom',(0,-5,0),(2,0,0),(0,0,2)),
        ('seam',(5,0,5),(1,0,-1),(0,2,0)),
        ('grazing',(5,0,.02),(1,0,0),(0,1,0))):
        center,along,up = map(np.asarray,(center,along,up))
        vertices = np.asarray([center+x*along*3+y*up*3 for x,y in
                               ((-1,-1),(1,-1),(1,1),(-1,-1),(1,1),(-1,1))],dtype=np.float32)
        gl('BufferData',None,u,C.c_ssize_t,p,u)(0x8892,vertices.nbytes,vertices.ctypes.data,0x88E0)
        gl('UseProgram',None,u)(caster)
        gl('UniformMatrix4fv',None,i,i,C.c_ubyte,p)(location(caster,b'bones'),1,0,identity.ctypes.data)
        gl('Uniform4f',None,i,f,f,f,f)(location(caster,b'light'),0,0,0,100)
        gl('BindFramebuffer',None,u,u)(0x8D40,depth_fbo)
        gl('DrawBuffer',None,u)(0); gl('ReadBuffer',None,u)(0)
        gl('ClipControl',None,u,u)(0x8CA1,0x935F if reversed_depth else 0x935E)
        gl('Viewport',None,i,i,i,i)(0,0,size,size)
        gl('Enable',None,u)(0x0B71)
        gl('DepthFunc',None,u)(0x0204 if reversed_depth else 0x0201)
        gl('ClearDepth',None,C.c_double)(0 if reversed_depth else 1)
        for face in range(6):
            gl('FramebufferTexture2D',None,u,u,u,u,i)(0x8D40,0x8D00,0x8515+face,cube,0)
            assert gl('CheckFramebufferStatus',u,u)(0x8D40)==0x8CD5
            gl('Clear',None,u)(0x00000100)
            uniform1(location(caster,b'face'),face)
            draw(0x0004,0,6)
        gl('Disable',None,u)(0x0B71)
        gl('BindFramebuffer',None,u,u)(0x8D40,framebuffer)
        gl('Viewport',None,i,i,i,i)(0,0,256,256)
        gl('UseProgram',None,u)(program)
        for soft in (0,1):
            result = capture(center,along,up,soft)
            key = f'caster-{name}-{soft}'
            metrics[key] = {'shadowed_fraction':float(np.mean(result<250)), 'minimum':int(result.min())}
            Image.fromarray(result).save(args.output/(key+'.png'))
        if name == 'front':
            blocked = capture(center*2,along,up,1)
            assert blocked.max()==0, 'actual caster must shadow a receiver behind it'
    # A separate blocker must project to distance_ratio times its width.
    # Exercise every cube face, nonidentity weighted bones, world translation
    # and close emitters. The expected rectangle follows similar triangles,
    # independently of cube projection or depth reconstruction.
    gl('Uniform2f',None,i,f,f)(uniform('modern_slots'),1,1)
    pixel = (np.arange(256)+.5)*2/256-1
    px, py = np.meshgrid(pixel,pixel)
    expected = (abs(px)<.25/.36)&(abs(py)<.18/.30)
    edge_distance = np.minimum(abs(abs(px)-.25/.36),abs(abs(py)-.18/.30))
    interior = edge_distance>.025
    quad = np.array([[-.25,-.18,1],[.25,-.18,1],[.25,.18,1],
                     [-.25,-.18,1],[.25,.18,1],[-.25,.18,1]],dtype=np.float32)
    gl('BufferData',None,u,C.c_ssize_t,p,u)(0x8892,quad.nbytes,quad.ctypes.data,0x88E0)
    gl('VertexAttrib2f',None,u,f,f)(1,0,1)
    gl('VertexAttrib1f',None,u,f)(2,.35)
    projection_cases = []
    for direction in ((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1),(1,1,1)):
        forward = np.asarray(direction,dtype=float)
        forward /= np.linalg.norm(forward)
        right = np.cross(forward,(0,0,1) if abs(forward[2])<.9 else (0,1,0))
        right /= np.linalg.norm(right)
        basis = np.column_stack((right,np.cross(forward,right),forward))
        for scale, ratio in ((1.,1.5),(1.,4.),(.02,12.)):
            translation = np.array((370.,-693.,65.))
            bones = np.repeat(np.eye(4)[None],2,axis=0)
            bones[0,:3,:3] = basis * scale
            # Different transforms and a nontrivial blend detect ignored
            # weights, swapped node indices and row/column mistakes.
            bones[1,:3,:3] = basis @ np.array([[.9,-.2,0],[.2,1.1,0],[0,0,1.2]]) * scale
            bones[:,:3,3] = translation
            transform = .35*bones[0]+.65*bones[1]
            packed = np.ascontiguousarray(bones.transpose(0,2,1),dtype=np.float32)
            gl('UseProgram',None,u)(caster)
            gl('UniformMatrix4fv',None,i,i,C.c_ubyte,p)(location(caster,b'bones'),2,0,packed.ctypes.data)
            gl('Uniform4f',None,i,f,f,f,f)(location(caster,b'light'),*translation,100)
            gl('BindFramebuffer',None,u,u)(0x8D40,depth_fbo)
            gl('Viewport',None,i,i,i,i)(0,0,size,size)
            gl('Enable',None,u)(0x0B71)
            for face in range(6):
                gl('FramebufferTexture2D',None,u,u,u,u,i)(0x8D40,0x8D00,0x8515+face,cube,0)
                gl('Clear',None,u)(0x00000100)
                uniform1(location(caster,b'face'),face)
                draw(0x0004,0,6)
            gl('Disable',None,u)(0x0B71)
            gl('BindFramebuffer',None,u,u)(0x8D40,framebuffer)
            gl('Viewport',None,i,i,i,i)(0,0,256,256)
            gl('UseProgram',None,u)(program)
            gl('Uniform4f',None,i,f,f,f,f)(uniform('modern_light'),*translation,100)
            center = transform[:3,2]*ratio
            along = transform[:3,0]*(.36*ratio)
            up = transform[:3,1]*(.30*ratio)
            for model in (False,True):
                uniform1(uniform('model_slot'),0 if model else -1)
                for soft in (0,1):
                    result = capture(center+translation if model else center,along,up,soft)
                    blocked = result<128
                    error = float(np.mean(blocked[interior]!=expected[interior]))
                    area_error = abs(float(blocked.mean()-expected.mean()))
                    assert error==0 and area_error<.015, (direction,scale,ratio,model,soft,error,area_error)
                    projection_cases.append(dict(direction=direction,scale=scale,distance_ratio=ratio,
                                                 model=model,soft=soft,interior_error=error,area_error=area_error))
    metrics['projection'] = projection_cases
    assert gl('GetError',u)()==0, 'GPU quality test generated an OpenGL error'
    gl('DisableVertexAttribArray',None,u)(0)
    gl('DeleteBuffers',None,i,C.POINTER(u))(1,C.byref(buffer))
    gl('DeleteFramebuffers',None,i,C.POINTER(u))(1,C.byref(depth_fbo))
    gl('DeleteProgram',None,u)(caster)
    report = {'renderer':gl('GetString',C.c_char_p,u)(0x1F01).decode(), 'size':size,
              'shader_sha256':hashlib.sha256(receiver.encode()).hexdigest(),
              'caster_shader_sha256':hashlib.sha256('\0'.join(stages).encode()).hexdigest(),
              'source_sha256':hashlib.sha256(args.source.read_bytes()).hexdigest(), 'metrics':metrics}
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    gl('DeleteTextures',None,i,C.POINTER(u))(1,C.byref(cube))
    gl('DeleteTextures',None,i,C.POINTER(u))(1,C.byref(target))
    gl('DeleteTextures',None,i,C.POINTER(u))(1,C.byref(other))
    gl('DeleteFramebuffers',None,i,C.POINTER(u))(1,C.byref(framebuffer))
    gl('DeleteVertexArrays',None,i,C.POINTER(u))(1,C.byref(vao))
    gl('DeleteProgram',None,u)(program)
    api(ogl,'wglMakeCurrent',C.c_bool,C.c_void_p,C.c_void_p)(None,None)
    api(ogl,'wglDeleteContext',C.c_bool,C.c_void_p)(context)
    api(user,'ReleaseDC',C.c_int,C.c_void_p,C.c_void_p)(window,dc)
    api(user,'DestroyWindow',C.c_bool,C.c_void_p)(window)
    if not args.record:
        assert all(v['shadowed_fraction']==0 for v in metrics.values() if 'shadowed_fraction' in v), metrics
        assert metrics['soft-edge']['largest_step'] <= 8, metrics
        assert metrics['soft-edge']['levels'] >= 32, metrics
        assert metrics['occluder-seam']['largest_step'] <= 8, metrics
        assert metrics['occluder-seam']['lit']==255 and metrics['occluder-seam']['shadow']==0, metrics
        assert metrics['model-pair']=={'first_shadow':0,'second_lit':255,'disabled_lit':255}, metrics


if __name__ == '__main__':
    main()
