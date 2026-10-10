"""Release ordering and fork isolation, including the desktop C implementation."""
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from tools.release_version import official_release, release_version, version_code

ROOT = Path(__file__).resolve().parent.parent
CASES = {
    "0.1": 1000, "0.1.0": 1000, "0.1.1": 1001,
    "0.9": 9000, "0.10": 10000, "1.0": 1000000,
    "999.999.999": 999999999,
}
INVALID = ["", "1", "01.2", "1.02", "1.2.03", "1.2.", "1.2.3.4",
           "1.2-beta", "1.2+abc", "1.2/evil", "1.2 ", "1000.0", "-1.0"]


@pytest.mark.parametrize("version,expected", CASES.items())
def test_numeric_version(version, expected):
    assert version_code(version) == expected


@pytest.mark.parametrize("version", INVALID)
def test_reject_invalid_version(version):
    with pytest.raises(ValueError):
        version_code(version)


@pytest.mark.parametrize("repository,ref,enabled", [
    ("chestahh/GulchCE", "refs/heads/main", False),
    ("chestahh/GulchCE", "refs/heads/codex/test", False),
    ("OpenCommunityEdition/OpenCE", "VERSION_TAG", False),
    ("someone/another-fork", "VERSION_TAG", False),
    ("chestahh/GulchCE", "VERSION_TAG", True),
    ("chestahh/GulchCE", "refs/tags/build-168", False),
])
def test_only_own_release_tags_enable_updates(monkeypatch, repository, ref, enabled):
    monkeypatch.setenv("GITHUB_REPOSITORY", repository)
    monkeypatch.setenv("GITHUB_REF", ref.replace("VERSION_TAG", "refs/tags/v" + release_version()))
    monkeypatch.setenv("HALO_BUILD_NUMBER", "168")
    assert official_release() == enabled


def test_wrong_release_tag_fails(monkeypatch):
    monkeypatch.setenv("GITHUB_REF", "refs/tags/v999.998.997")
    with pytest.raises(ValueError, match="match VERSION"):
        official_release()


def test_desktop_parser(tmp_path):
    cc = shutil.which("clang") or shutil.which("cc")
    if not cc:
        pytest.skip("C compiler unavailable")
    source = tmp_path / "version.c"
    source.write_text('#include <stdio.h>\n#include "release_version.h"\n'
                      'int main(int argc, char **argv) { int i; for (i=1;i<argc;++i) '
                      'printf("%ld\\n",gulchce_version_code(argv[i])); return 0; }\n')
    exe = tmp_path / ("version.exe" if os.name == "nt" else "version")
    flags = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld"] if os.name == "nt" else []
    subprocess.run([cc, *flags, "-Wall", "-Wextra", "-Werror", "-I",
                    str(ROOT / "port/linux/src"), str(source), "-o", str(exe)], check=True)
    tags = ["v" + v for v in CASES] + ["v" + v for v in INVALID] + ["build-168", "0.1", "v.0.1"]
    result = subprocess.check_output([str(exe), *tags], text=True)
    assert [int(v) for v in result.splitlines()] == list(CASES.values()) + [-1] * (len(INVALID) + 3)
