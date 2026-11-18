"""Fragmentation, capacity pressure and locked blocks in the real LRU allocator."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, enum_with, function, read, run, structure


@pytest.mark.parametrize("release", [False, True])
def test_fragmented_cache(release):
    source = read("source/memory/lruv_cache.c")
    header = read("source/memory/lruv_cache.h")
    types = "\n".join(structure(header, name) for name in ("lruv_cache", "lruv_cache_block"))
    types += "\n" + structure(source, "lruv_cache_hole") + "\n" + enum_with(source, "LRUV_CACHE_SIGNATURE =")
    code = "\n".join(function(source, name) for name in
                     ("lruv_cache_verify", "lruv_cache_bytes_to_pages", "lruv_block_delete", "lruv_block_new"))
    if release:
        code = "#define HALO_RELEASE\n" + code
    status, output = run(build("lruv_cache", (("types.inc", types), ("under_test.inc", code))), "fragmented")
    assert status == 0, output
