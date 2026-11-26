"""Grouped ON/OFF controls only write on Save; migration is tested separately."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run, function, structure


def test_groups():
    source = re.sub(r'^#include.*$', '', read('port/linux/game/lighting_menu.c'), flags=re.M)
    menus = read('port/linux/game/menu_functions.c')
    names = ('text_is_number', 'spinner_item', 'setting_value_index', 'setting_text', 'setting_write', 'setting_load', 'setting_save', 'setting_changed_save', 'setting_default_show')
    source += '\n' + '\n'.join(function(menus, name) for name in names)
    status, output = run(build('lighting_menu', (('under_test_types.inc', structure(menus, 'pc_menu_setting')), ('under_test.inc', source))), 'groups')
    assert status == 0, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
