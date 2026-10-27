"""Generated setting rows remain operable after pointer/controller focus changes."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, read, run


def test_setting_focus():
    code = function(read('port/linux/game/menu_functions.c'), 'settings_help')
    status, output = run(build('settings_focus', (('under_test.inc', code),)), 'focus')
    assert status == 0, output
    # The missing input route reproduced by the real-menu acceptance run.
    broken = code.replace('row->focused_child = child;', '(void)child;')
    status, output = run(build('settings_focus', (('under_test.inc', broken),)), 'focus')
    assert status == CHECK_FAILED, output
