"""Extract Bigass v3 plasma-bolt sprites for local use; does not redistribute map assets."""
import argparse
import hashlib
from pathlib import Path
import struct


def extract(path):
    data = Path(path).read_bytes()
    def read(offset, size):
        if offset < 0 or size < 0 or offset > len(data) - size:
            raise ValueError("map reference outside file")
        return data[offset:offset + size]
    def u32(offset):
        return struct.unpack("<I", read(offset, 4))[0]
    if len(data) < 2048 or u32(4) != 609:
        raise ValueError("expected an uncompressed Custom Edition map")
    start = u32(16)
    def ptr(address, size):
        offset = start + address - 0x40440000
        read(offset, size)
        return offset
    count = u32(start + 12)
    if not 0 < count < 65535:
        raise ValueError("invalid tag count")
    table = ptr(u32(start), count * 32)
    found = None
    for i in range(count):
        at = table + i * 32
        if read(at, 4) != b"mtib":
            continue
        name = ptr(u32(at + 16), 1)
        end = data.find(b"\0", name, min(name + 1024, len(data)))
        if end < 0:
            raise ValueError("unterminated tag name")
        if data[name:end] == b"altis\\effects\\bitmaps\\plasma bolts":
            if u32(at + 24):
                raise ValueError("external texture is unsupported")
            found = ptr(u32(at + 20), 108)
            break
    if found is None:
        raise ValueError("Bigass plasma-bolt bitmap not found")
    if u32(found + 84) != 1 or u32(found + 96) != 2:
        raise ValueError("unexpected sprite-sheet layout")
    sequence = ptr(u32(found + 88), 64)
    if u32(sequence + 52) != 31:
        raise ValueError("expected 31 plasma-bolt frames")
    sprites = read(ptr(u32(sequence + 56), 31 * 32), 31 * 32)
    for i in range(31):
        if struct.unpack_from("<h", sprites, i * 32)[0] not in (0, 1):
            raise ValueError("invalid sprite bitmap")
    bitmaps = ptr(u32(found + 100), 96)
    pixels = []
    for i in range(2):
        at = bitmaps + i * 48
        if struct.unpack("<6h", read(at + 4, 12)) != (512, 512, 1, 0, 14, 131):
            raise ValueError("expected the original 512x512 DXT1 atlases")
        if struct.unpack("<h", read(at + 20, 2))[0] != 2 or u32(at + 28) != 172032:
            raise ValueError("unexpected mip chain")
        pixels.append(read(u32(at + 24), 172032))
    return b"GCARCS1\0" + sprites + b"".join(pixels)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map", type=Path)
    parser.add_argument("output", type=Path, help="data-root/shield_assets/bigass_arcs.bin")
    args = parser.parse_args()
    payload = extract(args.map)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f"{len(payload)} bytes; SHA-256 {hashlib.sha256(payload).hexdigest()}")
