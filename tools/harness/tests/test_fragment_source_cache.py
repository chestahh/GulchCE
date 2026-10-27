"""Compile the real full-key/source cache against a counted shader compiler."""
import pytest
from tools.harness import CHECK_FAILED, build, function, mutated, read, run, structure


def executable(collision=False, fault=None):
    source = read('port/linux/src/d3d8_gl.c')
    code = '\n'.join(structure(source, name) for name in ('fragment_entry', 'fragment_source_entry'))
    code += '\n#define FRAGMENT_BUCKETS 1024\n'
    code += 'static struct fragment_entry *fragment_buckets[FRAGMENT_BUCKETS];\n'
    code += 'static struct fragment_source_entry *fragment_source_buckets[FRAGMENT_BUCKETS];\n'
    code += 'static unsigned long long *shader_hashes;\nstatic GLuint shader_hashes_size;\n'
    code += '\n'.join(function(source, name) for name in (
        'source_hash', 'shader_hash_set', 'hash_words', 'words_equal',
        'fragment_source_hash', 'fragment_source_get', 'fragment_shader_get'))
    if collision:
        hashing = function(source, 'fragment_source_hash')
        code = code.replace(hashing, hashing.replace('return hash;', '(void)hash; return 0;'))
    if fault:
        code = mutated(code, *fault)
    return build('fragment_source_cache', (('under_test.inc', code),))


@pytest.mark.parametrize('collision', [False, True])
@pytest.mark.parametrize('case', ['reuse', 'distinct', 'failure', 'allocation', 'recent', 'prefix'])
def test_cache(case, collision):
    status, output = run(executable(collision), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [
    (('entry->hash == hash && !strcmp(entry->source, source)', 'entry->hash == hash'), 'distinct'),
    (('entry->hash == hash && !strcmp(entry->source, source)', 'FALSE'), 'reuse'),
    (('shader_hash_set(shader, source);', '(void)0;'), 'reuse'),
])
def test_negative_control(fault, case):
    status, output = run(executable(True, fault), case)
    assert status == CHECK_FAILED, output
