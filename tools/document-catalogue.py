# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Regenerate the complete user-facing module reference from a Pi export.

    python tools/document-catalogue.py
    python tools/document-catalogue.py --catalogue path/to/pi-export.json --date YYYY-MM-DD

Only module presentation fields are interpreted here, in the same order as
apply_catalogue_overlay: names, descriptions, categories, hidden, availability and
the spill bypass.
No plugin compatibility or control definitions are inferred by this documenter.
"""
import argparse
from collections import Counter
import copy
from datetime import date
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = ["Drive", "Amp", "Cabinet", "Dynamics", "Filter", "Modulation", "Pitch", "Delay", "Reverb",
              "Texture", "Resonator", "Sound source", "Control", "Utility"]
RANK = {"available": 0, "untested": 1, "unavailable": 2}
MISSING_NAMES = {
    "adenv": "Attack/decay envelope", "adenv_lvl": "Attack/decay envelope with levels",
    "i4o3": "Matrix mixer (4 in, 3 out)", "mclk": "MIDI clock", "controller": "MIDI controller",
    "bitcrusher": "Bitcrusher", "onset": "Onset detector", "pitch": "Pitch detector",
    "loop_common_in": "Looper input", "loop_common_out": "Looper output",
    "loop_extra_midi": "Looper extra MIDI", "loop_midi_out": "Looper MIDI output",
    "midi_pc": "MIDI program change", "midi_to_cv_mono": "MIDI to CV (mono)",
    "midi_to_cv_poly": "MIDI to CV (poly)", "poly_3_band_eq": "Three-band EQ",
    "polyfoot": "Original Poly foot switches", "stone-phaser-ext": "Phaser (CV)",
    "stone-phaser-stereo-ext": "Phaser stereo (CV)", "wet_dry": "Wet/dry (mono)",
    "wet_dry_stereo": "Wet/dry (stereo)",
}


# Our own modules whose own switch lets the tail ring on, from the tail check
# (P13.5, docs/evidence/scenes-results.md). Modules with no tail at their
# defaults (Eureka Bloom, Crinkle, Prism) and Loo-pi, a looper, are not listed.
OWN_TAILS = {
    "https://github.com/ABustos2606/aura#aura", "https://github.com/ABustos2606/discipline#discipline",
    "https://github.com/ABustos2606/drizzle#drizzle", "https://github.com/ABustos2606/euclidelay#euclidelay",
    "https://github.com/ABustos2606/eureka#skip", "https://github.com/ABustos2606/eureka#spool",
    "https://github.com/ABustos2606/eureka#hush", "https://github.com/ABustos2606/eureka#slowmo",
    "https://github.com/ABustos2606/eureka#granny",
}


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def cell(value):
    return str(value).replace("|", "\\|").replace("\n", " ").replace("\r", " ")


def present(module, rules):
    module = copy.deepcopy(module)
    for rule in rules:
        matches = module["uri"] == rule["uri"] if "uri" in rule else rule["match"] in module["uri"]
        if not matches:
            continue
        prefix = rule.get("strip_prefix", "")
        if prefix and len(module["name"]) > len(prefix) and module["name"].startswith(prefix):
            module["name"] = module["name"][len(prefix):]
        for key in ("name", "description", "category", "bypass"):
            if rule.get(key):
                module[key] = rule[key]
        if "hidden" in rule:
            module["hidden"] = rule["hidden"]
            if rule.get("reason"):
                module["reason"] = rule["reason"]
        if "availability" in rule and RANK[rule["availability"]] > RANK[module["availability"]]:
            module["availability"] = rule["availability"]
            module["reason"] = rule.get("reason", "")
    return module


def state(module):
    if module.get("hidden"):
        return "Hidden"
    return {"available": "Ready", "untested": "Needs adaptation", "unavailable": "Unavailable"}[module["availability"]]


def connections(module):
    parts = []
    for kind, label in (("audio", "audio"), ("cv", "CV"), ("midi", "MIDI/events")):
        counts = Counter(p["direction"] for p in module["ports"] if p["type"] == kind)
        if counts:
            parts.append(f"{label} {counts['input']} in / {counts['output']} out")
    return "; ".join(parts) or "No patchable connectors"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalogue", type=Path, default=ROOT / "docs/evidence/catalogue-pi-20261022.json")
    parser.add_argument("--date", default="2026-10-22")
    args = parser.parse_args()
    recorded = date.fromisoformat(args.date).strftime("%d %B %Y").lstrip("0")
    export = read_json(args.catalogue)
    rules = read_json(ROOT / "config/catalogue/overlay.json")["rules"]
    inventory = [line.split("  #", 1)[0].strip() for line in
                 (ROOT / "config/catalogue/inventory.txt").read_text(encoding="utf-8").splitlines()
                 if line.strip() and not line.startswith("#")]
    assert len(inventory) == len(set(inventory)), "Duplicate inventory identities"
    installed = {m["uri"]: present(m, rules) for m in export["modules"]}
    assert set(installed) <= set(inventory), "Pi export includes unselected identities"
    hidden = sorted((m for m in installed.values() if m.get("hidden")), key=lambda m: m["name"].casefold())
    visible = [m for m in installed.values() if not m.get("hidden")]
    missing = [uri for uri in inventory if uri not in installed]
    counts = Counter(state(m) for m in installed.values())
    assert sum(counts.values()) + len(missing) == len(inventory)
    gx = read_json(ROOT / "config/catalogue/gxplugins.json")["plugins"]
    gx_uris = {p["uri"] for p in gx}
    assert len(gx_uris) == 43 and gx_uris <= set(installed)
    assert all(state(installed[uri]) == "Ready" for uri in gx_uris)
    expansion = read_json(ROOT / "config/catalogue/patchstorage.json")["entries"]
    all_gx_uris = gx_uris | {p["uri"] for entry in expansion for p in entry["plugins"]
                            if p["uri"].startswith("http://guitarix.sourceforge.net/plugins/")}
    assert all(uri in installed and state(installed[uri]) == "Ready" for uri in all_gx_uris)
    technical = {p["uri"]: p for p in read_json(ROOT / "docs/evidence/compatibility.json")["entries"]}
    reference = read_json(ROOT / "docs/lv2-plugin-comparison-data.json")
    source_names = {p["uri"]: p["name"] for p in reference["poly_plugins"]}
    mappings = {p["uri"]: p for p in reference["beebo_mappings"]}
    snapshot = args.catalogue.resolve().relative_to(ROOT / "docs").as_posix()
    lines = ["# Complete module list", "", f"**Updated {recorded}.** This is the full selected Peepo module inventory, using the names and categories shown on the touchscreen.", "",
             f"**{len(inventory)} plugin identities:** {len(installed)} installed in the Pi export and {len(missing)} not installed. All **{len(all_gx_uris)} selected Guitarix identities**, including the original **43 GxPlugins**, and the **three retained Dragonfly reverbs** are included.", "",
             "**Removed effects:** Bollie Delay XT and Faust Granulator have been removed from Peepo and this inventory. The Granulator - Mono and Granulator - Stereo entries below are separate AVW plugins. [Removal details](35-patchstorage-expansion.md).", "",
             "**First catalogue purge:** Gx Live looper, Ambience, MVerb, Gx Reverb stereo, Gx Room simulator, Gx Zita reverb stereo and Dragonfly Early Reflections are retired. IR effects now use the Cabinet and IR Reverb families; alternate convolution identities are hidden for older patches. [Purge details](36-catalogue-purge.md).", "",
             "The tables below list every identity once. IN and OUT are fixed patch endpoints, described separately.", "",
             "## Availability", "", "| Status | Count | Meaning |", "| --- | ---: | --- |",
             f"| Ready | {counts['Ready']} | Can be added to a patch. This does not imply a guitar listening test of every module. |",
             f"| Needs adaptation | {counts['Needs adaptation']} | Installed, but disabled in Add for the stated limitation. |",
             f"| Unavailable | {counts['Unavailable']} | Present in the Pi export but unusable by the current host; see each reason. |",
             f"| Hidden | {len(hidden)} | Omitted from Add as a duplicate or redundant module; retained for saved patches. |",
             f"| Not installed | {len(missing)} | Selected reference identity without an installed implementation. |", "",
             "**File-based effects:** NAM, Cabinet and IR Reverb have file selectors. Each IR family offers Mono, Mono to Stereo and Stereo configurations. Stereo accepts mono/stereo IRs for separate left/right processing; a four-channel IR adds cross-channel responses. Alternate convolvers remain hidden for older patches. [File handling](29-parameters-files-tempo.md#model-and-ir-files-p83).", "",
             "**Tails when bypassed:** a delay, reverb or texture module marked *tail rings on when bypassed* keeps sounding its echoes or reverb, without a click, when it is switched off or a scene turns it off; the rest stop at once. Each was marked after passing the tail check, Peepo's own modules through their own switch (Discipline from 0.4.1, with Trails on by default). [Scenes](44-scenes.md#spill-bypass-in-the-engine).", "",
             "**Reading connections:** counts are plugin connectors before Peepo's mono/stereo routing. CV connectors carry modulation. MIDI/events includes LV2 event/message ports, including file-selection messages; it does not imply implemented musical MIDI routing.", "",
             "## Browse by category", "", "| Category | Ready / listed |", "| --- | ---: |"]
    categories = [c for c in CATEGORIES if any(m["category"] == c for m in visible)]
    categories += sorted({m["category"] for m in visible} - set(categories))
    for category in categories:
        group = [m for m in visible if m["category"] == category]
        anchor = category.lower().replace(" ", "-")
        lines.append(f"| [{category}](#{anchor}) | {sum(state(m) == 'Ready' for m in group)} / {len(group)} |")
    lines += ["", "Also: [hidden identities](#hidden-identities), [not installed](#not-installed), [fixed endpoints](#fixed-endpoints), [all 43 GxPlugins](32-gxplugins.md#all-43-gxplugins).", ""]
    listed_uris = []
    for category in categories:
        lines += [f"## {category}", "", "| Module | What it does | Connections | Status |", "| --- | --- | --- | --- |"]
        for module in sorted((m for m in visible if m["category"] == category), key=lambda m: (m["name"].casefold(), m["uri"])):
            listed_uris.append(module["uri"])
            name = module["name"] + (" (GxPlugins)" if module["uri"] in gx_uris else "")
            status = state(module)
            if status != "Ready":
                status += ": " + module.get("reason", "See compatibility record.")
            elif module.get("bypass") == "spill" or module["uri"] in OWN_TAILS:
                status += "; tail rings on when bypassed"
            lines.append("| " + " | ".join(map(cell, (name, module.get("description") or "Description not supplied by the plugin.", connections(module), status))) + " |")
        lines.append("")
    lines += ["## Hidden identities", "", "| Module | Category | Why it is hidden | Plugin identity |", "| --- | --- | --- | --- |"]
    for module in hidden:
        listed_uris.append(module["uri"])
        reason = module.get("reason") or ("Duplicate tuner; use Tuner." if "tuna#" in module["uri"] else "Redundant clean boost; use Gain.")
        lines.append("| " + " | ".join(map(cell, (module["name"], module["category"], reason, "`"+module["uri"]+"`"))) + " |")
    lines += ["", "## Not installed", "", "These are reference identities, not available effects. The installed Foot switch, mixers and other modules can cover some use cases without implementing the original identity.", "",
              "| Reference module | Reason / reference description | Plugin identity |", "| --- | --- | --- |"]
    missing_rows = []
    for uri in missing:
        listed_uris.append(uri)
        key = re.split(r"[/#]", uri)[-1]
        name = MISSING_NAMES.get(key, source_names.get(uri, key))
        mapping = mappings.get(uri, {})
        reason = technical.get(uri, {}).get("why_missing", "No installed implementation in this Pi export.")
        if mapping.get("description"):
            reason = reason.rstrip(". ") + ". Reference: " + mapping["description"]
        missing_rows.append((name, reason, "`" + uri + "`"))
    for row in sorted(missing_rows, key=lambda r: r[0].casefold()):
        lines.append("| " + " | ".join(map(cell, row)) + " |")
    lines += ["", "## Fixed endpoints", "", "| Endpoint | Purpose |", "| --- | --- |",
              "| IN | Stereo input from the audio interface. |", "| OUT | Stereo output to the audio interface. |", "",
              "## Source and updates", "",
              f"Generated by `tools/document-catalogue.py` from the [Pi export]({snapshot}), [presentation overlay](../config/catalogue/overlay.json) and [selected inventory](../config/catalogue/inventory.txt). The export contains installed identities; the overlay supplies Peepo's names, categories, hidden flags and restrictions. Exact URIs and control metadata for installed modules are in the export.", "",
              "The older [compatibility matrix](evidence/compatibility-matrix.md) is a historical test record. [GxPlugins results](32-gxplugins.md) supplement it. Availability in this list follows the recorded Pi export and current overlay, not the historical PC matrix.", "",
              "Regenerate after saving a new Pi export and updating the overlay:", "", "```bash",
              "python tools/document-catalogue.py",
              "# For another recorded export:",
              "python tools/document-catalogue.py --catalogue docs/evidence/catalogue-pi-YYYYMMDD.json --date YYYY-MM-DD",
              "```", ""]
    assert len(listed_uris) == len(set(listed_uris)) == len(inventory)
    assert set(listed_uris) == set(inventory)
    (ROOT / "docs/33-module-list.md").write_text("\n".join(lines), encoding="utf-8", newline="\n")
    gx_lines = ["<!-- gxplugins-list:start -->", "## All 43 GxPlugins", "",
                "These are the names shown in Peepo. All 43 are installed and ready to add; see the [complete module list](33-module-list.md) for the rest of the instrument.", "",
                "| Module in Peepo | Category | What it does |", "| --- | --- | --- |"]
    for plugin in sorted(gx, key=lambda p: (installed[p["uri"]]["category"], installed[p["uri"]]["name"].casefold())):
        module = installed[plugin["uri"]]
        gx_lines.append("| " + " | ".join(map(cell, (module["name"], module["category"], module["description"]))) + " |")
    gx_lines += ["", "<!-- gxplugins-list:end -->", ""]
    gx_path = ROOT / "docs/32-gxplugins.md"
    gx_doc = gx_path.read_text(encoding="utf-8")
    if "<!-- gxplugins-list:start -->" in gx_doc:
        gx_doc = re.sub(r"<!-- gxplugins-list:start -->.*?<!-- gxplugins-list:end -->\n", "\n".join(gx_lines), gx_doc, flags=re.S)
    else:
        gx_doc = gx_doc.replace("## Build and verification", "\n".join(gx_lines) + "\n## Build and verification", 1)
    gx_path.write_text(gx_doc, encoding="utf-8", newline="\n")
    print(f"Wrote all {len(inventory)} identities: {dict(counts)}, {len(missing)} not installed")


if __name__ == "__main__":
    main()
