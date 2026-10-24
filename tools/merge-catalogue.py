# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Merge one family's freshly exported records into an existing catalogue.

    python tools/merge-catalogue.py EXISTING FRESH OUTPUT --uri URI [--uri URI ...]
                                    [--provenance key=value ...]

The WSL fixture and the Pi exports come from installations with different
plugin families, so a new family is exported alone (peepo_catalogue_export
--only) and merged in: its records are replaced or added, every other record
is kept byte for byte, and the merge is refused unless the fresh export holds
exactly the named identities and each is available.
"""
import argparse
import json
from pathlib import Path
import re


def indented(record):
    return json.dumps(record, indent=2, ensure_ascii=True).replace("\n", "\n    ")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("existing", type=Path)
    parser.add_argument("fresh", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--uri", action="append", required=True)
    parser.add_argument("--provenance", action="append", default=[], help="key=value added to the provenance")
    args = parser.parse_args()
    text = args.existing.read_text(encoding="utf-8")
    original = json.loads(text)
    exported = json.loads(args.fresh.read_text(encoding="utf-8"))
    wanted = set(args.uri)
    fresh = {m["uri"]: m for m in exported["modules"]}
    if set(fresh) != wanted or len(exported["modules"]) != len(wanted):
        raise SystemExit("the fresh export must hold exactly: " + ", ".join(sorted(wanted)))
    for m in fresh.values():
        if m["availability"] != "available":
            raise SystemExit(f"{m['uri']} is {m['availability']}: {m.get('reason', '')}")
    # Replace the family's records where they are; keep the others' text.
    start = re.search(r'"modules"\s*:\s*\[', text).end()
    cursor, edits, decoder = start, [], json.JSONDecoder()
    while True:
        while text[cursor] in " \r\n\t,":
            cursor += 1
        if text[cursor] == "]":
            break
        record, end = decoder.raw_decode(text, cursor)
        if record["uri"] in fresh:
            edits.append((cursor, end, indented(fresh.pop(record["uri"]))))
        cursor = end
    for begin, end, replacement in reversed(edits):
        text = text[:begin] + replacement + text[end:]
    if fresh:
        addition = ",\n    ".join(indented(m) for m in fresh.values())
        text = text[:start] + "\n    " + addition + "," + text[start:]
    for item in args.provenance:
        key, _, value = item.partition("=")
        pair = json.dumps(key) + ": " + json.dumps(value)
        if re.search(rf'"{re.escape(key)}"\s*:\s*"[^"]*"', text):
            text = re.sub(rf'"{re.escape(key)}"\s*:\s*"[^"]*"', lambda _: pair, text)
        else:
            at = re.search(r'"provenance"\s*:\s*\{', text).end()
            text = text[:at] + "\n    " + pair + "," + text[at:]
    merged = json.loads(text)
    kept = [m for m in original["modules"] if m["uri"] not in wanted]
    assert [m for m in merged["modules"] if m["uri"] not in wanted] == kept
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8", newline="\n")
    print(f"{args.output}: {len(merged['modules'])} modules, {len(wanted)} merged")


if __name__ == "__main__":
    main()
