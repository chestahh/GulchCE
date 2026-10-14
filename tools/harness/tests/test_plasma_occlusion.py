"""Exercise the actual visibility field with synthetic collision geometry."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, read, run


def test_visibility_field():
    code = re.sub(r'^#include.*$', '', read('port/linux/game/plasma_occlusion.c'), flags=re.M)
    status, output = run(build('plasma_occlusion', (('under_test.inc', code),)), 'visibility')
    assert status == 0, output


def test_stale_pass_upload_is_detected():
    code = re.sub(r'^#include.*$', '', read('port/linux/game/plasma_occlusion.c'), flags=re.M)
    assert code.count('field->texture_mask = 0;') == 1
    code = code.replace('field->texture_mask = 0;', '(void)0;')
    status, output = run(build('plasma_occlusion', (('under_test.inc', code),)), 'visibility')
    assert status == CHECK_FAILED, output
