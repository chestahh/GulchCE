"""Bounded Windows GL geometry-visibility prototype, never linked into the game.

Consumes the offline surface probe's reference.npz. It tests local opaque
geometry only, not full-scene shadow composition or game performance.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import time

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SHADER = ROOT / 'tools/fixtures/shadow_geometry_visibility.comp'


def build_tree(triangles):
    """Pack a depth-first BVH with escape links; traversal needs no GPU stack."""
    triangles = np.asarray(triangles, dtype=np.float32)
    if triangles.ndim != 3 or triangles.shape[1:] != (3, 3) or not len(triangles):
        raise ValueError('Expected nonempty N x 3 x 3 triangle array')
    if not np.isfinite(triangles).all():
        raise ValueError('Nonfinite triangles')
    nodes, ordered = [], []

    def visit(ids):
        index = len(nodes)
        points = triangles[ids]
        low, high = points.min(axis=(0, 1)), points.max(axis=(0, 1))
        nodes.append(None)
        start, count = 0, 0
        if len(ids) <= 8:
            start, count = len(ordered), len(ids)
            ordered.extend(ids)
        else:
            axis = (high - low).argmax()
            ids = ids[np.argsort(points.mean(axis=1)[:, axis], kind='stable')]
            middle = len(ids) // 2
            visit(ids[:middle])
            visit(ids[middle:])
        nodes[index] = struct.pack('<8f4i', *low, 0, *high, 0,
                                   start, count, len(nodes), 0)

    visit(np.arange(len(triangles)))
    packed = np.zeros((len(triangles), 3, 4), dtype=np.float32)
    posed = triangles[ordered]
    packed[:, 0, :3] = posed[:, 0]
    packed[:, 1, :3] = posed[:, 1] - posed[:, 0]
    packed[:, 2, :3] = posed[:, 2] - posed[:, 0]
    return b''.join(nodes), packed.tobytes()


def vectors(values):
    values = np.asarray(values, dtype=np.float32).reshape(-1, 3)
    if not len(values) or not np.isfinite(values).all():
        raise ValueError('Expected finite nonempty vectors')
    result = np.zeros((len(values), 4), dtype=np.float32)
    result[:, :3] = values
    return result.tobytes()


class Context:
    """Hidden WGL context owned by this diagnostic process."""
    def __init__(self):
        self.user, self.gdi, self.ogl = (C.WinDLL(n) for n in ('user32', 'gdi32', 'opengl32'))
        self.window = self.dc = self.context = None
        self.buffers, self.programs, self.queries = [], [], []
        try:
            self.window = self.api(self.user, 'CreateWindowExW', C.c_void_p,
                C.c_uint, C.c_wchar_p, C.c_wchar_p, C.c_uint,
                C.c_int, C.c_int, C.c_int, C.c_int,
                C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p)(
                    0, 'STATIC', 'Geometry visibility diagnostic', 0,
                    0, 0, 64, 64, None, None, None, None)
            if not self.window:
                raise RuntimeError('Cannot create hidden window')
            self.dc = self.api(self.user, 'GetDC', C.c_void_p, C.c_void_p)(self.window)
            descriptor = bytearray(40)
            struct.pack_into('<HHI', descriptor, 0, 40, 1, 0x24)
            descriptor[9] = 32
            pixel_format = C.create_string_buffer(bytes(descriptor))
            index = self.api(self.gdi, 'ChoosePixelFormat', C.c_int, C.c_void_p, C.c_void_p)(self.dc, pixel_format)
            if not self.api(self.gdi, 'SetPixelFormat', C.c_int, C.c_void_p, C.c_int, C.c_void_p)(self.dc, index, pixel_format):
                raise RuntimeError('Cannot select GL pixel format')
            self.context = self.api(self.ogl, 'wglCreateContext', C.c_void_p, C.c_void_p)(self.dc)
            if not self.api(self.ogl, 'wglMakeCurrent', C.c_int, C.c_void_p, C.c_void_p)(self.dc, self.context):
                raise RuntimeError('Cannot activate GL context')
            self.proc = self.api(self.ogl, 'wglGetProcAddress', C.c_void_p, C.c_char_p)
            self.info = {name: self.gl('GetString', C.c_char_p, C.c_uint)(key).decode()
                         for name, key in (('vendor', 0x1F00), ('renderer', 0x1F01), ('version', 0x1F02))}
        except Exception:
            self.close()
            raise

    @staticmethod
    def api(library, name, result, *args):
        function = getattr(library, name)
        function.restype, function.argtypes = result, args
        return function

    def gl(self, name, result, *args):
        name = 'gl' + name
        if hasattr(self.ogl, name):
            return self.api(self.ogl, name, result, *args)
        address = self.proc(name.encode())
        if address in (None, 0, 1, 2, 3, C.c_void_p(-1).value):
            raise RuntimeError('Unavailable GL entry point: ' + name)
        return C.WINFUNCTYPE(result, *args)(address)

    def check(self):
        error = self.gl('GetError', C.c_uint)()
        if error:
            raise RuntimeError(f'GL error 0x{error:x}')

    def compile(self, source):
        u, i, p = C.c_uint, C.c_int, C.c_void_p
        shader = self.gl('CreateShader', u, u)(0x91B9)
        try:
            text = C.c_char_p(source.encode())
            self.gl('ShaderSource', None, u, i, C.POINTER(C.c_char_p), p)(shader, 1, C.byref(text), None)
            self.gl('CompileShader', None, u)(shader)
            status = i()
            self.gl('GetShaderiv', None, u, u, C.POINTER(i))(shader, 0x8B81, C.byref(status))
            log = C.create_string_buffer(16384)
            self.gl('GetShaderInfoLog', None, u, i, p, C.c_char_p)(shader, len(log), None, log)
            if not status.value:
                raise RuntimeError(log.value.decode())
            program = self.gl('CreateProgram', u)()
            self.programs.append(program)
            self.gl('AttachShader', None, u, u)(program, shader)
            self.gl('LinkProgram', None, u)(program)
            self.gl('GetProgramiv', None, u, u, C.POINTER(i))(program, 0x8B82, C.byref(status))
            self.gl('GetProgramInfoLog', None, u, i, p, C.c_char_p)(program, len(log), None, log)
            if not status.value:
                raise RuntimeError(log.value.decode())
            return program
        finally:
            self.gl('DeleteShader', None, u)(shader)

    def buffer(self, binding, data):
        obj = C.c_uint()
        self.gl('GenBuffers', None, C.c_int, C.POINTER(C.c_uint))(1, C.byref(obj))
        self.buffers.append(obj.value)
        self.gl('BindBuffer', None, C.c_uint, C.c_uint)(0x90D2, obj.value)
        contents = C.create_string_buffer(data)
        self.gl('BufferData', None, C.c_uint, C.c_ssize_t, C.c_void_p, C.c_uint)(
            0x90D2, len(data), contents, 0x88E4)
        self.gl('BindBufferBase', None, C.c_uint, C.c_uint, C.c_uint)(0x90D2, binding, obj.value)
        self.check()
        return obj.value

    def close(self):
        if self.context and hasattr(self, 'proc'):
            for name, values in (('Buffers', self.buffers), ('Queries', self.queries)):
                if values:
                    objects = (C.c_uint * len(values))(*values)
                    self.gl('Delete' + name, None, C.c_int, C.POINTER(C.c_uint))(len(values), objects)
            for program in self.programs:
                self.gl('DeleteProgram', None, C.c_uint)(program)
        if self.context:
            self.api(self.ogl, 'wglMakeCurrent', C.c_int, C.c_void_p, C.c_void_p)(None, None)
            self.api(self.ogl, 'wglDeleteContext', C.c_int, C.c_void_p)(self.context)
            self.context = None
        if self.dc:
            self.api(self.user, 'ReleaseDC', C.c_int, C.c_void_p, C.c_void_p)(self.window, self.dc)
            self.dc = None
        if self.window:
            self.api(self.user, 'DestroyWindow', C.c_int, C.c_void_p)(self.window)
            self.window = None


def run(context, data, copies, repeats, packet=False, shader_source=None):
    start = time.perf_counter()
    nodes, triangles = build_tree(data['triangles'])
    build_ms = (time.perf_counter() - start) * 1000
    receivers = data['receivers'].reshape(-1, 3)
    count, sources = len(receivers), data['sources']
    shader = SHADER.read_text() if shader_source is None else shader_source
    if packet:
        shader = shader.replace('#version 430 core', '#version 430 core\n#define PACKET')
    program = context.compile(shader)
    gl = context.gl
    u, i = C.c_uint, C.c_int
    gl('UseProgram', None, u)(program)
    names = ('receiver_count', 'source_count', 'first_receiver', 'output_offset')
    locations = {name: gl('GetUniformLocation', i, u, C.c_char_p)(program, name.encode()) for name in names}
    if min(locations.values()) < 0:
        raise RuntimeError('Missing diagnostic uniform')
    set_int = gl('Uniform1i', None, i, i)
    dispatch = gl('DispatchCompute', None, u, u, u)
    barrier = gl('MemoryBarrier', None, u)
    def uniform(name, value):
        set_int(locations[name], value)
    payloads = [nodes, triangles, vectors(sources), vectors(receivers), bytes(count * copies * 4)]
    outputs = [context.buffer(binding, payload) for binding, payload in enumerate(payloads)]
    uniform('receiver_count', count)
    uniform('source_count', len(sources))
    queries = (u * 2)()
    gl('GenQueries', None, i, C.POINTER(u))(2, queries)
    context.queries.extend(queries)
    stamp = gl('QueryCounter', None, u, u)
    get_time = gl('GetQueryObjectui64v', None, u, u, C.POINTER(C.c_uint64))
    timings = []
    for iteration in range(repeats + 3):
        stamp(queries[0], 0x8E28)
        for copy in range(copies):
            uniform('output_offset', copy * count)
            # Bounded dispatches avoid a long single invocation on Windows.
            for first in range(0, count, 1024):
                uniform('first_receiver', first)
                batch = min(1024, count - first)
                dispatch(batch if packet else (batch + 63) // 64, 1, 1)
        barrier(0x200)
        stamp(queries[1], 0x8E28)
        begin, end = C.c_uint64(), C.c_uint64()
        get_time(queries[0], 0x8866, C.byref(begin))
        get_time(queries[1], 0x8866, C.byref(end))
        if iteration >= 3:
            timings.append((end.value - begin.value) / 1.e6)
    result = np.empty(count * copies, dtype=np.float32)
    gl('BindBuffer', None, u, u)(0x90D2, outputs[-1])
    gl('GetBufferSubData', None, u, C.c_ssize_t, C.c_ssize_t, C.c_void_p)(
        0x90D2, 0, result.nbytes, result.ctypes.data)
    context.check()
    result = result.reshape(copies, *data['full'].shape)
    return result, dict(gpu_ms=timings, gpu_median_ms=float(np.median(timings)),
                       build_python_ms=build_ms, geometry_bytes=len(nodes)+len(triangles),
                       total_buffer_bytes=sum(map(len, payloads)), node_count=len(nodes)//48)


def verify_analytic(context, packet):
    """Known finite-segment answers, independent of captured game geometry."""
    rectangle = [[[-.4, -2, 1], [.4, -2, 1], [.4, 2, 1]],
                 [[-.4, -2, 1], [.4, 2, 1], [-.4, 2, 1]]]
    cases = [dict(triangles=rectangle,
                  sources=[[-2, 0, 0], [-.2, 0, 0], [.2, 0, 0], [2, 0, 0]],
                  receivers=[[0, 0, 2], [5, 0, 2], [0, 0, 0], [0, 0, .5], [0, 0, 1], [0, 0, 3]],
                  full=[[.5, 1, 1, 1, 1, .5]]),
             dict(triangles=[[[-1, -1, .0001], [1, -1, .0001], [0, 1, .0001]]],
                  sources=[[0, 0, 0]], receivers=[[0, 0, 1], [0, 0, 100], [0, 0, -1]],
                  full=[[0, 0, 1]])]
    for case in cases:
        data = {key: np.asarray(value, dtype=np.float32) for key, value in case.items()}
        result, _ = run(context, data, 1, 3, packet)
        if not np.array_equal(result[0], data['full']):
            raise RuntimeError(f'Analytic visibility failed: {result}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--copies', type=int, choices=(1, 8), default=1)
    parser.add_argument('--repeats', type=int, choices=range(3, 21), default=7)
    parser.add_argument('--packet', action='store_true')
    parser.add_argument('--geometry', choices=('full', 'captured'), default='full',
                        help='captured is a deliberate missing-occluder negative control')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.reference, allow_pickle=False) as archive:
        data = {key: archive[key] for key in ('triangles', 'receivers', 'sources', 'full')}
        if args.geometry == 'captured':
            data['triangles'] = data['triangles'][archive['visible_triangles']]
    if (data['full'].ndim != 2 or not 0 < data['full'].size <= 1048576
            or not np.isfinite(data['full']).all()
            or data['full'].min() < 0 or data['full'].max() > 1):
        raise ValueError('Invalid reference visibility image')
    if (data['sources'].ndim != 2 or data['sources'].shape[1] != 3
            or not 0 < len(data['sources']) <= 1024
            or data['receivers'].shape != (*data['full'].shape, 3)):
        raise ValueError('Invalid reference sources or receiver grid')
    context = Context()
    try:
        verify_analytic(context, args.packet)
        result, metrics = run(context, data, args.copies, args.repeats, args.packet)
        reference = data['full']
        error = np.abs(result - reference)
        if not np.isfinite(result).all() or result.min() < 0 or result.max() > 1:
            raise RuntimeError('Invalid GPU visibility')
        metrics.update(context.info)
        metrics.update(receiver_pixels=reference.size, source_samples=len(data['sources']),
                       copies=args.copies, packet=args.packet,
                       mean_error=float(error.mean()), max_error=float(error.max()),
                       fraction_error_over_one_sample=float(np.mean(error > 1 / len(data['sources']) + 1.e-7)),
                       geometry_triangles=len(data['triangles']),
                       geometry=args.geometry, analytic_checks_passed=True,
                       scope='GPU local opaque geometry only; repeated copies are synthetic, not combat lights',
                       provenance={str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in (args.reference, SHADER, Path(__file__))})
        np.savez_compressed(args.output/'result.npz', visibility=result)
        width, height = reference.shape[1], reference.shape[0]
        image = Image.new('RGB', (width*3, height+30), '#171b24')
        draw = ImageDraw.Draw(image)
        for index, (label, array) in enumerate((('CPU full geometry', reference),
                ('GPU full geometry', result[0]), ('Absolute error (red)', error[0]))):
            pixels = np.uint8(np.clip(array, 0, 1) * 255)
            panel = Image.fromarray(pixels).convert('RGB')
            if index == 2:
                panel = Image.fromarray(np.stack((pixels, np.zeros_like(pixels), np.zeros_like(pixels)), axis=-1))
            image.paste(panel, (index*width, 30))
            draw.text((index*width+4, 7), label, fill='white')
        image.resize((width*6, (height+30)*2)).save(args.output/'comparison.png')
        (args.output/'report.json').write_text(json.dumps(metrics, indent=2)+'\n')
        print(json.dumps(metrics, indent=2), flush=True)
        if metrics['max_error'] > 1 / len(data['sources']) + 1.e-7:
            raise RuntimeError('GPU visibility failed reference agreement')
    finally:
        context.close()


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
