"""Offline, bounded GL stencil comparison using the game's captured posed meshes.

No game code imports this tool. It compares identical opaque triangles and
point lights, with an independent ray/triangle reference. Alpha cutouts and
owner receiver exemptions are reported, not silently claimed as supported.
GPU replay timings are not end-to-end game FPS.
"""
import argparse
import ast
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import struct
import time

import numpy as np
from PIL import Image
if __package__:
    from .modern_shadow_geometry_gpu import Context, run as ray_run
else:
    from modern_shadow_geometry_gpu import Context, run as ray_run

ROOT = Path(__file__).resolve().parents[1]
U, I, P, F = C.c_uint, C.c_int, C.c_void_p, C.c_float


def shadow_near():
    source = (ROOT/'port/linux/src/modern_shadows_gl.c').read_text()
    return float(re.search(r'float n=([\d.e+-]+); float f=light.w', source)[1])


def ray_reference(context, data, copies, repeats):
    # Float Moller-Trumbore can reject BOTH triangles at a shared diagonal.
    # Analytic plate tests caught this; permit roundoff in barycentric tests.
    # This is a numeric reference tolerance, not shadow-edge filtering.
    source=(ROOT/'tools/fixtures/shadow_geometry_visibility.comp').read_text()
    if source.count('u < 0.0 || u > 1.0')!=1 or source.count('v < 0.0 || u + v > 1.0')!=1:
        raise ValueError('Independent ray reference contract changed')
    source=source.replace('u < 0.0 || u > 1.0', 'u < -0.000002 || u > 1.000002')
    source=source.replace('v < 0.0 || u + v > 1.0', 'v < -0.000002 || u + v > 1.000002')
    return ray_run(context,data,copies,repeats,shader_source=source)


def triangle_distance(triangles, point):
    a,b,c=np.asarray(triangles,dtype=np.float64).transpose(1,0,2)
    ab,ac=b-a,c-a
    ap=np.asarray(point)-a
    dot=lambda x,y:np.einsum('ij,ij->i',x,y)
    aa,bb,cc=dot(ab,ab),dot(ab,ac),dot(ac,ac)
    determinant=aa*cc-bb*bb
    safe=np.where(determinant>1e-24,determinant,1)
    u=(cc*dot(ap,ab)-bb*dot(ap,ac))/safe
    v=(aa*dot(ap,ac)-bb*dot(ap,ab))/safe
    plane=a+u[:,None]*ab+v[:,None]*ac
    distance=dot(plane-point,plane-point)
    distance[(determinant<=1e-24)|(u<0)|(v<0)|(u+v>1)]=np.inf
    for start,end in ((a,b),(b,c),(c,a)):
        edge=end-start
        along=np.clip(dot(point-start,edge)/np.maximum(dot(edge,edge),1e-30),0,1)
        delta=start+along[:,None]*edge-point
        distance=np.minimum(distance,dot(delta,delta))
    return np.sqrt(distance)


def captured(root, frame, slot, *, material=None, omit_draws=()):
    near = shadow_near()
    log=(root/'stderr.log').read_text(errors='replace')
    names={int(mesh)-1:shader for mesh,shader in re.findall(r'SHADOW PART model \S+ part \d+ mesh (\d+) region \d+ geometry \d+ shader (.*)',log)}
    raw = (root/'shadow-meshes.bin').read_bytes()
    meshes, offset = {}, 0
    while offset < len(raw):
        if len(raw)-offset < 12:
            raise ValueError('Truncated mesh header')
        key, nv, ni = struct.unpack_from('<3i', raw, offset)
        offset += 12
        if not (0 <= key < 2048 and 0 < nv <= 65536 and 0 < ni <= 196608 and ni % 3 == 0):
            raise ValueError('Invalid mesh dimensions')
        end = offset + nv*32 + ni*2
        if end > len(raw) or key in meshes:
            raise ValueError('Truncated or duplicate mesh')
        vertices = np.frombuffer(raw, '<f4', nv*8, offset).reshape(nv,8)
        indices = np.frombuffer(raw, '<u2', ni, offset+nv*32).reshape(-1,3)
        if indices.max() >= nv or not np.isfinite(vertices).all():
            raise ValueError('Invalid captured mesh')
        meshes[key] = vertices, indices
        offset = end
    draw_path = root/f'shadow-draws-{frame}-{slot}.bin'
    raw = draw_path.read_bytes()
    if len(raw) < 20:
        raise ValueError('Missing draw header')
    light = np.frombuffer(raw, '<f4', 4).copy()
    count, = struct.unpack_from('<i', raw, 16)
    if not 0 <= count <= 256 or len(raw) != 20 + count*4144 or not np.isfinite(light).all():
        raise ValueError('Invalid captured draw list')
    triangles, alpha_triangles, opaque_alpha_triangles, nearest = [], 0, 0, []
    for index in range(count):
        if index in omit_draws:
            continue
        start = 20 + index*4144
        key, nodes, views = struct.unpack_from('<3i', raw, start)
        if key not in meshes or not 0 < nodes <= 64:
            raise ValueError('Invalid draw mesh/nodes')
        v, ix = meshes[key]
        if material is not None and names.get(key) != material:
            continue
        if not views & 1:
            continue
        alpha, = struct.unpack_from('<I', raw, start+4140)
        if alpha:
            texture = root/f'shadow-alpha-{alpha}.bin'
            opaque = False
            if texture.exists():
                payload = texture.read_bytes()
                if len(payload)<8:raise ValueError('Truncated alpha texture')
                w,h = struct.unpack_from('<2i',payload)
                if not 0<w<=4096 or not 0<h<=4096 or len(payload)!=8+w*h*4:
                    raise ValueError('Invalid captured alpha texture')
                opaque = bool((np.frombuffer(payload,np.uint8,offset=8).reshape(-1,4)[:,3]>127).all())
            if not opaque:
                alpha_triangles += len(ix)
                continue
            opaque_alpha_triangles += len(ix)
        if (v[:,3:5] < 0).any() or (v[:,3:5] >= nodes).any() or (v[:,5] < 0).any() or (v[:,5] > 1).any():
            raise ValueError('Invalid skinning input')
        poses = np.frombuffer(raw, '<f4', 1024, start+12).reshape(64,4,4).transpose(0,2,1)
        positions = np.c_[v[:,:3], np.ones(len(v), dtype=np.float32)]
        posed = sum(np.einsum('nij,nj->ni', poses[v[:,n].astype(int)], positions)[:,:3]*w
                    for n,w in ((3,v[:,5,None]), (4,1-v[:,5,None])))
        triangles.append(posed[ix])
        distances=triangle_distance(posed[ix],light[:3])
        nearest.append(dict(mesh=key,shader=names.get(key,'world/unnamed'),
                            distance=float(distances.min()),near_plane_triangles=int(np.sum(distances<near))))
    if material is not None and len(triangles) != 1:
        raise ValueError(f'Expected one posed draw for {material}, found {len(triangles)}')
    if not triangles:
        raise ValueError('No opaque world-view triangles')
    triangles = np.concatenate(triangles).astype(np.float32)
    good = np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0], triangles[:,2]-triangles[:,0]), axis=1) > 1e-10
    triangles = triangles[good]
    return triangles, light, dict(draws=count, opaque_triangles=len(triangles),
        omitted_alpha_triangles=alpha_triangles, verified_opaque_alpha_triangles=opaque_alpha_triangles,
        nearest_parts=sorted(nearest,key=lambda x:x['distance'])[:8],
        mesh_sha256=hashlib.sha256((root/'shadow-meshes.bin').read_bytes()).hexdigest(),
        draw_sha256=hashlib.sha256(raw).hexdigest())


def volume(triangles, light):
    """Closed triangle prisms with exact coincident side cancellation.

    Each triangle is two-sided, as in the map pass. Cancel only identical
    opposite edges; do not approximately weld or assume a watertight model.
    Remaining edges may have multiplicity (closed meshes commonly give two).
    Infinite far vertices plus depth clamp make camera-inside z-fail robust.
    """
    t = np.asarray(triangles, np.float32).copy()
    light = np.asarray(light, np.float32)
    if t.ndim!=3 or t.shape[1:]!=(3,3) or not len(t) or light.shape!=(3,) or not np.isfinite(t).all() or not np.isfinite(light).all():
        raise ValueError('Expected finite triangles and point source')
    n = np.cross(t[:,1]-t[:,0], t[:,2]-t[:,0])
    flip = np.einsum('ij,ij->i', n, light-t[:,0]) < 0
    t[flip] = t[flip][:, [0,2,1]]
    vertices, ids = np.unique(t.reshape(-1,3), axis=0, return_inverse=True)
    ids = ids.reshape(-1,3)
    edges = np.concatenate([ids[:,[0,1]], ids[:,[1,2]], ids[:,[2,0]]])
    ordered = np.sort(edges, axis=1)
    unique, inverse = np.unique(ordered, axis=0, return_inverse=True)
    counts = np.bincount(inverse, weights=np.where(edges[:,0] < edges[:,1], 1, -1)).astype(int)
    keep = counts != 0
    remaining = np.repeat(unique[keep], np.abs(counts[keep]), axis=0)
    negative = np.repeat(counts[keep] < 0, np.abs(counts[keep]))
    remaining[negative] = remaining[negative][:, ::-1]
    direction=vertices-light
    distance=np.linalg.norm(direction,axis=1)
    if np.any(distance<1e-12):raise ValueError('Source coincides with caster vertex')
    # Match the ray endpoint/map minimum tolerance, including surfaces viewed
    # from their unlit side. Radial cap displacement leaves silhouette rays
    # unchanged; camera-depth polygon offset cannot satisfy both orientations.
    near = np.c_[vertices+direction*(.0005/distance)[:,None], np.ones(len(vertices), np.float32)]
    far = np.c_[vertices-light, np.zeros(len(vertices), np.float32)]
    a,b = remaining.T
    sides = np.stack([near[b], near[a], far[a], near[b], far[a], far[b]], axis=1)
    result = np.concatenate([near[ids].reshape(-1,4), far[ids[:,::-1]].reshape(-1,4), sides.reshape(-1,4)])
    return np.ascontiguousarray(result, dtype=np.float32), dict(
        input_triangles=len(t), retained_side_edges=len(remaining),
        volume_triangles=len(result)//3, max_edge_multiplicity=int(np.abs(counts).max(initial=0)))


def camera_matrix(eye, target, aspect):
    f = np.asarray(target)-eye
    f /= np.linalg.norm(f)
    up = np.array([0.,0.,1.])
    if abs(f[2]) > .99:
        up = np.array([0.,1.,0.])
    right = np.cross(f, up); right /= np.linalg.norm(right)
    up = np.cross(right, f)
    view = np.eye(4); view[:3,:3] = [right, up, -f]
    view[:3,3] = -view[:3,:3] @ eye
    near, far = .01, 2000.
    scale = 1/np.tan(np.radians(65)/2)
    projection = np.array([[scale/aspect,0,0,0],[0,scale,0,0],
                           [0,0,-(far+near)/(far-near),-2*far*near/(far-near)], [0,0,-1,0]])
    return projection @ view


class Replay:
    def __init__(self, context, width, height):
        self.c, self.gl, self.width, self.height = context, context.gl, width, height
        self.objects = {'Framebuffers': [], 'Renderbuffers': [], 'Textures': [], 'VertexArrays': []}
        self.fbo = self.new('Framebuffers')
        self.gl('BindFramebuffer', None,U,U)(0x8D40,self.fbo)
        self.color = self.new('Textures')
        self.gl('BindTexture',None,U,U)(0x0DE1,self.color)
        self.gl('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x0DE1,0,0x8814,width,height,0,0x1908,0x1406,None)
        self.gl('TexParameteri',None,U,U,I)(0x0DE1,0x2801,0x2600)
        self.gl('FramebufferTexture2D',None,U,U,U,U,I)(0x8D40,0x8CE0,0x0DE1,self.color,0)
        depth = self.new('Renderbuffers')
        self.gl('BindRenderbuffer',None,U,U)(0x8D41,depth)
        self.gl('RenderbufferStorage',None,U,U,I,I)(0x8D41,0x8CAD,width,height)
        self.gl('FramebufferRenderbuffer',None,U,U,U,U)(0x8D40,0x821A,0x8D41,depth)
        if self.gl('CheckFramebufferStatus',U,U)(0x8D40) != 0x8CD5:
            raise RuntimeError('Incomplete depth/stencil framebuffer')
        self.vao = self.new('VertexArrays')
        self.gl('BindVertexArray',None,U)(self.vao)
        vs = '#version 330 core\nlayout(location=0) in vec4 position; uniform mat4 mvp; out vec3 world; void main(){world=position.xyz;gl_Position=mvp*position;}'
        self.positions = self.program(vs, '#version 330 core\nin vec3 world;out vec4 color;void main(){color=vec4(world,1);}')
        self.solid = self.program(vs, '#version 330 core\nout vec4 color;void main(){color=vec4(0,0,0,1);}')
        source = (ROOT/'port/linux/src/modern_shadows_gl.c').read_text()
        body = source.split('void modern_shadow_emit(',1)[1].split('void modern_shadow_uniforms_initialize',1)[0]
        shader = ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', body))
        self.compare = self.program(vs, '#version 330 core\nin vec3 world;out vec4 color;\n'+shader+
            '\nvoid main(){float v=modern_visibility(world-modern_light.xyz);color=vec4(v,v,v,1);}')
        cube_vs = '#version 330 core\nlayout(location=0)in vec4 position;uniform vec4 light;uniform int face;void main(){vec3 d=position.xyz-light.xyz;vec3 v;if(face==0)v=vec3(-d.z,-d.y,d.x);else if(face==1)v=vec3(d.z,-d.y,-d.x);else if(face==2)v=vec3(d.x,d.z,d.y);else if(face==3)v=vec3(d.x,-d.z,-d.y);else if(face==4)v=vec3(d.x,-d.y,d.z);else v=vec3(-d.x,-d.y,-d.z);float n=.005;gl_Position=vec4(v.xy,n*(light.w-v.z)/(light.w-n),v.z);}'
        cube_vs = cube_vs.replace('float n=.005;', f'float n={shadow_near():.8f};')
        self.cube_program = self.program(cube_vs, '#version 330 core\nvoid main(){}')
        self.cube_fbo = self.new('Framebuffers')
        self.cube = self.new('Textures')
        self.c.check()

    def new(self, kind):
        obj=U();self.gl('Gen'+kind,None,I,C.POINTER(U))(1,C.byref(obj))
        self.objects[kind].append(obj.value)
        return obj.value

    def program(self, vertex, fragment, geometry=None):
        shaders=[]
        program=self.gl('CreateProgram',U)();self.c.programs.append(program)
        try:
            stages = [(0x8B31,vertex),(0x8B30,fragment)]
            if geometry is not None: stages.append((0x8DD9,geometry))
            for kind, text in stages:
                shader=self.gl('CreateShader',U,U)(kind);shaders.append(shader)
                data=C.c_char_p(text.encode())
                self.gl('ShaderSource',None,U,I,C.POINTER(C.c_char_p),P)(shader,1,C.byref(data),None)
                self.gl('CompileShader',None,U)(shader)
                status=I();self.gl('GetShaderiv',None,U,U,C.POINTER(I))(shader,0x8B81,C.byref(status))
                if not status.value:
                    log=C.create_string_buffer(16000);self.gl('GetShaderInfoLog',None,U,I,P,C.c_char_p)(shader,len(log),None,log)
                    raise RuntimeError(log.value.decode())
                self.gl('AttachShader',None,U,U)(program,shader)
            self.gl('LinkProgram',None,U)(program)
            status=I();self.gl('GetProgramiv',None,U,U,C.POINTER(I))(program,0x8B82,C.byref(status))
            if not status.value:
                raise RuntimeError('Replay program link failed')
            return program
        finally:
            for shader in shaders:self.gl('DeleteShader',None,U)(shader)

    def uniform(self, program, name, values):
        location=self.gl('GetUniformLocation',I,U,C.c_char_p)(program,name.encode())
        if np.isscalar(values):self.gl('Uniform1i',None,I,I)(location,int(values))
        else:
            values=np.ascontiguousarray(values,dtype=np.float32)
            if values.shape==(4,4):self.gl('UniformMatrix4fv',None,I,I,U,P)(location,1,1,values.ctypes.data)
            else:self.gl('Uniform4fv',None,I,I,P)(location,1,values.ctypes.data)

    def upload(self, values):
        values=np.ascontiguousarray(values,dtype=np.float32)
        obj=self.c.buffer(0,values.tobytes())
        return obj,len(values)

    def draw(self, program, mesh, matrix):
        self.gl('UseProgram',None,U)(program)
        self.uniform(program,'mvp',matrix)
        self.gl('BindVertexArray',None,U)(self.vao)
        self.gl('BindBuffer',None,U,U)(0x8892,mesh[0])
        self.gl('EnableVertexAttribArray',None,U)(0)
        self.gl('VertexAttribPointer',None,U,I,U,U,I,P)(0,4,0x1406,0,16,None)
        self.gl('DrawArrays',None,U,I,I)(4,0,mesh[1])

    def begin(self):
        g=self.gl
        g('BindFramebuffer',None,U,U)(0x8D40,self.fbo)
        g('Viewport',None,I,I,I,I)(0,0,self.width,self.height)
        g('ClipControl',None,U,U)(0x8CA1,0x935E)
        for flag in (0x0B44,0x0BE2,0x0B90,0x0C11):g('Disable',None,U)(flag)
        g('Enable',None,U)(0x0B71)
        g('DepthFunc',None,U)(0x0201);g('DepthMask',None,U)(1)
        g('ColorMask',None,U,U,U,U)(1,1,1,1)
        g('ClearColor',None,F,F,F,F)(1,1,1,0)
        g('ClearDepth',None,C.c_double)(1)
        g('StencilMask',None,U)(255)
        g('ClearStencil',None,I)(0)
        g('Clear',None,U)(0x4000|0x0100|0x0400)

    def read(self):
        pixels=np.empty((self.height,self.width,4),np.float32)
        self.gl('ReadPixels',None,I,I,I,I,U,U,P)(0,0,self.width,self.height,0x1908,0x1406,pixels.ctypes.data)
        self.c.check()
        return pixels[::-1].copy()

    def stencil(self, receiver, extrusion, matrix):
        self.begin();g=self.gl
        # Write receiver depth and white visibility, then count closed volumes.
        self.draw(self.positions,receiver,matrix)
        g('Clear',None,U)(0x4000)
        g('DepthMask',None,U)(0);g('ColorMask',None,U,U,U,U)(0,0,0,0)
        g('Enable',None,U)(0x0B90);g('Enable',None,U)(0x864F)
        g('StencilFunc',None,U,I,U)(0x0207,0,255)
        g('StencilOpSeparate',None,U,U,U,U)(0x0404,0x1E00,0x8508,0x1E00)
        g('StencilOpSeparate',None,U,U,U,U)(0x0405,0x1E00,0x8507,0x1E00)
        self.draw(self.solid,extrusion,matrix)
        g('Disable',None,U)(0x864F)
        g('ColorMask',None,U,U,U,U)(1,1,1,1)
        g('StencilFunc',None,U,I,U)(0x0205,0,255)
        g('StencilOpSeparate',None,U,U,U,U)(0x0408,0x1E00,0x1E00,0x1E00)
        g('DepthFunc',None,U)(0x0203)
        self.draw(self.solid,receiver,matrix)
        g('Disable',None,U)(0x0B90)

    def shadowmap(self, receiver, light, matrix, resolution):
        g=self.gl
        g('BindTexture',None,U,U)(0x8513,self.cube)
        if getattr(self,'resolution',None)!=resolution:
            for face in range(6):g('TexImage2D',None,U,I,I,I,I,I,U,U,P)(0x8515+face,0,0x8CAC,resolution,resolution,0,0x1902,0x1406,None)
            for name,value in ((0x2800,0x2600),(0x2801,0x2600),(0x2802,0x812F),(0x2803,0x812F),(0x8072,0x812F)):
                g('TexParameteri',None,U,U,I)(0x8513,name,value)
            self.resolution=resolution
        self.begin()
        g('BindFramebuffer',None,U,U)(0x8D40,self.cube_fbo)
        g('DrawBuffer',None,U)(0);g('ReadBuffer',None,U)(0)
        g('Viewport',None,I,I,I,I)(0,0,resolution,resolution)
        g('ClipControl',None,U,U)(0x8CA1,0x935F)
        g('DepthFunc',None,U)(0x0204);g('ClearDepth',None,C.c_double)(0)
        g('ColorMask',None,U,U,U,U)(0,0,0,0)
        g('UseProgram',None,U)(self.cube_program);self.uniform(self.cube_program,'light',light)
        for face in range(6):
            g('FramebufferTexture2D',None,U,U,U,U,I)(0x8D40,0x8D00,0x8515+face,self.cube,0)
            g('Clear',None,U)(0x0100)
            self.uniform(self.cube_program,'face',face)
            self.draw(self.cube_program,self.face_meshes[face],matrix)
        self.begin()
        g('ActiveTexture',None,U)(0x84C0);g('BindTexture',None,U,U)(0x8513,self.cube)
        g('UseProgram',None,U)(self.compare)
        self.uniform(self.compare,'modern_light',light)
        self.uniform(self.compare,'modern_depth',0)
        self.uniform(self.compare,'modern_soft',0)
        self.draw(self.compare,receiver,matrix)

    def timings(self, call, repeats):
        if hasattr(self, 'native'):
            call = self.native_record(call)
        query=U();self.gl('GenQueries',None,I,C.POINTER(U))(1,C.byref(query));self.c.queries.append(query.value)
        times=[]
        for index in range(repeats+3):
            self.gl('BeginQuery',None,U,U)(0x88BF,query.value)
            call()
            self.gl('EndQuery',None,U)(0x88BF)
            elapsed=C.c_uint64();self.gl('GetQueryObjectui64v',None,U,U,C.POINTER(C.c_uint64))(query.value,0x8866,C.byref(elapsed))
            if index>=3:times.append(elapsed.value/1e6)
        self.c.check()
        return dict(gpu_ms=times,median_ms=float(np.median(times)))

    def native_record(self, call):
        class Command(C.Structure):
            _fields_=[('function',P),('kind',C.c_uint64),('a',C.c_uint64*8)]
        commands,storage=[],[]
        original=self.gl
        def record(name,result,*types):
            fn=original(name,result,*types)
            if result is not None:
                return fn
            def append(*args):
                values=list(args)
                kind=len(values)
                if name=='ClearColor':
                    kind=9;values=[struct.unpack('<I',struct.pack('<f',x))[0] for x in values]
                elif name=='ClearDepth':
                    kind=10;values=[struct.unpack('<Q',struct.pack('<d',values[0]))[0]]
                elif name in ('Uniform4fv','UniformMatrix4fv'):
                    kind=6 if name=='Uniform4fv' else 7
                    payload=C.create_string_buffer(C.string_at(values[-1],values[1]*(16 if kind==6 else 64)))
                    storage.append(payload);values[-1]=C.addressof(payload)
                elif name=='VertexAttribPointer':kind=8
                elif name not in {'BindFramebuffer','Viewport','ClipControl','Disable','Enable','DepthFunc','DepthMask',
                                  'ColorMask','Clear','StencilMask','ClearStencil','StencilFunc','StencilOpSeparate',
                                  'UseProgram','Uniform1i','BindVertexArray','BindBuffer','EnableVertexAttribArray',
                                  'DrawArrays','ActiveTexture','BindTexture','DrawBuffer','ReadBuffer','FramebufferTexture2D'}:
                    raise ValueError('Unsupported replay command '+name)
                commands.append(Command(C.cast(fn,P).value,kind,(C.c_uint64*8)(*[(x or 0)&0xffffffffffffffff for x in values])))
            return append
        self.gl=record
        try:call()
        finally:self.gl=original
        packed=(Command*len(commands))(*commands)
        native=self.native.shadow_replay
        native.argtypes=[P,I];native.restype=None
        def run():
            # Closure owns all copied uniform payloads and function pointers.
            assert storage or commands
            native(packed,len(commands))
        return run

    def close(self):
        for kind,values in self.objects.items():
            if values:self.gl('Delete'+kind,None,I,C.POINTER(U))(len(values),(U*len(values))(*values))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    parser.add_argument('--frame',type=int,default=180)
    parser.add_argument('--slot',type=int,default=0)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--size',default='960x540')
    parser.add_argument('--repeats',type=int,default=20)
    parser.add_argument('--native',type=Path,help='Native command submission DLL for comparable GPU timings')
    args=parser.parse_args()
    width,height=map(int,args.size.split('x'))
    if not 32<=width<=3840 or not 32<=height<=2160 or not 1<=args.repeats<=100:
        parser.error('Invalid replay bounds')
    t,light,report=captured(args.capture,args.frame,args.slot)
    # Translate before GPU work to avoid precision loss on distant map origins.
    center=light[:3].copy();t-=center;light[:3]=0
    log=(args.capture/'stderr.log').read_text(errors='replace')
    matches=re.findall(r'SHADOW SCENE created .*? camera ([-\d.]+) ([-\d.]+) ([-\d.]+) forward ([-\d.]+) ([-\d.]+) ([-\d.]+)',log)
    if not matches:
        matches=re.findall(r'PLASMA camera ([-\d.]+) ([-\d.]+) ([-\d.]+) forward ([-\d.]+) ([-\d.]+) ([-\d.]+)',log)
    camera_path=args.capture/f'shadow-camera-{args.frame}-{args.slot}.bin'
    if not camera_path.exists():camera_path=args.capture/f'shadow-camera-{args.frame}.bin'
    if camera_path.exists():
        values=np.frombuffer(camera_path.read_bytes(),'<f4').astype(float)
        if values.shape!=(6,) or not np.isfinite(values).all():raise ValueError('Invalid captured camera')
        eye=values[:3]-center;target=eye+values[3:]
        camera_source='captured camera'
    elif matches:
        values=np.array(matches[-1],dtype=float);eye=values[:3]-center;target=eye+values[3:]
        camera_source='fixture log'
    else:
        eye=np.array([3.,-3.,2.]);target=np.zeros(3)
        camera_source='synthetic fallback'
    matrix=camera_matrix(eye,target,width/height)
    start=time.perf_counter();v,stats=volume(t,light[:3]);report.update(stats)
    report['python_volume_build_ms']=(time.perf_counter()-start)*1000
    args.output.mkdir(parents=True,exist_ok=True)
    context=Context();replay=None
    try:
        replay=Replay(context,width,height)
        if args.native:replay.native=C.CDLL(str(args.native.resolve()))
        mesh=replay.upload(np.c_[t.reshape(-1,3),np.ones(t.size//3)])
        # Give the map replay conservative per-triangle face culling. This is
        # a GPU kernel comparison; CPU preparation costs of BOTH techniques
        # remain outside the timer, and are not an end-to-end engine estimate.
        low,high=t.min(axis=1),t.max(axis=1)
        replay.face_meshes=[]
        for face in range(6):
            axis=face//2;depth=-low[:,axis] if face&1 else high[:,axis]
            accepted=depth>=shadow_near()
            for other in range(3):
                if other!=axis:accepted &= (depth+high[:,other]>=0)&(depth-low[:,other]>=0)
            vertices=t[accepted].reshape(-1,3)
            replay.face_meshes.append(replay.upload(np.c_[vertices,np.ones(len(vertices))]))
        extrusion=replay.upload(v)
        replay.begin();replay.draw(replay.positions,mesh,matrix);positions=replay.read()
        valid=(positions[:,:,3]>.5)&(np.linalg.norm(positions[:,:,:3],axis=2)<light[3])
        receivers=positions[:,:,:3][valid]
        # Match the map comparator's minimum endpoint tolerance. Interpolated
        # float receiver positions are not exact points on their source plane.
        receivers=receivers-receivers/np.linalg.norm(receivers,axis=1)[:,None]*.0005
        if not len(receivers):raise RuntimeError('Camera sees no in-range receivers')
        reference,_=ray_reference(context,dict(triangles=t,sources=light[:3][None],receivers=receivers,full=np.zeros((1,len(receivers)))),1,1)
        truth=np.ones((height,width));truth[valid]=reference.reshape(-1)
        masks={}
        for name,call in [('stencil',lambda: replay.stencil(mesh,extrusion,matrix)),
                          ('shadowmap1024',lambda: replay.shadowmap(mesh,light,matrix,1024)),
                          ('shadowmap4096',lambda: replay.shadowmap(mesh,light,matrix,4096))]:
            call();pixels=replay.read();mask=pixels[:,:,0]>.5;masks[name]=mask
            report[name]=replay.timings(call,args.repeats)
            if not np.array_equal(replay.read(),pixels):
                raise RuntimeError('Native replay changed rendering output')
            report[name]['disagreement_percent']=float(100*np.mean(mask[valid]!=(truth[valid]>.5)))
            report[name]['shadow_percent']=float(100*np.mean(~mask[valid]))
            expected=(truth[valid]<.5);actual=~mask[valid]
            report[name]['shadow_iou']=float(np.sum(expected&actual)/max(1,np.sum(expected|actual)))
            report[name]['missed_shadow_percent']=float(100*np.sum(expected&~actual)/max(1,np.sum(expected)))
            image=np.full((height,width,3),40,np.uint8)
            image[valid]=np.where(mask[valid,None],220,60)
            image[valid & (mask!=(truth>.5))]=[230,65,65]
            Image.fromarray(image).save(args.output/(name+'.png'))
        report.update(renderer=context.info,receiver_pixels=int(valid.sum()),light_radius=float(light[3]),
            native_submission=bool(args.native),camera_source=camera_source,vertical_fov_degrees=65,
            tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            shader_source_sha256=hashlib.sha256((ROOT/'port/linux/src/modern_shadows_gl.c').read_bytes()).hexdigest(),
            native_sha256=hashlib.sha256(args.native.read_bytes()).hexdigest() if args.native else None,
            camera_sha256=hashlib.sha256(camera_path.read_bytes()).hexdigest() if camera_path.exists() else None,
            resolution=[width,height],reference_shadow_percent=float(100*np.mean(truth[valid]<.5)),
            scope='Opaque captured geometry replay, not full game FPS; no alpha cutouts or receiver-owner exceptions')
        (args.output/'report.json').write_text(json.dumps(report,indent=2))
        np.savez_compressed(args.output/'masks.npz',valid=valid,reference=truth,**masks)
        print(json.dumps({key:value for key,value in report.items() if key not in ('stencil','shadowmap1024','shadowmap4096')},indent=2))
    finally:
        if replay:replay.close()
        context.close()


if __name__=='__main__':main()
