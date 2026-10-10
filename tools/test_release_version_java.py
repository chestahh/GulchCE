"""Run the Android version parser without requiring an Android device."""
import subprocess
from pathlib import Path

if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    classes = root / "build/release-version-tests"
    classes.mkdir(parents=True, exist_ok=True)
    subprocess.run(["javac", "--release", "17", "-d", str(classes),
                    str(root / "port/android/app/src/main/java/com/halo/decomp/ReleaseVersion.java"),
                    str(root / "tools/tests/ReleaseVersionTest.java")], check=True)
    subprocess.run(["java", "-cp", str(classes), "com.halo.decomp.ReleaseVersionTest"], check=True)
