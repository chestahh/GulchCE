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
    args = parser.parse_args()
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
    body = source.split('void modern_shadow_emit(struct xgpu_text *text)', 1)[1].split('\nvoid modern_shadow_uniforms_initialize', 1)[0]
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    receiver = ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', body))
    vertex = '''#version 330 core
    out vec2 uv; void main() {
      uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;
      gl_Position=vec4(uv,0,1);
    }'''
    fragment = '#version 330 core\n' + receiver + '''
    in vec2 uv; out vec4 color;
    uniform vec3 center, along, up;
    void main() { color=vec4(vec3(modern_visibility(center+uv.x*along+uv.y*up)),1); }
    '''
    program = gl('CreateProgram', u)()
    attach = gl('AttachShader', None, u, u)
    for kind, text in ((0x8B31, vertex), (0x8B30, fragment)):
        obj = compile_stage(kind, text); attach(program, obj)
        gl('DeleteShader', None, u)(obj)
    gl('LinkProgram', None, u)(program)
    status = i(); gl('GetProgramiv', None, u, u, C.POINTER(i))(program, 0x8B82, C.byref(status))
    assert status.value, 'receiver link failed'
    gl('UseProgram', None, u)(program)
    location = gl('GetUniformLocation', i, u, C.c_char_p)
    def uniform(name): return location(program, name.encode())
    uniform3 = gl('Uniform3f', None, i, f, f, f)
    gl('Uniform4f', None, i, f, f, f, f)(uniform('modern_light'), 0, 0, 0, 100)
    uniform1 = gl('Uniform1i', None, i, i)
    uniform1(uniform('modern_depth'), 0)
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
    s, t = np.meshgrid((np.arange(512)+.5)/256-1, (np.arange(512)+.5)/256-1)
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
            data = np.ascontiguousarray(np.clip(depth/100,0,1), dtype=np.float32)
            upload(0x8515+face,0,0x8CAC,512,512,0,0x1902,0x1406,data.ctypes.data)

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
    edge = capture((0,0,5),(.05,0,0),(0,.01,0),1)
    Image.fromarray(edge).save(args.output/'soft-edge.png')
    metrics['soft-edge'] = {'largest_step':int(np.abs(np.diff(edge[128].astype(int))).max()),
                            'levels':int(len(np.unique(edge[128])))}
    # Hard remains binary, and both modes retain fully shadowed and lit
    # interiors. A shader returning white everywhere cannot pass this test.
    hard = capture((0,0,5),(.05,0,0),(0,.01,0),0)
    assert set(np.unique(hard)) == {0,255}
    assert edge[128,0] == 255 and edge[128,-1] == 0
    depth_plane(np.array((1,0,1)),10,np.array((0,1,0)))
    seam = capture((5,0,5),(1,0,-1),(0,.08,0),1)
    Image.fromarray(seam).save(args.output/'occluder-seam.png')
    metrics['occluder-seam'] = {'largest_step':int(np.abs(np.diff(seam.astype(int),axis=1)).max()),
                               'lit':int(seam[:8].min()), 'shadow':int(seam[-8:].max())}
    report = {'renderer':gl('GetString',C.c_char_p,u)(0x1F01).decode(),
              'shader_sha256':hashlib.sha256(receiver.encode()).hexdigest(), 'metrics':metrics}
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    gl('DeleteTextures',None,i,C.POINTER(u))(1,C.byref(cube))
    gl('DeleteTextures',None,i,C.POINTER(u))(1,C.byref(target))
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


if __name__ == '__main__':
    main()
