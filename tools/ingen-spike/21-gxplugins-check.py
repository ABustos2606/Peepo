# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Audio and saved-control checks for GxPlugins on an isolated Ingen/JACK.

Reuses the catalogue's signal/save/restore probe. Never point this at the
instrument's engine: 21-gxplugins-check.sh creates the separate test session.
"""
import importlib.util
import json
import os
from pathlib import Path
import sys
import time

spec = importlib.util.spec_from_file_location("compat", Path(__file__).with_name("15-compat.py"))
compat = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compat)


def main():
    catalogue_path, live_path, result_path, work = sys.argv[1:]
    catalogue = json.load(open(catalogue_path))["modules"]
    live = compat.live_results(live_path)
    manifest = json.load(open(Path(__file__).resolve().parents[2] / "config/catalogue/gxplugins.json"))
    expected = {p["uri"] for p in manifest["plugins"]}
    assert len(catalogue) == 43 and {p["uri"] for p in catalogue} == expected
    os.makedirs(work, exist_ok=True)
    rig, client = compat.Rig(), compat.Client()
    results = []
    try:
        for name, kind in (("ain", "lv2:InputPort , lv2:AudioPort"), ("aout", "lv2:OutputPort , lv2:AudioPort"),
                           ("cin", "lv2:InputPort , lv2:CVPort"), ("cout", "lv2:OutputPort , lv2:CVPort")):
            assert client.put("ingen:/main/" + name, "a " + kind)[0] == 0
        time.sleep(0.5)
        for entry in catalogue:
            result = {"uri": entry["uri"], "name": entry["name"], "live": live.get(entry["uri"], ["MISSING", ""])}
            result.update(compat.exercise(client, rig, entry, work))
            result["passed"] = (entry["availability"] == "available" and result["live"][0] == "PASS"
                                and result.get("finite") is True and result.get("signal_ok") is True
                                and result.get("restore_ok") is not False)
            print(("PASS" if result["passed"] else "FAIL") + " " + entry["name"] + ": " + result.get("signal", ""), flush=True)
            results.append(result)
    finally:
        client.close()
        rig.client.deactivate()
        rig.client.close()
    report = {"date": time.strftime("%Y-%m-%d"), "machine": os.uname().machine, "sample_rate": rig.rate,
              "collection_commit": manifest["collection_commit"], "passed": sum(r["passed"] for r in results),
              "total": len(results), "entries": results}
    Path(result_path).write_text(json.dumps(report, indent=2) + "\n")
    print(f"SUMMARY {report['passed']}/{report['total']} GxPlugins passed")
    return 0 if report["passed"] == 43 else 1


if __name__ == "__main__":
    sys.exit(main())
