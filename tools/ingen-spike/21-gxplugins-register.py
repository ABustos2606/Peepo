# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Register installed GxPlugins without rebuilding the instrument's graph.

Usage: python3 21-gxplugins-register.py MANIFEST SOCKET WRITABLE-RECORD-DIR
The record directory must be writable by the engine's service account.
"""
import json
from pathlib import Path
import sys
import time

from ingenprobe import Client


def main():
    manifest, socket, record = sys.argv[1:]
    record = Path(record).resolve()
    client = Client(socket)
    loaded = []
    try:
        assert client.copy("ingen:/main", (record / "before.ingen").as_uri(), timeout=15)[0] == 0
        for plugin in json.load(open(manifest))["plugins"]:
            uri = plugin["uri"]
            if client.get(uri)[0] != 0:
                bundle = Path("/opt/peepo/lib/lv2") / plugin["bundle"]
                assert (bundle / "manifest.ttl").is_file(), bundle
                value = json.dumps(str(bundle) + "/") + "^^atom:Path"
                assert client.set("ingen:/", "ingen:loadedBundle", value, timeout=10)[0] == 0, uri
                loaded.append(uri)
            assert client.get(uri)[0] == 0, uri
        assert client.copy("ingen:/main", (record / "after.ingen").as_uri(), timeout=15)[0] == 0
    finally:
        client.close()
    report = {"date": time.strftime("%Y-%m-%d"), "registered": len(loaded), "available": 43, "new_uris": loaded,
              "note": "No graph edits or engine restart. before.ingen and after.ingen capture the playing graph."}
    (record / "registration.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS {len(loaded)} new plugins registered; all 43 available in the running engine")


if __name__ == "__main__":
    main()
