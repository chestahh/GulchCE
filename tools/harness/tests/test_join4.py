"""Exercise four-port lobby ownership and keyboard sharing with the real handlers."""
from tools.harness import build, function, read, run


def test_join_and_back_for_every_opener():
    source = read("port/linux/game/menu_functions.c")
    handlers = "\n".join(function(source, name) for name in ("join4_back", "join4_done", "join4_keyboard"))
    status, output = run(build("join4", (("handlers.inc", handlers),)), "ports")
    assert status == 0, output
