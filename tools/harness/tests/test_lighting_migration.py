"""Run the lighting migration against the production TOML parser."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run


def test_saved_lighting_reset_once():
    status, output = run(build('lighting_migration', (
        ('tomlc17.h', read('port/third_party/tomlc17/tomlc17.h')),
        ('toml.inc', read('port/third_party/tomlc17/tomlc17.c')),
        ('under_test.inc', read('port/linux/src/lighting_settings_migration.inl')),
    )), 'migration')
    assert status == 0, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
