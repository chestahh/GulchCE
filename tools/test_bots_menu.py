"""Bot menus remain reachable, generated, and use typed host actions."""
from pathlib import Path
import xml.etree.ElementTree as ET

import port_settings

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / "port/assets/menus/ce"
BASE = port_settings.PE + "/network_setup/bots"


def document(path):
    return ET.parse(MENUS / (path.replace("/", ".") + ".xml")).getroot()


def test_generated_bot_screens_and_entry_points():
    generated = port_settings.settings_files()
    generated.update(port_settings.multiplayer_files())
    for path in (BASE, BASE + "/manage", port_settings.PE + "/network_setup",
                 port_settings.PE + "/network_setup.in_game",
                 port_settings.MT + "/server_settings", port_settings.MT + "/lobby"):
        name = path.replace("/", ".") + ".xml"
        assert (MENUS / name).read_text(encoding="utf-8") == "\n".join(generated[name])
    for path in (port_settings.PE + "/network_setup", port_settings.PE + "/network_setup.in_game",
                 port_settings.MT + "/server_settings", port_settings.MT + "/lobby"):
        assert any(BASE + "/bots_screen" in node.attrib.values() for node in document(path).iter())
    lobby = document(port_settings.MT + "/lobby")
    assert lobby.find(".//on[@run='port bots host entry']") is not None


def test_capacity_choices_and_live_actions():
    root = document(BASE)
    count = root.find(".//widget[@setting='bots.count']")
    assert count is not None
    assert count.get("values").split("|") == [str(n) for n in range(64)]
    root = document(BASE + "/manage")
    rows = [w for w in root.findall("widget") if w.find("on[@run='port bots command']") is not None]
    assert {w.get("name").rsplit("/", 1)[1] for w in rows} == {
        "op_add_any", "op_add_red", "op_add_blue", "op_drop_any", "op_drop_red", "op_drop_blue"}
    for row in rows:
        assert row.find("on[@event='left_mouse']") is not None
        assert row.find("on[@event='a']") is not None
    # All menu-local child references resolve, including the management footer.
    names = {w.get("name") for w in root.findall("widget")}
    for child in root.iter("child"):
        target = child.get("widget")
        if target.startswith(BASE):
            assert target in names
