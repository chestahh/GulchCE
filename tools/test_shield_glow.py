"""Restored glow follows host policy and drops handles at map boundaries."""
from pathlib import Path
import subprocess

from harness import enum_with, function, structure
from test_mcc_identity import compile_tool

ROOT = Path(__file__).resolve().parents[1]


def test_policy_and_map_reset(tmp_path):
    glow = (ROOT / "port/linux/game/shield_glow.c").read_text()
    overshield = (ROOT / "port/linux/game/overshield_effect.c").read_text()
    policy = (ROOT / "port/linux/game/network_shield_effects.h").read_text()
    code = r'''
#include <string.h>
typedef int boolean;
typedef float real;
#define FALSE 0
#define TRUE 1
#define NONE (-1)
#define SHIELD_GLOW_MAXIMUM 8
static short host_choice = NONE;
static int local_glow, local_overshield;
static unsigned long changes;
static char const *intensity = "default";
static short network_shield_effects(void) { return host_choice; }
static unsigned long config_changes(void) { return changes; }
static int config_boolean(char const *key) {
    return !strcmp(key, "display.shield_glow") ? local_glow : local_overshield;
}
static char const *config_string(char const *key) { (void)key; return intensity; }
'''
    code += enum_with(policy, "_shield_effects_stock")
    code += structure(glow, "shield_glow")
    code += "\nstatic struct shield_glow shield_glows[8];\n"
    code += "static real shield_glow_time;\nstatic real shield_glow_scale = 1.f;\n"
    code += "static boolean overshield_effects_initialized;\nstatic void overshield_effect_reset(void) {}\n"
    code += function(glow, "shield_glow_reset")
    code += function(glow, "shield_glow_enabled")
    code += function(overshield, "overshield_effect_enabled")
    code += r'''
int main(void) {
    int choice, i;
    if (shield_glow_enabled() || overshield_effect_enabled()) return 1;
    local_glow = local_overshield = 1; intensity = "light_show"; changes++;
    if (!shield_glow_enabled() || shield_glow_scale != 3.f || !overshield_effect_enabled()) return 2;
    for (choice = 0; choice < NUMBER_OF_SHIELD_EFFECTS; choice++) {
        host_choice = (short)choice;
        if (shield_glow_enabled() != (choice != 0 && choice != 3)) return 3;
        if (overshield_effect_enabled() != (choice >= 3)) return 4;
        if (shield_glow_scale != ((choice == 2 || choice == 5) ? 3.f : 1.f)) return 5;
    }
    host_choice = NONE;
    if (!shield_glow_enabled() || shield_glow_scale != 3.f || !overshield_effect_enabled()) return 6;
    local_glow = local_overshield = 0; changes++;
    if (shield_glow_enabled() || overshield_effect_enabled()) return 7;
    memset(shield_glows, 0x55, sizeof(shield_glows)); shield_glow_time = 100;
    shield_glow_reset(); shield_glow_reset();
    if (shield_glow_time) return 8;
    for (i = 0; i < 8; i++)
        if (shield_glows[i].unit_index != NONE || shield_glows[i].light_index != NONE || shield_glows[i].connected) return 9;
    return 0;
}
'''
    source = tmp_path / "glow.c"
    source.write_text(code)
    exe = compile_tool(tmp_path, [source])
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


def test_glow_lifetime_is_owned_by_map_lights():
    lights = (ROOT / "source/objects/object_lights.c").read_text()
    for name in ("lights_initialize_for_new_map", "lights_dispose_from_old_map"):
        assert "shield_glow_reset();" in function(lights, name)
    assert "lights_initialize_for_new_map();" in function(lights, "lights_port_recover")
