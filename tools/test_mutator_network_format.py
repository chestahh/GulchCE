"""Pin the stock contract separately without hiding optional wire changes."""
from pathlib import Path

import pytest
import network_format


def test_both_contracts_recorded():
    assert network_format.check() == []
    base = network_format.items()
    optional = network_format.items(optional=True)
    fixture = (network_format.ROOT / "tools/fixtures/opence_network_43.txt").read_text().splitlines()
    expected = {f"{kind} {name}": value for kind, name, value in
                (line.split(" ", 2) for line in fixture[1:])}
    assert base == expected
    assert network_format.version() == 44
    assert optional["define MUTATOR_PROTOCOL_VERSION"] == "32747"
    assert optional["define MUTATOR_CAMERA_PROTOCOL_VERSION"] == "32748"
    assert "enum _distributed_message_mutators" not in base
    assert optional["enum _distributed_message_mutators"] == "112"


def change_read(monkeypatch, suffix, old, new):
    original = Path.read_text

    def changed(path, *args, **kwargs):
        text = original(path, *args, **kwargs)
        if path.as_posix().endswith(suffix):
            assert old in text
            return text.replace(old, new, 1)
        return text

    monkeypatch.setattr(Path, "read_text", changed)


def test_optional_change_is_not_hidden(monkeypatch):
    change_read(monkeypatch, "/network_distributed.h",
                "_distributed_message_mutators = 112", "_distributed_message_mutators = 118")
    assert any("mutator_network_format.txt: changed" in problem for problem in network_format.check())


def test_stock_change_still_breaks_base(monkeypatch):
    change_read(monkeypatch, "/network_distributed.c",
                "DISTRIBUTED_RELAYED_ACTION_MAXIMUM_SIZE = 14 +",
                "DISTRIBUTED_RELAYED_ACTION_MAXIMUM_SIZE = 13 +")
    assert any("network_format.txt: changed" in problem for problem in network_format.check())


def test_unknown_reserved_layout_is_not_erased(monkeypatch):
    change_read(monkeypatch, "/source/game/game_engine.h",
                "byte addon_vehicles_fewer[2]", "byte addon_vehicles_fewer[3]")
    with pytest.raises(AssertionError, match="unrecognized"):
        network_format.items()
