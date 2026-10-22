"""Compile the actual shield-look module against a small 32-bit fake map.

SHIELD_LOOK_CC can name a portable compiler wrapper. Optional reference checks
use SHIELD_LOOK_REFERENCE_MAP and SHIELD_LOOK_STOCK_MAP, never bundled assets.
"""
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zlib

import pytest

from harness import function, structure

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def shield_tool(tmp_path_factory):
    cc = os.environ.get("SHIELD_LOOK_CC") or shutil.which("clang")
    if not cc:
        pytest.skip("a 32-bit C compiler is required")
    work = tmp_path_factory.mktemp("shield-look")
    source = (ROOT / "port/linux/game/shield_look.c").read_text()
    source = re.sub(r'^#include "[^"\n]+"\s*$', '', source, flags=re.M)
    # Stub successful cache discovery separately from the real bitmap reader.
    source = source.replace("static boolean shield_look_stock_noise(", "static boolean shield_look_read_noise(")
    source = source.replace("shield_look_stock_noise(index)", "(index == 3 ? noise_matches : 1)")
    shader_header = (ROOT / "source/shaders/shader_definitions.h").read_text()
    object_header = (ROOT / "source/objects/object_definitions.h").read_text()
    structs = "\n".join(structure(shader_header, n) for n in
                        ("shader_radiosity_properties", "shader_physics_properties", "shader_base", "shader"))
    structs += "\n" + structure(object_header, "object_function_definition")
    (work / "real_types.inc").write_text(structs)
    overshield = (ROOT / "port/linux/game/overshield_effect.c").read_text()
    source += "\n" + structure(overshield, "overshield_effect_plasma")
    source += "\n" + function(overshield, "overshield_effect_intensity_source")
    (work / "under_test.inc").write_text(source)
    output = work / ("shield.exe" if os.name == "nt" else "shield")
    flags = ["-m32", "-std=gnu89", "-Wall", "-Werror", "-Wno-unused-function", "-Wno-multichar"]
    if os.name == "nt" and not os.environ.get("SHIELD_LOOK_CC"):
        flags += ["--target=i686-pc-windows-msvc", "-fuse-ld=lld", "-D_CRT_SECURE_NO_WARNINGS"]
    command = [cc, *flags, "-I", str(work), str(ROOT / "tools/harness/tests/shield_look.c"), "-o", str(output)]
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return output


@pytest.mark.parametrize("case", ["colors", "classic", "invalid-setting", "profiles", "authored-shader", "authored-noise",
                                  "authored-function", "authored-channel", "authored-nonplasma", "authored-environment", "authored-hands", "authored-body", "authored-decal", "missing", "no-noise", "split",
                                  "toggle", "outer", "arms-off", "reload", "capacity", "malformed-noise"])
@pytest.mark.parametrize("look", ["enhanced", "enhanced2", "honeycomb"])
def test_shield_look(shield_tool, case, look):
    run = subprocess.run([str(shield_tool), case, look], capture_output=True, text=True, timeout=15)
    assert run.returncode == 0, run.stdout + run.stderr


def test_menu_and_default():
    sys.path.insert(0, str(ROOT / "tools"))
    import port_settings
    spec = port_settings.SCREENS["video_settings/fov_viewmodels"]
    row = next(row for row in spec["rows"] if row[1] == "display.shield_look")
    assert row[2] == [("CLASSIC", "classic"), ("CLASSIC WITH SPRINKLES", "classic_with_sprinkles"), ("ENHANCED", "enhanced"), ("ENHANCED HONEYCOMB", "honeycomb")]
    assert "video_settings/shield_look" not in port_settings.SCREENS
    assert not list((ROOT / "port/assets/menus/ce").glob("*.shield_look.xml"))
    assert "video_settings.shield_look.xml" not in (ROOT / "port/assets/menus/menus.json").read_text()
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    assert re.search(r'"display.shield_look", _config_string, "\\"classic\\"", "HALO_SHIELD_LOOK"', config)
    for name, lines in port_settings.settings_files().items():
        if "video_settings" in name:
            assert (ROOT / "port/assets/menus/ce" / name).read_text() == "\n".join(lines)
            if name.endswith(".video_settings.xml"):
                widgets = {w.attrib["name"]: w for w in ET.fromstring("\n".join(lines)).findall("widget")}
                options = next(w for key, w in widgets.items() if key.endswith("/options_menu"))
                help_top = int(next(w for key, w in widgets.items() if key.endswith("/help")).attrib["top"])
                for child in options.findall("child"):
                    if "/op_" in child.attrib["widget"]:
                        row = widgets[child.attrib["widget"]]
                        assert int(child.attrib["y"]) + int(row.attrib["height"]) <= help_top


def test_shared_hooks():
    source = (ROOT / "source/interface/first_person_weapons.c").read_text()
    draw = function(source, "first_person_weapon_draw")
    weapon, hands = [m.start() for m in re.finditer(r'\brender_model\(', draw)]
    assert weapon < draw.index("viewmodel_shield_apply(") < draw.index("shield_look_arms(") < hands
    cache = (ROOT / "source/cache/cache_files.c").read_text()
    assert "shield_look_reset();" in function(cache, "scenario_tags_load")
    assert "shield_look_reset();" in function(cache, "scenario_tags_unload")
    models = (ROOT / "source/models/models.c").read_text()
    assert "shield_look_skip_shell(model, shader)" in function(models, "render_model_parts")
    plasma = (ROOT / "source/rasterizer/xbox/rasterizer_xbox_plasma_energy.c").read_text()
    draw = function(plasma, "rasterizer_plasma_energy_draw")
    assert draw.index("shield_look_colors(") < draw.index("overshield_effect_color(")
    objects = (ROOT / "source/render/render_objects.c").read_text()
    assert "shield_look_body(object_index, model_effect.modifier_shader)" in objects


def map_tags(path):
    data = path.read_bytes()
    version = struct.unpack_from("<I", data, 4)[0]
    if version == 5:
        data = data[:2048] + zlib.decompress(data[2048:])
    offset, size = struct.unpack_from("<II", data, 16)
    assert offset + size <= len(data)
    tags = data[offset:offset + size]
    base = 0x803A6000 if version == 5 else 0x40440000
    entries, _, _, count = struct.unpack_from("<4I", tags)
    assert 0 < count <= 65536 and 0 <= entries - base <= len(tags) - count * 32
    result = []
    for index in range(count):
        group, _, _, handle, name, address, external, _ = struct.unpack_from("<8I", tags, entries - base + index * 32)
        assert base <= name < base + len(tags)
        end = tags.index(b"\0", name - base, min(len(tags), name - base + 1024))
        result.append((group, tags[name - base:end].decode("ascii"), address - base, handle, external))
    return data, tags, result, base


def test_approved_profile_parameters(shield_tool):
    path = os.environ.get("SHIELD_LOOK_REFERENCE_MAP")
    if not path:
        pytest.skip("set SHIELD_LOOK_REFERENCE_MAP for the local CMT comparison")
    _, tags, entries, _ = map_tags(Path(path))
    result = subprocess.run([str(shield_tool), "export"], capture_output=True, check=True)
    # Export only rendering fields, in their on-disk order, excluding opaque
    # padding, addresses and radiosity (not used by the plasma draw).
    spans = [(0x2C, 0x40), (0x60, 0x82), (0xC0, 0xD4), (0x108, 0x11C)]
    body = bytearray(tags[entries[1935][2]:entries[1935][2] + 0x14C])
    # The approved ENHANCED 2 displacement is now the sole ENHANCED body.
    struct.pack_into("<h", body, 0x34, 2)
    struct.pack_into("<ff", body, 0x38, 0.0125, 0.5)
    arms = bytearray(tags[entries[2163][2]:entries[2163][2] + 0x14C])
    # First-person displacement retains the approved Arm Shields profile.
    struct.pack_into("<h", arms, 0x34, 2)
    struct.pack_into("<ff", arms, 0x38, 0.03, 2.0)
    expected = b"".join(profile[lo:hi] for profile in (body, arms) for lo, hi in spans)
    assert bytes.fromhex(result.stdout.decode().strip()) == expected


def test_stock_fingerprints(shield_tool, tmp_path):
    path = os.environ.get("SHIELD_LOOK_STOCK_MAP")
    if not path:
        pytest.skip("set SHIELD_LOOK_STOCK_MAP for the local stock-map comparison")
    data, tags, entries, base = map_tags(Path(path))
    by_name = {entry[1]: entry for entry in entries}
    plasma = by_name[r"characters\cyborg\shaders\shield hit"][2]
    bitmap = by_name[r"characters\elite\bitmaps\plasma shield noise"][2]
    bitmap_count, bitmap_pointer = struct.unpack_from("<II", tags, bitmap + 0x60)
    assert bitmap_count == 1
    pixels, size = struct.unpack_from("<II", tags, bitmap_pointer - base + 24)
    assert pixels + size <= len(data)
    shader_path = tmp_path / "shader.bin"
    pixels_path = tmp_path / "noise.bin"
    shader_path.write_bytes(tags[plasma:plasma + 0x14C])
    pixels_path.write_bytes(data[pixels:pixels + size])
    result = subprocess.run([str(shield_tool), "reference", str(shader_path), str(pixels_path)],
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr


def test_cmt_noise_is_stock(shield_tool, tmp_path):
    cmt = os.environ.get("SHIELD_LOOK_REFERENCE_MAP")
    stock = os.environ.get("SHIELD_LOOK_STOCK_MAP")
    if not cmt or not stock:
        pytest.skip("set both map paths for the complete texture comparison")
    _, _, entries, base = map_tags(Path(cmt))
    noise = entries[1126]
    assert noise[4] and noise[1] == r"characters\elite\bitmaps\plasma shield noise"
    resource_index = noise[2] + base
    resource = Path(cmt).parent / "bitmaps.map"
    with resource.open("rb") as file:
        length = resource.stat().st_size

        def read(offset, size):
            assert 0 <= offset <= length and 0 <= size <= length - offset
            file.seek(offset)
            result = file.read(size)
            assert len(result) == size
            return result

        kind, _, table, count = struct.unpack("<4I", read(0, 16))
        assert kind == 1 and resource_index < count
        _, size, offset = struct.unpack("<3I", read(table + resource_index * 12, 12))
        metadata = read(offset, size)
        count, address = struct.unpack_from("<II", metadata, 0x60)
        assert count == 1 and address + 48 <= len(metadata)
        bitmap = metadata[address:address + 48]
        assert struct.unpack_from("<5H", bitmap, 4) == (32, 32, 32, 1, 11)
        offset, size = struct.unpack_from("<II", bitmap, 24)
        cmt_pixels = read(offset, size)
    assert len(cmt_pixels) == 149796
    data, tags, entries, base = map_tags(Path(stock))
    by_name = {e[1]: e for e in entries}
    shader_offset = by_name[r"characters\cyborg\shaders\shield hit"][2]
    group = by_name[r"characters\elite\bitmaps\plasma shield noise"][2]
    bitmap = struct.unpack_from("<I", tags, group + 0x64)[0] - base
    offset, _ = struct.unpack_from("<II", tags, bitmap + 24)
    xbox = data[offset:offset + 74898]
    packed = bytearray(len(cmt_pixels))
    position = 0
    for level in range(6):
        width = 32 >> level
        bits = 5 - level
        for z in range(width):
            for y in range(width):
                for x in range(width):
                    swizzled = sum(((axis >> bit) & 1) << (3 * bit + dimension)
                                   for dimension, axis in enumerate((x, y, z)) for bit in range(bits))
                    linear = (z * width + y) * width + x
                    luminance, alpha = xbox[(position + swizzled) * 2:(position + swizzled) * 2 + 2]
                    rgba = cmt_pixels[(position + linear) * 4:(position + linear) * 4 + 4]
                    assert rgba == bytes((luminance, luminance, luminance, alpha))
                    packed[(position + swizzled) * 4:(position + swizzled) * 4 + 4] = rgba
        position += width ** 3
    assert position == 37449
    shader_path = tmp_path / "shader.bin"
    pixels_path = tmp_path / "noise.bin"
    shader_path.write_bytes(tags[shader_offset:shader_offset + 0x14C])
    # The real feature reader accepts CE's linear pixels and the same pixels
    # in the hardware layout produced by the existing MCC adapter.
    for layout, pixels in (("ce", cmt_pixels), ("mcc", packed)):
        pixels_path.write_bytes(pixels)
        result = subprocess.run([str(shield_tool), "reference", str(shader_path), str(pixels_path), layout],
                                capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stdout + result.stderr


def test_sprinkles(shield_tool):
    run = subprocess.run([str(shield_tool), "sprinkles"], capture_output=True, text=True, timeout=15)
    assert run.returncode == 0, run.stdout + run.stderr


def test_sprinkles_stock_surface(shield_tool):
    path = os.environ.get("SHIELD_LOOK_STOCK_MAP")
    if not path:
        pytest.skip("set SHIELD_LOOK_STOCK_MAP for the classic surface comparison")
    _, tags, entries, _ = map_tags(Path(path))
    at = next(e[2] for e in entries if e[1] == r"characters\cyborg\shaders\shield hit")
    stock = bytearray(tags[at:at + 0x14C])
    body, arms = bytearray(stock), bytearray(stock)
    struct.pack_into("<ff", body, 0x38, 0.0125, 0.5)
    spans = [(0x2C, 0x40), (0x60, 0x82), (0xC0, 0xD4), (0x108, 0x11C)]
    expected = b"".join(p[lo:hi] for p in (body, arms) for lo, hi in spans)
    result = subprocess.run([str(shield_tool), "export", "classic_with_sprinkles"], capture_output=True, check=True)
    assert bytes.fromhex(result.stdout.decode().strip()) == expected


def test_honeycomb_profile_identity(shield_tool):
    run = subprocess.run([str(shield_tool), "honeycomb"], capture_output=True, text=True, timeout=15)
    assert run.returncode == 0, run.stdout + run.stderr
