"""Late projectile damage must tolerate a retired owner without following its slot."""
import pytest
from tools.harness import CHECK_FAILED, build, function, mutated, read, run


def executable(fault=False):
    code = function(read('source/objects/damage.c'), 'get_player_index_from_object_or_parents')
    if fault:
        code = mutated(code, 'object = object_try_and_get(object_index);',
                       'object = object_get(object_index);')
    return build('damage_owner_lookup', (('under_test.inc', code),))


@pytest.mark.parametrize('case', ['none', 'live', 'ai', 'parent', 'chain',
                                  'unparented', 'deleted', 'reused', 'deleted-parent'])
def test_lookup(case):
    status, output = run(executable(), case)
    assert status == 0, output


@pytest.mark.parametrize('case', ['deleted', 'reused', 'deleted-parent'])
def test_strict_lookup_regression(case):
    status, output = run(executable(True), case)
    assert status == CHECK_FAILED, output
