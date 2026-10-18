"""Native Vulkan ownership regressions, requiring no GPU or Vulkan loader."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
BACKEND = ROOT / "port/linux/src/vulkan"


@pytest.mark.parametrize("fault", [None, "early_fence", "partial", "wait", "wrong_slot", "reuse", "query_error", "teardown"])
def test_completion_ownership(tmp_path, fault):
    cc = shutil.which("clang")
    if not cc:
        pytest.skip("clang required")
    code = (BACKEND / "vulkan_device.c").read_text()
    changes = {
        "early_fence": ("if (status == VK_NOT_READY)\n\t\treturn 0;", "if (0)\n\t\treturn 0;"),
        "partial": ("if (!results[i][1])", "if (0)"),
        "wait": ("VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT", "VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | VK_QUERY_RESULT_WAIT_BIT"),
        "wrong_slot": ("d->queries, frame->query_first,\n", "d->queries, 0,\n"),
        "reuse": ("if (frame->state != _halo_vk_frame_free && frame->state != _halo_vk_frame_ready)", "if (0)"),
        "query_error": ("if (status != VK_SUCCESS)\n\t\tgoto failed;", "if (0)\n\t\tgoto failed;"),
        "teardown": ("d->validation_errors = validation_errors;", "(void)validation_errors;"),
    }
    if fault:
        before, after = changes[fault]
        assert before in code
        # query_error must leave the fence check intact to reach query failures.
        if fault == "query_error":
            index = code.rindex(before)
            code = code[:index] + code[index:].replace(before, after, 1)
        else:
            code = code.replace(before, after, 1)
    source = tmp_path / "device.c"
    source.write_text(code)
    output = tmp_path / ("completion.exe" if os.name == "nt" else "completion")
    flags = ["-std=gnu89", "-Werror", "-Wno-missing-field-initializers",
             "-I" + str(BACKEND), "-I" + str(ROOT / "port/third_party/vulkan_headers/include")]
    flags += ["--target=i686-pc-windows-msvc", "-fuse-ld=lld"] if os.name == "nt" else ["-m32"]
    libs = [] if os.name == "nt" else ["-ldl"]
    subprocess.run([cc, *flags, str(source), str(BACKEND / "tests/completion.c"), *libs, "-o", str(output)], check=True, capture_output=True)
    result = subprocess.run([str(output)], capture_output=True)
    assert (result.returncode == 0) == (fault is None), result.stderr.decode(errors="replace")


def test_shader_sources_match_embedded_provenance():
    header = (BACKEND / "tests/depth_spirv.h").read_text()
    for stage in ("vert", "frag"):
        source = BACKEND / ("tests/depth." + stage)
        assert f"depth.{stage} SHA256: {hashlib.sha256(source.read_bytes()).hexdigest()}" in header
    assert header.count("0x07230203") == 2


def test_vendored_headers_unchanged():
    root = ROOT / "port/third_party/vulkan_headers"
    for line in (root / "SHA256SUMS").read_text().splitlines():
        digest, name = line.split("  ", 1)
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
