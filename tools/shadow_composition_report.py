"""Summarize real pass readbacks and encode an annotated gameplay capture.

Requires PyAV/Pillow/NumPy. Captures and timing logs are inputs, never modified.
Video timing follows recorded wall-clock timestamps, including capture stalls.
"""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import re
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont


def stages(directory):
    images = []
    for index in range(5):
        data = (directory / f'composition-stage-{index}.bin').read_bytes()
        width, height = struct.unpack_from('<ii', data)
        if not (0 < width <= 8192 and 0 < height <= 8192 and len(data) == 8 + width * height * 16):
            raise ValueError('Invalid framebuffer dimensions')
        images.append(np.frombuffer(data, '<f4', offset=8).reshape(height, width, 4)[..., :3])
    differences = [images[b] - images[a] for a, b in ((0, 1), (1, 2), (3, 4))]
    report = {}
    for label, delta in zip(('environment', 'dynamic_diffuse', 'dynamic_specular'), differences):
        report[label] = dict(brighter_pixels=int((delta.max(axis=2) > .002).sum()),
                             darker_pixels=int((delta.min(axis=2) < -.002).sum()),
                             minimum=float(delta.min()), maximum=float(delta.max()))
    shadow = differences[0].min(axis=2) < -.002
    fill = differences[1].max(axis=2) > .002
    report['environment_shadow_pixels_filled_by_dynamic_light'] = int((shadow & fill).sum())
    assert report['environment']['darker_pixels'] > 0, 'No environmental replacement exercised'
    assert report['environment_shadow_pixels_filled_by_dynamic_light'] > 0, 'No dynamic fill inside environmental shadow'
    assert report['dynamic_diffuse']['darker_pixels'] == 0, 'Additive diffuse darkened existing lighting'
    assert report['dynamic_specular']['darker_pixels'] == 0, 'Additive specular darkened existing lighting'
    return report


def frames(directory):
    files = sorted((directory / 'screenshots').glob('frame*.png'))
    if not files:
        files = sorted((directory / 'screenshots').glob('frame*.bmp'))
    log = (directory / 'stderr.log').read_text(errors='replace')
    times = [(int(ms), int(tick)) for ms, tick in re.findall(r'PLASMA frame (\d+) tick (\d+)', log)
             if 100 <= int(tick) <= 220]
    if len(files) != len(times) or not files:
        raise ValueError(f'Cannot match screenshots to frame timestamps: {len(files)} images, {len(times)} timestamps')
    return files, times


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path)
    args = parser.parse_args()
    if args.dependencies:
        sys.path.insert(0, str(args.dependencies.resolve()))
    import av
    args.output.parent.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 23)
    small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    reports = []
    container = av.open(str(args.output), 'w')
    stream = container.add_stream('libx264', rate=30)
    stream.width, stream.height = 1280, 784
    stream.pix_fmt = 'yuv420p'
    stream.options = {'crf': '18', 'preset': 'medium'}
    count = 0

    def emit(image):
        nonlocal count
        frame = av.VideoFrame.from_image(image)
        frame.pts, frame.time_base = count, Fraction(1, 30)
        count += 1
        for packet in stream.encode(frame):
            container.mux(packet)

    try:
        for directory in args.capture:
            files, times = frames(directory)
            result = json.loads((directory / 'report.json').read_text())
            evidence = stages(directory)
            reports.append(dict(capture=str(directory), stages=evidence,
                                screenshots=len(files), source_span_ms=times[-1][0] - times[0][0]))
            index = 0
            duration = (times[-1][0] - times[0][0]) / 1000
            last = None
            for sample in range(round(duration * 30) + 1):
                timestamp = times[0][0] + sample * 1000 / 30
                while index + 1 < len(times) and times[index + 1][0] <= timestamp:
                    index += 1
                with Image.open(files[index]) as source:
                    image = Image.new('RGB', (1280, 784), '#111820')
                    image.paste(source.convert('RGB').resize((1280, 720)), (0, 64))
                draw = ImageDraw.Draw(image)
                title = f"{result['shadow_scene'].title()} | {result['modern_shadows'].title()} shadows | 1024"
                tick = times[index][1]
                phase = 'Environmental lighting' if tick < 115 or tick > 205 else 'Dynamic light approaching' if tick < 155 else 'Dynamic light receding' if tick > 170 else 'Dynamic light closest'
                draw.text((18, 6), title, font=font, fill='white')
                draw.text((18, 36), 'Controlled engine capture: real projectile moved through normal falloff; no shadow animation.', font=small, fill='#b9c7d6')
                draw.text((920, 11), phase, font=small, fill='#b8efb4')
                if last is None:
                    for _ in range(20):
                        emit(image)
                emit(image)
                last = image
            for _ in range(30):
                emit(last)
        for packet in stream.encode():
            container.mux(packet)
    finally:
        container.close()
    with av.open(str(args.output)) as check:
        decoded = sum(1 for _ in check.decode(video=0))
    assert decoded == count, (decoded, count)
    args.output.with_suffix('.json').write_text(json.dumps(dict(frames=count, fps=30,
        seconds=count / 30, captures=reports), indent=2))
    print(json.dumps(dict(video=str(args.output), frames=count, seconds=count / 30), indent=2))


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
