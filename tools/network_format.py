#!/usr/bin/env python3
"""The wire format of HALO_PORT_NETWORK_VERSION (port/linux/network_format.txt).

Taken from the sources: the game's message numbers, sizes and fields
(network_messages.c, .h), the distributed netcode's (network_*.c, .h,
map_share*.c, .h), and the gametype's PC options (game_engine.h's struct
game_variant_options) and where the game settings record has them: the enums
with _distributed_ members and the one of the riding deck part's flags
(vehicle_riding.h), the DISTRIBUTED_ constants, the structs sent
whole (the _message structs, those whose size is taken, and the structs in
them) as hashes, and the constants their arrays are sized by, as their
definitions. An entry the netcode packs itself (a unit's state, a relayed
action) is covered by its flags and its least and largest sizes only.

test_linux_port.py fails when the sources differ from the record. An item
changed or removed is one older builds cannot read: raise the version and
say why in NETCODE.md. An item added (a message kind, a bit) may be one they
ignore. Then record it: network_format.py --record
"""

import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECORD = ROOT / "port/linux/network_format.txt"
OPTIONAL_RECORD = ROOT / "port/linux/mutator_network_format.txt"
OPTIONAL_ENUMS = {
    "_distributed_message_mutators", "_distributed_message_mutator_tag_edits",
    "_distributed_message_mutator_share_ask", "_distributed_message_mutator_share_entry",
    "_distributed_message_mutator_share_fetching", "_distributed_message_mutator_share_done",
}
OPTIONAL_STRUCTS = {
    "distributed_mutator_entry", "distributed_mutators_message",
    "distributed_mutator_tag_edits_message",
}
OPTIONAL_SHARE_STRUCTS = {"mutator_share_entry_message", "port_mutator_share", "port_mutator_share_file"}
OPTIONAL_BUTTON = "DISTRIBUTED_RELAYED_ACTION_SCRIPT_BUTTONS_BIT"


def _blocks(text, opener):
    """each declaration (to its semicolon) that starts with the opener"""
    for match in re.finditer(opener, text, re.M):
        end, depth = text.index("{", match.start()), 0
        while True:
            depth += {"{": 1, "}": -1}.get(text[end], 0)
            end += 1
            if not depth:
                break
        yield text[match.start():text.index(";", end) + 1]


def _hash(text):
    return hashlib.sha256(" ".join(text.split()).encode()).hexdigest()[:12]


def items(optional=False):
    """Stock 44 by default; optional=True includes the gated mutator wire layout."""
    paths = sorted((ROOT / "port/linux/game").glob("network_*.[ch]"))
    paths += sorted((ROOT / "port/linux/game").glob("map_share*.[ch]"))
    paths += [ROOT / "port/linux/game/vehicle_riding.h"]
    paths += [ROOT / "source/networking/network_messages.c", ROOT / "source/networking/network_messages.h"]
    texts = [re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(encoding="latin-1"), flags=re.S) for path in paths]
    if optional:
        texts.insert(0, re.sub(r"/\*.*?\*/|//[^\n]*", " ",
                             (ROOT / "port/linux/game/mutator_share.c").read_text(), flags=re.S))
    everything = "\n".join(texts)
    result, structs = {}, {}

    def add(name, value):
        assert name not in result, f"{name} is declared twice"
        result[name] = str(value)

    for text in texts:
        for block in _blocks(text, r"^struct (?:distributed_\w+|mutator_share_entry_message|port_mutator_share(?:_file)?)\s*\{"):
            name = re.match(r"struct (\w+)", block).group(1)
            if optional or name not in OPTIONAL_STRUCTS | OPTIONAL_SHARE_STRUCTS:
                structs[name] = block
        for block in _blocks(text, r"^enum\b[^;{]*\{"):
            if ("_distributed_" not in block and "network_game_message_type" not in block and
                    "_vehicle_riding_deck_falling_bit" not in block):
                continue
            value = -1
            for member in filter(None, map(str.strip, block[block.index("{") + 1:block.rindex("}")].split(","))):
                name, _, expression = (part.strip() for part in member.partition("="))
                value = int(expression, 0) if expression else value + 1
                if not name.startswith("NUMBER_OF_") and (optional or name not in OPTIONAL_ENUMS):
                    add(f"enum {name}", value)
        for name, value in re.findall(r"^#define (DISTRIBUTED_\w+)\s+(.*?)\s*$", text, re.M):
            if optional or name != OPTIONAL_BUTTON:
                add(f"define {name}", value)
    sent = {name for name in structs if name.endswith("_message") or f"sizeof(struct {name})" in everything}
    while True:
        bodies = " ".join(structs[name].split("{", 1)[1] for name in sent)
        held = structs.keys() & set(re.findall(r"\bstruct (\w+)", bodies))
        if held <= sent:
            break
        sent |= held
    for name in sent:
        add(f"struct {name}", _hash(structs[name]))
    # the constants the structs' arrays are sized by, and the netcode's
    # DISTRIBUTED_ sizes in enums of no name, as their definitions' text, with
    # the names those use in turn
    bounds = set()
    for name in sent:
        for size in re.findall(r"\[([^\]]+)\]", structs[name].split("{", 1)[1]):
            bounds |= set(re.findall(r"\b[A-Z_][A-Z0-9_]*\b", size))
    bounds |= set(re.findall(r"^\s*(DISTRIBUTED_\w+)\s*=", everything, re.M)) - {
        key.split(" ", 1)[1] for key in result if key.startswith("enum ")}
    if optional:
        bounds |= {"MUTATOR_RELAYED_ACTION_MAXIMUM_SIZE", "MAXIMUM_VARIANT_ADDON_VEHICLES"}
    headers = None
    pending, done = sorted(bounds), set()
    while pending:
        name = pending.pop()
        if name in done:
            continue
        done.add(name)
        pattern = rf"^#define {name}\b[ \t]*((?:[^\n]*\\\n)*[^\n]*)|\b{name}[ \t]*=(?!=)([^,{{}};]*)"
        found = re.findall(pattern, everything, re.M)
        if not found:
            if headers is None:
                headers = "\n".join(re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(encoding="latin-1"), flags=re.S)
                                    for root in ("source", "port/linux/include", "port/linux/game", "port/linux/src")
                                    for path in sorted((ROOT / root).rglob("*.h")))
            found = re.findall(pattern, headers, re.M)
        if not found:
            # (an enum's member counted, NUMBER_OF_ at its end: the enum)
            found = [("enum " + _hash(block), "") for text in (everything, headers)
                     for block in _blocks(text, r"\benum\b[^;{]*\{")
                     if re.search(rf"\b{name}\s*[,}}]", block)][:1]
        if not found:
            assert name not in bounds, f"{name}: a size of the wire format with no definition found"
            continue
        values = sorted({" ".join((define or expression).replace("\\\n", " ").split()) for define, expression in found})
        add(f"bound {name}", " | ".join(values))
        pending += re.findall(r"\b[A-Z_][A-Z0-9_]*\b", " ".join(values))
    # the gametype's PC options, which the game settings record carries
    # whole, and where that record puts them
    engine = re.sub(r"/\*.*?\*/|//[^\n]*", " ", (ROOT / "source/game/game_engine.h").read_text(encoding="latin-1"),
                    flags=re.S)
    variant = next(_blocks(engine, r"^struct game_variant_options\s*\{"))
    if not optional:
        # Exactly four addon bytes occupy the old reserved space; never erase
        # arbitrary declarations or a changed array bound/type.
        assert re.search(r"\bMAXIMUM_VARIANT_ADDON_VEHICLES\s*=\s*2\s*,", engine)
        assert "sizeof(struct game_variant_options) == 0x2C" in engine
        variant, count = re.subn(
            r"byte addon_vehicles_fewer\[2\]\[MAXIMUM_VARIANT_ADDON_VEHICLES\];\s*byte reserved\[1\];",
            "byte reserved[5];", variant)
        assert count == 1, "unrecognized game_variant_options reserved-byte layout"
    add("struct game_variant_options", _hash(variant))
    if optional:
        protocol = (ROOT / "port/linux/game/mutator_protocol.h").read_text()
        for name, value in re.findall(r"^#define (MUTATOR_(?:CAMERA_)?PROTOCOL_\w+) ([^\n]+)", protocol, re.M):
            add(f"define {name}", value)
    limits = (ROOT / "port/linux/include/halo_port_limits.h").read_text()
    for name, value in re.findall(r"^#define (HALO_PORT_NETWORK_GAME_(?:VARIANT_OPTIONS_OFFSET|LOCAL_DATA_OFFSET|SIZE)) "
                                  r"(.*)$", limits, re.M):
        add(f"layout {name}", " ".join(value.split()))
    messages = texts[-2]
    for name, size in re.findall(r"^DEFINE_NETWORK_GAME_MESSAGE\((\w+), (.*)\);", messages, re.M):
        add(f"size {name}", " ".join(size.split()))
    for fields, name in re.findall(r"\t\{\n((?:\t\t.*\n)*?)\t\},\n\tNETWORK_GAME_MESSAGE_DEFINITION\((\w+),", messages):
        add(f"fields {name}", _hash(fields))
    workshop = (ROOT / "port/linux/game/mcc_workshop.h").read_text()
    for name, value in re.findall(r"^#define (MCC_WORKSHOP_WIRE_\w+) ([^\n]+)", workshop, re.M):
        add(f"define {name}", value)
    # MCC package sharing's request, response and manifest entry (messages 103 and 104).
    share = ""
    for header in ("mcc_share_session.h", "mcc_share_stage.h", "mcc_share_list.h"):
        share += re.sub(r"/\*.*?\*/", " ", (ROOT / "port/linux/game" / header).read_text(), flags=re.S)
    for name in ("mcc_share_request", "mcc_share_response", "mcc_share_entry"):
        add(f"struct {name}", _hash(next(_blocks(share, rf"^struct {name}\s*\{{"))))
    for name in ("mcc_share_op", "mcc_share_result", "mcc_share_file_kind"):
        add(f"enum {name}", _hash(next(_blocks(share, rf"^enum {name}\s*\{{"))))
    for name, value in re.findall(r"^#define (MCC_SHARE_(?:LEVEL_SIZE|CHUNK_BYTES|MANIFEST_PAGE|LEAF_SIZE|CREDIT_SIZE|STAGE_FILES|"
                                  r"MAXIMUM_\w+)) ([^\n]+)", share, re.M):
        add(f"define {name}", value)
    admission = re.sub(r"/\*.*?\*/", " ",
                       (ROOT / "port/linux/game/mcc_admission.h").read_text(), flags=re.S)
    for name in ("mcc_admission_identity", "mcc_admission_message"):
        add(f"struct {name}", _hash(next(_blocks(admission, rf"^struct {name}\s*\{{"))))
    add("enum mcc_admission_op", _hash(next(_blocks(admission, r"^enum mcc_admission_op\s*\{"))))
    for name, value in re.findall(r"^#define (MCC_ADMISSION_\w+) ([^\n]+)", admission, re.M):
        add(f"define {name}", value)
    loading = re.sub(r"/\*.*?\*/", " ",
                     (ROOT / "port/linux/game/mcc_load_network.h").read_text(), flags=re.S)
    add("struct mcc_load_message", _hash(next(_blocks(loading, r"^struct mcc_load_message\s*\{"))))
    for name, value in re.findall(r"^#define (MCC_LOAD_NETWORK_\w+) ([^\n]+)", loading, re.M):
        add(f"define {name}", value)
    return result


def version():
    limits = (ROOT / "port/linux/include/halo_port_limits.h").read_text()
    return int(re.search(r"#define HALO_PORT_NETWORK_VERSION (\d+)", limits).group(1))


def recorded(path=None):
    lines = [line.split(" ", 2) for line in (path or RECORD).read_text().splitlines()]
    return int(lines[0][1]), {f"{kind} {name}": value for kind, name, value in lines[1:]}


def changes(old, new):
    """(changed or removed, added)"""
    return sorted(name for name in old if old[name] != new.get(name)), sorted(name for name in new if name not in old)


def optional_version():
    return int(items(optional=True)["define MUTATOR_PROTOCOL_VERSION"])


def check():
    """Check both agreements; extension changes must never disappear in the base view."""
    problems = []
    for path, optional, current in ((RECORD, False, version()),
                                    (OPTIONAL_RECORD, True, optional_version())):
        if not path.exists():
            problems.append(f"{path.name}: missing snapshot; run network_format.py --record")
            continue
        recorded_version, old = recorded(path)
        breaking, added = changes(old, items(optional=optional))
        for names, what in ((breaking, "changed or removed (raise the version)"),
                            (added, "added (record if compatible)")):
            if names:
                problems += [f"{path.name}: {what}:"] + [f"  {name}" for name in names]
        if recorded_version != current:
            problems.append(f"{path.name}: recorded for version {recorded_version}, sources {current}")
    if problems:
        problems.append("then record them: python tools/network_format.py --record")
    netcode = (ROOT / "port/linux/NETCODE.md").read_text()
    if not re.search(rf"\bversion\s+{version()}\b", netcode):
        problems.append(f"port/linux/NETCODE.md's versions say nothing of version {version()}")
    return problems


def record():
    updates = []
    for path, optional, current in ((RECORD, False, version()),
                                    (OPTIONAL_RECORD, True, optional_version())):
        new = items(optional=optional)
        if path.exists():
            recorded_version, old = recorded(path)
            breaking = changes(old, new)[0]
            if breaking and recorded_version == current:
                sys.exit(f"{path.name}: changed or removed: " + ", ".join(breaking) +
                         "\nraise the corresponding protocol version first")
        updates.append((path, f"version {current}\n" +
                        "".join(f"{name} {new[name]}\n" for name in sorted(new))))
    # Validate both before updating either snapshot.
    for path, text in updates:
        path.write_text(text)


if __name__ == "__main__":
    if sys.argv[1:] == ["--record"]:
        record()
    else:
        print("\n".join(check()) or "the wire format is the one recorded")
