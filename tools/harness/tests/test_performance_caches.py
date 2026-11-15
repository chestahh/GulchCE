"""Exercise the production cache functions, including invalidation and failure."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, read, run


@pytest.mark.parametrize("case", ["names", "settings", "failure", "none"])
def test_gametype_cache(case):
    source = read("port/linux/game/menu_functions.c")
    end = source.index("} gametype_shown = { NONE };") + len("} gametype_shown = { NONE };")
    start = source.rindex("static struct", 0, end)
    code = source[start:end] + "\n" + "\n".join(function(source, name) for name in
        ("gametype_shown_get", "gametype_shown_forget", "gametype_display_name", "gametype_rules_text"))
    status, output = run(build("gametype_cache", (("under_test.inc", code),)), case)
    assert status == 0, output


def test_socket_port_cache():
    source = read("port/linux/src/xnet.c")
    start = source.index("#define CACHED_SOCKET_PORTS")
    end = source.index("static unsigned short socket_port", start)
    code = source[start:end] + function(source, "socket_port") + "\n" + function(source, "halo_ws_closesocket")
    status, output = run(build("socket_port_cache", (("under_test.inc", code),)), "lifetime")
    assert status == 0, output


@pytest.mark.parametrize("case", ["full", "partial", "would-block", "closed"])
def test_reliable_read(case):
    source = read("source/networking/network_connection.c")
    line = next(line for line in source.splitlines() if line.startswith("#define RELIABLE_READ_MAXIMUM_SIZE"))
    code = line + "\n" + function(source, "network_connection_idle_client_reliable_endpoint")
    status, output = run(build("reliable_read", (("under_test.inc", code),)), case)
    assert status == 0, output
