"""GulchCE release identity, independent of upstream workflow counters."""
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

def version_code(version):
    if not re.fullmatch(r"(?:0|[1-9][0-9]{0,2})\.(?:0|[1-9][0-9]{0,2})(?:\.(?:0|[1-9][0-9]{0,2}))?", version):
        raise ValueError("VERSION must be major.minor or major.minor.patch (components 0..999)")
    parts = [int(p) for p in version.split(".")]
    parts += [0] * (3 - len(parts))
    return 1000000 * parts[0] + 1000 * parts[1] + parts[2]

def release_version():
    version = (ROOT / "VERSION").read_text().strip()
    version_code(version)
    return version

def official_release():
    ref = os.environ.get("GITHUB_REF", "")
    if ref.startswith("refs/tags/v") and ref != "refs/tags/v" + release_version():
        raise ValueError("Release tag must match VERSION")
    return os.environ.get("GITHUB_REPOSITORY") == "chestahh/GulchCE" and ref == "refs/tags/v" + release_version()

def updater_defines(release):
    version = release_version()
    enabled = int(official_release())
    flavor = "release" if release else "debug"
    return f'-DGULCHCE_VERSION=\\"v{version}\\" -DGULCHCE_RELEASE={enabled} -DHALO_BUILD_FLAVOR=\\"{flavor}\\"'

if __name__ == "__main__":
    if not official_release():
        raise SystemExit("Not a GulchCE version tag matching VERSION")
    print("v" + release_version())
