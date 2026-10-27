GulchCE Android shader diagnostic

The APK beside this folder uses stock lighting on Android. Dynamic Lighting
is compiled out, including its glow shaders. Desktop lighting is unchanged.
This is a testing build; affected devices still need a visual check.

INSTALL
Install the APK as an update, or use:
    adb install -r ../app-debug.apk
If Android reports a signature mismatch, keep the existing app and saves.
Do not uninstall to work around it; report the mismatch instead.

CAPTURE A PROBLEM
You need Python 3 and Android SDK Platform-Tools on the computer:
    https://www.python.org/downloads/
    https://developer.android.com/tools/releases/platform-tools

1. Enable USB debugging on the device. Connect it to the computer and accept
   the device's authorization prompt. "adb devices" should show "device".
2. Windows: double-click Collect-Shader-Logs.cmd. Or run from this folder:
       python collect_shader_logs.py
   macOS/Linux:
       python3 collect_shader_logs.py
   If adb is not on PATH, provide its full path:
       python collect_shader_logs.py --adb "C:/platform-tools/adb.exe"
   With several devices connected, add --serial DEVICE_SERIAL.
3. Once capture starts, close and reopen GulchCE to capture startup GPU
   information and shader compilation. Load the affected map and reproduce
   missing/black models, weapon rendering or other visual problems.
4. Capture ends after two minutes, or press Ctrl+C to finish early. You can
   select another limit with --seconds 300 (maximum ten minutes).
5. Send the resulting shader-logs-*.zip with the map name/family, affected
   weapon/object, settings, what happened and an approximate time in the log.
   The folder also contains reproduction.txt as a prompt for these details.

WHAT IT CAPTURES
Only the game's "halo" log tag is recorded. Existing renderer diagnostics
include shader compiler errors and generated GLSL source, program-link errors,
and the startup OpenGL renderer/version. The collector preserves complete log
lines and creates a short summary, plus device/Android and installed-app info.
The bundled build revision is included if present. It keeps partial output
when adb disconnects, and stops after the time limit or about 32 MiB of logs.

The collector does not enable lighting, change settings, install/stop the app,
clear device logs, copy maps/saves, or upload anything. Review logs before
sharing: game logs can include map paths and multiplayer details. A clean
summary alone does not establish that rendering is correct. Include a clip
or screenshot when the visual fault occurs without a shader error.

No root access is needed. If the APK still fails to render, this capture can
also be made with a previously installed build for comparison; include which
APK was installed. It does not enable extra GL tracing, which may affect FPS.
Logcat reference: https://developer.android.com/tools/logcat
