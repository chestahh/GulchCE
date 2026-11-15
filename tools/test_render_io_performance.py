"""Production HUD and mod-reader behavior, failures and I/O counts."""
from pathlib import Path
import subprocess
import sys
import pytest
from harness import function
from test_mcc_identity import compile_tool
ROOT=Path(__file__).resolve().parents[1]


def run_fixture(tmp_path, name, code, flags=()):
    inc=tmp_path/'under_test.inc';inc.write_text(code)
    path=tmp_path/'case.c';path.write_text((ROOT/'tools/harness/tests'/name).read_text())
    (tmp_path/'posix.h').write_text((ROOT/'port/linux/src/posix.h').read_text())
    if flags:path.write_text('\n'.join('#define '+f for f in flags)+'\n'+path.read_text())
    exe=compile_tool(tmp_path,[path])
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    print(result.stdout.strip())


@pytest.mark.parametrize('windows',[False,True],ids=['retained-posix','windows-original'])
def test_mod_readers(tmp_path,windows):
    s=(ROOT/'port/linux/src/port_mods.c').read_text()
    a=s.index('static pthread_mutex_t open_files_lock');b=s.index('static void files_dispose',a)
    code=s[a:b]+function(s,'port_mods_file_read')
    run_fixture(tmp_path,'mod_reader_cache.c',code,['HALO_WINDOWS'] if windows else [])


def test_hud_projection_preserves_draw_and_fresh_visibility(tmp_path):
    s=(ROOT/'source/interface/hud.c').read_text()
    run_fixture(tmp_path,'hud_name_projection.c',function(s,'hud_draw_player_name'))


@pytest.mark.skipif(sys.platform=='win32',reason='real POSIX identity and pthread integration')
def test_real_mod_files_and_concurrent_cleanup(tmp_path):
    s=(ROOT/'port/linux/src/port_mods.c').read_text()
    a=s.index('static pthread_mutex_t open_files_lock');b=s.index('static void files_dispose',a)
    (tmp_path/'under_test.inc').write_text(s[a:b]+function(s,'port_mods_file_read'))
    (tmp_path/'posix.h').write_text((ROOT/'port/linux/src/posix.h').read_text())
    path=tmp_path/'case.c';path.write_text((ROOT/'tools/harness/tests/mod_reader_filesystem.c').read_text())
    exe=compile_tool(tmp_path,[path,ROOT/'port/linux/src/posix_files.c'])
    result=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    print(result.stdout.strip())
