"""Characterize existing co-op failures, not desired-behavior regression tests.
Extracts real functions; transport and engine dependencies are controlled stubs.
Run with Python and clang plus a 32-bit C runtime. No maps or game processes.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from harness import constant, enum_with, function, structure

def generated():
    source = (ROOT / "port/linux/game/network_coop.c").read_text()
    header = (ROOT / "port/linux/game/network_coop.h").read_text()
    code = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef unsigned char byte;
typedef unsigned short word;
typedef int boolean;
typedef float real;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define _error_silent 0
#define csmemset memset
#define csmemcpy memcpy
#define csmemmove memmove
#define _distributed_message_coop_events 1
#define _object_mask_unit 1
struct distributed_message_header { word header; byte type, count; long time; unsigned long epoch; };
static int is_host, object_exists, animations, packets, warnings;
static int coop_host(void) { return is_host; }
static int coop_client(void) { return !is_host; }
static void error(int level, const char *format, int kind) { warnings++; }
static int distributed_real_valid(float v) { return isfinite(v); }
static int distributed_object_index_valid(long id) { return id >= 0; }
static int network_objects_client_has(long id) { return object_exists; }
static void *object_try_and_get_and_verify_type(long id, int mask) { return object_exists ? &object_exists : 0; }
static int distributed_graph_animation(long tag, short animation) { return 1; }
static void unit_stop_custom_animation(long id) { animations++; }
static void unit_port_play_user_animation(long id, long graph, short animation, int interpolate, short frame) { animations++; }
"""
    for name in ("MAXIMUM_QUEUED_EVENTS", "EVENT_SENDS", "MAXIMUM_NAV_POINTS"):
        code += f"\n#define {name} {constant(source, name)}\n"
    code += enum_with(source, "_coop_event_sound,")
    code += enum_with(header, "_coop_hud_help_text,")
    code += structure(source, "distributed_coop_event")
    code += structure(source, "distributed_coop_events_message")
    code += r"""
static struct {
    struct distributed_coop_event events[MAXIMUM_QUEUED_EVENTS];
    short sends[MAXIMUM_QUEUED_EVENTS], count;
    word next_number, applied_number;
    boolean applied_any;
} coop_events;
static struct {
    short objective, nav_point_count;
    struct { short kind, nav_index; long target, marker; real vertical_offset; } nav_points[MAXIMUM_NAV_POINTS];
} host_hud_state;
static struct { int refresh, joined; } host_resend;
static struct { int show_hud_help_text; } hud_globals;
#define hud_scripted_globals (&hud_globals)
static void send_to_clients(void *message, byte type, short count, word size) {
    packets++; /* deliberately drop the datagrams */
}
"""
    code += function(source, "client_apply_unit_animation")
    code += r"""
static void client_apply_event(struct distributed_coop_event const *event) {
    if (event->kind == _coop_event_unit_animation) client_apply_unit_animation(event);
}
"""
    code += "\n".join(function(source, name) for name in (
        "event_new", "host_send_events", "network_coop_handle_events",
        "network_coop_note_hud", "send_nav_point", "network_coop_note_nav_point",
        "host_send_hud_state"))
    code += r"""
static void reset(void) {
    memset(&coop_events, 0, sizeof(coop_events));
    memset(&host_hud_state, 0, sizeof(host_hud_state));
    memset(&host_resend, 0, sizeof(host_resend));
    host_hud_state.objective = NONE;
    packets = animations = warnings = object_exists = 0;
}
static struct distributed_coop_event animation(word sequence) {
    struct distributed_coop_event event;
    memset(&event, 0, sizeof(event));
    event.kind = _coop_event_unit_animation;
    event.number = sequence; event.object_index = 1; event.tag_index = 2;
    return event;
}
int main(void) {
    struct distributed_coop_event event, later;
    int i;
    reset(); is_host = 1;
    assert(event_new(_coop_event_unit_animation));
    for (i = 0; i < EVENT_SENDS + 10; i++) host_send_events();
    assert(packets == EVENT_SENDS && coop_events.count == 0);
    puts("CONFIRMED: three lost transmissions exhaust a script event without acknowledgement");

    reset(); is_host = 0; event = animation(1);
    network_coop_handle_events(&event, 1);
    assert(animations == 0 && coop_events.applied_number == 1);
    object_exists = 1;
    network_coop_handle_events(&event, 1);
    assert(animations == 0);
    later = animation(2); network_coop_handle_events(&later, 1);
    assert(animations == 1); /* positive control */
    puts("CONFIRMED: missing-object animation consumes its sequence; retry cannot apply it");

    reset(); is_host = 0; object_exists = 1;
    later = animation(2); event = animation(1);
    network_coop_handle_events(&later, 1); network_coop_handle_events(&event, 1);
    assert(animations == 1 && coop_events.applied_number == 2);
    puts("CONFIRMED: a sequence gap is skipped, not buffered for ordered recovery");

    reset(); is_host = 1;
    network_coop_note_nav_point(0, 3, 0, 9, 0.0f);
    assert(host_hud_state.nav_point_count == 1);
    for (i = 0; i < EVENT_SENDS; i++) host_send_events();
    network_coop_note_nav_point(0, NONE, 0, 9, 0.0f);
    for (i = 0; i < EVENT_SENDS; i++) host_send_events();
    assert(host_hud_state.nav_point_count == 0 && coop_events.count == 0);
    host_resend.refresh = 1;
    host_send_hud_state();
    assert(coop_events.count == 0);
    puts("CONFIRMED: periodic nav refresh sends no repair after all removal copies are lost");

    reset(); is_host = 1; host_hud_state.objective = 4;
    for (i = 0; i < MAXIMUM_QUEUED_EVENTS; i++) assert(event_new(_coop_event_title));
    network_coop_note_hud(_coop_hud_objective, 5);
    assert(warnings == 1 && host_hud_state.objective == 4);
    for (i = 0; i < EVENT_SENDS; i++) host_send_events();
    host_resend.refresh = host_resend.joined = 1;
    host_send_hud_state();
    assert(coop_events.count == 1 && coop_events.events[0].value == 4);
    puts("CONFIRMED: queue overflow prevents updating the late-join objective cache");
    return 0;
}
"""
    return code

def main():
    compiler = os.environ.get("CC") or shutil.which("clang")
    if not compiler:
        raise SystemExit("clang (or CC) and a 32-bit C runtime are required")
    target = ["--target=i686-pc-windows-msvc"] if os.name == "nt" else ["-m32"]
    with tempfile.TemporaryDirectory(prefix="coop-replication-probe-") as temporary:
        folder = Path(temporary)
        source, binary = folder / "probe.c", folder / "probe.exe"
        source.write_text(generated())
        subprocess.run([compiler, *target, "-std=gnu89", "-O1", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)

if __name__ == "__main__":
    main()
