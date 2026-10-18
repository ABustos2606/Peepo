# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""One compatibility record per inventory entry (P9.1, P9.3).

For every plugin in config/catalogue/inventory.txt: where it came from, whether
it is installed, its binary and libraries, the LV2 features it requires, its
ports, what the catalogue export decided, whether it loads and matches in the
engine (the output of peepo_catalogue_live), and for offered entries a signal
check and a save and reload of a changed control through a bundle. Writes
docs/evidence/compatibility.json and docs/evidence/compatibility-matrix.md.
Needs a running engine (see env.sh) and python3-lilv.
    python3 15-compat.py <catalogue.json> <catalogue-live output> <work directory>
"""
import json
import math
import os
import re
import shutil
import subprocess
import sys
import threading
import time
import urllib.parse

import jack
import lilv
import numpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ingenprobe import Client

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
INVENTORY = os.path.join(ROOT, "config", "catalogue", "inventory.txt")
DATA = os.path.join(ROOT, "docs", "lv2-plugin-comparison-data.json")
OUT_JSON = os.path.join(ROOT, "docs", "evidence", "compatibility.json")
OUT_MD = os.path.join(ROOT, "docs", "evidence", "compatibility-matrix.md")
LV2 = "http://lv2plug.in/ns/lv2core#"
ATOM = "http://lv2plug.in/ns/ext/atom#"
PATCH = "http://lv2plug.in/ns/ext/patch#"
STATE = "http://lv2plug.in/ns/ext/state#"
MIDI_EVENT = "http://lv2plug.in/ns/ext/midi#MidiEvent"
BLOCK = "ingen:/main/probe"

# Results that need more than a load and a signal to judge, with the evidence.
FINDINGS = {
    "http://polyeffects.com/lv2/polyclouds#beat_repeat": ("needs-adaptation",
        "Most controls have no audible effect: the port never sets the beat-repeat settings (probability, slices, pitch) the firmware derives from its knobs. See 11-clouds.sh."),
    "http://github.com/mikeoliphant/neural-amp-modeler-lv2": ("needs-adaptation",
        "Silent passthrough until a model file is chosen; the interface has no file selector yet (P8.3). Host checks in 13-nam.sh."),
}
for name in ("vco2_audio", "vco2_cv", "scope"):
    FINDINGS["http://avwlv2.sourceforge.net/plugins/avw/" + name] = ("needs-adaptation",
        "Upstream metadata does not parse (%s.ttl uses an undefined param: prefix), so its ports are unknown to the host" % name)
# Plugins that have crashed the engine. The live check leaves them out, since
# one crash ends it for every entry after.
CRASHES = {
    "http://polyeffects.com/lv2/bitcrushed_delay":
        "Crashed the engine (a segfault in its own run()) in 4 of 7 runs: during the live check, and twice when "
        "re-created and its bit depth changed; the cause is not established",
}
FINDINGS.update({uri: ("needs-adaptation", why) for uri, why in CRASHES.items()})
FINDINGS["http://moddevices.com/plugins/mod-devel/mod-cv-meter"] = ("needs-adaptation",
    "Upstream metadata does not parse (mod-cv-meter.ttl has an unterminated string), so its ports are unknown to the host")

# Why an inventory entry is not installed, where it is not simply unbuilt.
MISSING = {
    "http://drobilla.net/plugins/omins/": "Debian's omins package is LADSPA only; its LV2 port is not packaged",
    "http://gareus.org/oss/lv2/mclk": "x42 MIDI clock: not in Debian's x42-plugins; MIDI only, so not built while MIDI routing is deferred",
    "http://gareus.org/oss/lv2/matrixmixer#i4o3": "MOD's 4-in 3-out build of x42's matrix mixer; Debian ships only the 8 x 8 one",
    "http://github.com/blablack/midimsg-lv2/": "midimsg-lv2 is not packaged; MIDI only, so not built while MIDI routing is deferred",
}

# Third-party sources built by 14-more-plugins.sh, by bundle name.
SOURCES = {"tap-": "moddevices/tap-lv2", "stone-phaser": "jpcima/stone-phaser", "ykchorus": "SpotlightKid/ykchorus",
           "MVerb": "DISTRHO/MVerb", "envfollower": "ssj71/infamousPlugins", "powercut": "ssj71/infamousPlugins",
           "stuck": "ssj71/infamousPlugins", "mod-cv": "moddevices/mod-cv-plugins", "gx_ampegsvt": "brummer10/GxSVT.lv2",
           "gx_slowgear": "brummer10/GxSlowGear.lv2", "gx_supersonic": "brummer10/GxSuperSonic.lv2",
           "gx_CreamMachine": "brummer10/GxCreamMachine.lv2"}
GX_SOURCES = {p["uri"]: "brummer10/" + p["repo"] for p in json.load(open(
    os.path.join(ROOT, "config", "catalogue", "gxplugins.json"), encoding="utf-8"))["plugins"]}
MORPH = "http://lv2plug.in/ns/ext/morph#MorphPort"


def inventory():
    entries = []
    for line in open(INVENTORY):
        # Addresses can hold a '#'; a comment is two spaces and a '#'.
        uri = line.split("  #", 1)[0].strip() if line.startswith("http") else ""
        if uri:
            entries.append(uri)
    return entries


class Rig:
    def __init__(self):
        self.client = jack.Client("compat-probe")
        self.out = self.client.outports.register("src")
        self.inp = self.client.inports.register("cap")
        self.rate = self.client.samplerate
        self.phase, self.lock, self.frames, self.count = 0.0, threading.Lock(), [], 0
        self.client.set_process_callback(self.process)
        self.client.activate()

    def process(self, n):
        step = 2 * math.pi * 220.0 / self.rate
        self.out.get_array()[:] = 0.25 * numpy.sin(self.phase + step * numpy.arange(n))
        self.phase = (self.phase + step * n) % (2 * math.pi)
        with self.lock:
            self.frames.append(self.inp.get_array().copy())
            del self.frames[:-1000]
            self.count += 1

    def record(self, seconds, settle=0.3):
        time.sleep(settle)
        with self.lock:
            mark = self.count
        time.sleep(seconds)
        with self.lock:
            # Periods since the mark, counted, not read off the capped list.
            fresh = min(self.count - mark, len(self.frames))
            taken = self.frames[len(self.frames) - fresh:] if fresh else []
        return numpy.concatenate(taken) if taken else numpy.zeros(1)


def each(collection):
    """Debian's Lilv bindings give collections an iterator Python cannot step."""
    if collection is None:
        return []
    out, it = [], collection.begin()
    while not it.is_end():
        out.append(it.next())
    return out


def binary_facts(path):
    facts = {"binary": path}
    if not path or not os.path.exists(path):
        facts["binary_problem"] = "binary missing"
        return facts
    header = subprocess.run(["readelf", "-h", path], capture_output=True, text=True).stdout
    machine = re.search(r"Machine:\s+(.*)", header)
    facts["machine"] = machine.group(1).strip() if machine else "unknown"
    needed = re.findall(r"\(NEEDED\).*\[(.*)\]", subprocess.run(["readelf", "-d", path], capture_output=True, text=True).stdout)
    facts["needs"] = needed
    missing = [line.split()[0] for line in subprocess.run(["ldd", path], capture_output=True, text=True).stdout.splitlines() if "not found" in line]
    if missing:
        facts["binary_problem"] = "missing libraries: " + ", ".join(missing)
    return facts


def origin_of(uri, bundle, data_sources):
    if uri in GX_SOURCES:
        return GX_SOURCES[uri]
    if uri in data_sources:
        return "Poly: " + ", ".join(data_sources[uri])
    path = bundle.replace("file://", "")
    # Built from source into the prefix; Debian's own guitarix bundles share names.
    if path.startswith("/opt/peepo/"):
        for key, source in SOURCES.items():
            if key in os.path.basename(path.rstrip("/")):
                return source
    owner = subprocess.run(["dpkg", "-S", path.rstrip("/")], capture_output=True, text=True).stdout.split(":", 1)[0]
    if owner:
        version = subprocess.run(["dpkg-query", "-W", "-f=${Version}", owner], capture_output=True, text=True).stdout
        return "Debian %s %s" % (owner, version)
    return path


def metadata(world, plugin):
    n = world.new_uri
    ports = {"audio_in": 0, "audio_out": 0, "cv_in": 0, "cv_out": 0, "control_in": 0, "control_out": 0, "atom_in": 0, "atom_out": 0}
    midi, morph = False, 0
    for i in range(plugin.get_num_ports()):
        port = plugin.get_port_by_index(i)
        side = "in" if port.is_a(n(LV2 + "InputPort")) else "out"
        morph += 1 if port.is_a(n(MORPH)) else 0
        for kind, name in (("AudioPort", "audio"), ("CVPort", "cv"), ("ControlPort", "control")):
            if port.is_a(n(LV2 + kind)):
                ports["%s_%s" % (name, side)] += 1
        if port.is_a(n(ATOM + "AtomPort")):
            ports["atom_" + side] += 1
            midi = midi or port.supports_event(n(MIDI_EVENT))
    files = []
    for writable in each(plugin.get_value(n(PATCH + "writable"))):
        ranges = each(world.find_nodes(writable, n("http://www.w3.org/2000/01/rdf-schema#range"), None))
        if any(str(r) == ATOM + "Path" for r in ranges):
            files.append(str(writable))
    return {
        "name": str(plugin.get_name()),
        "class": str(plugin.get_class().get_label()),
        "bundle": str(plugin.get_bundle_uri()),
        "required_features": sorted(str(f) for f in each(plugin.get_required_features())),
        "extension_data": sorted(str(e) for e in each(plugin.get_extension_data())),
        "ports": ports,
        "midi": midi,
        "morph_ports": morph,
        "file_parameters": files,
    }


def live_results(path):
    results = {}
    for line in open(path):
        found = re.match(r"(PASS|FAIL|SKIP)\s+.*?<([^>]+)> -- (.*)", line.strip())
        if found:
            results[found.group(2)] = (found.group(1), found.group(3))
    return results


def value_of(c, path):
    found = re.search(r'ingen:value "?(-?[0-9.eE+-]+)', c.get(path)[1])
    return float(found.group(1)) if found else None


def exercise(c, rig, entry, work):
    """Signal through the module and a changed control back from a bundle."""
    ports = entry["ports"]
    enable = entry.get("enable") or ""
    inputs = [p for p in ports if p["direction"] == "input" and p["type"] in ("audio", "cv")]
    outputs = [p for p in ports if p["direction"] == "output" and p["type"] in ("audio", "cv")]
    result = {}
    # Loading a saved graph re-creates its ports in JACK, which drops the
    # test signal's connections; make them again for every entry.
    for port in ("ingen:ain", "ingen:cin"):
        if port not in [p.name for p in rig.client.get_all_connections("compat-probe:src")]:
            rig.client.connect("compat-probe:src", port)
    if c.put(BLOCK, "a ingen:Block ;\n\t\tlv2:prototype <%s>" % entry["uri"])[0] != 0:
        return {"signal": "did not load"}
    if enable:
        c.set(BLOCK + "/" + enable, "ingen:value", "\"1.0\"^^xsd:float")
    audio_in = next((p for p in inputs if p["type"] == "audio"), None) or (inputs[0] if inputs else None)
    audio_out = next((p for p in outputs if p["type"] == "audio"), None) or (outputs[0] if outputs else None)
    if audio_in:
        c.connect("ingen:/main/%s" % ("ain" if audio_in["type"] == "audio" else "cin"), "%s/%s" % (BLOCK, audio_in["symbol"]))
    if audio_out:
        c.connect("%s/%s" % (BLOCK, audio_out["symbol"]), "ingen:/main/%s" % ("aout" if audio_out["type"] == "audio" else "cout"))
        rig.client.connect("ingen:%s" % ("aout" if audio_out["type"] == "audio" else "cout"), "compat-probe:cap")
        out = rig.record(1.0)
        finite = bool(numpy.isfinite(out).all())
        level = float(numpy.sqrt(numpy.mean(numpy.square(out.astype(numpy.float64))))) if finite else float("nan")
        moves = float(numpy.std(out)) if finite else float("nan")
        heard = finite and (level > 1e-3 or moves > 1e-3)
        result["signal"] = "%s: %s %s -> %s %s, %.4f RMS%s" % (
            "heard" if heard else ("non-finite output" if not finite else "silent"),
            audio_in["type"] if audio_in else "no input", audio_in["symbol"] if audio_in else "",
            audio_out["type"], audio_out["symbol"], level if finite else 0, "" if finite else " (NaN or inf)")
        result["signal_ok"] = heard
        result["finite"] = finite
        rig.client.disconnect("ingen:%s" % ("aout" if audio_out["type"] == "audio" else "cout"), "compat-probe:cap")
    else:
        result["signal"] = "no audio or CV output to measure"
        result["signal_ok"] = False
    # Save and reload a changed control.
    continuous = [p for p in entry["parameters"] if p["kind"] == "continuous" and p["maximum"] > p["minimum"]]
    if continuous:
        p = continuous[0]
        target = p["minimum"] + 0.37 * (p["maximum"] - p["minimum"])
        c.set("%s/%s" % (BLOCK, p["symbol"]), "ingen:value", "\"%r\"^^xsd:float" % target)
        bundle = os.path.join(work, "%d.ingen" % abs(hash(entry["uri"])))
        saved = c.copy("ingen:/main", "file://" + bundle, timeout=10)[0] == 0
        c.delete(BLOCK)
        time.sleep(0.2)
        # Loaded back as a subgraph, leaving the test graph alone: loaded over
        # the root graph, the bundle's graph ports would land on the existing
        # ones (their properties came back doubled).
        restored = "ingen:/main/restored"
        loaded = saved and c.copy("file://" + bundle, restored, timeout=10)[0] == 0
        time.sleep(0.3)
        back = value_of(c, "%s/probe/%s" % (restored, p["symbol"])) if loaded else None
        c.delete(restored)
        result["restore_ok"] = back is not None and abs(back - target) <= 1e-5 * max(1.0, abs(target))
        result["restore"] = "%s %s: set %.4g, %s" % ("restored" if result["restore_ok"] else "NOT restored", p["symbol"], target,
                                                     "read back %.4g" % back if back is not None else ("bundle did not load" if saved else "save refused"))
    else:
        result["restore_ok"] = None
        result["restore"] = "no continuous control to change"
    c.delete(BLOCK)
    time.sleep(0.2)
    return result


def classify(record):
    uri = record["uri"]
    if not record.get("installed"):
        return "unavailable", record.get("why_missing", "not installed")
    if record.get("binary_problem"):
        return "unavailable", record["binary_problem"]
    if uri in FINDINGS:
        return FINDINGS[uri]
    export = record.get("export", {})
    if record.get("morph_ports") and (export.get("availability") != "available" or record.get("live", [""])[0] != "PASS"):
        return "needs-adaptation", ("%d morph ports (the host chooses control or CV); the export and the engine disagree on their type"
                                    % record["morph_ports"])
    if export.get("availability") == "unavailable" and record.get("midi") and "no audio or control" in export.get("reason", ""):
        return "needs-adaptation", "MIDI only: waits for MIDI routing, which is deferred (D16)"
    if export.get("availability") == "unavailable":
        return "needs-adaptation", export.get("reason", "withheld by the export")
    if export.get("availability") == "untested":
        return "needs-adaptation", export.get("reason", "withheld by the export")
    if "availability" not in export:
        return "not-tested", "installed but not in the export"
    live = record.get("live", ["", ""])
    if live[0] != "PASS":
        return "needs-adaptation", "engine check failed: " + live[1]
    if record.get("file_parameters"):
        return "needs-adaptation", "takes a file (%s); no file selector yet (P8.3)" % ", ".join(f.rsplit("#", 1)[-1] for f in record["file_parameters"])
    if record.get("finite") is False:
        return "needs-adaptation", "produced non-finite samples (NaN or infinity) from a 220 Hz sine at 0.25: " + record.get("signal", "")
    if not record.get("signal_ok"):
        return "not-tested", "loads and matches, but the generic signal check heard nothing: " + record.get("signal", "")
    if record.get("restore_ok") is False:
        return "needs-adaptation", record["restore"]
    return "supported-pc", "loads, matches its metadata, passes signal" + ("" if record.get("restore_ok") is None else ", restores a changed control")


def without_crashes(source, target):
    """The catalogue for the live check: every entry except known crashers."""
    document = json.load(open(source))
    document["modules"] = [m for m in document["modules"] if m["uri"] not in CRASHES]
    with open(target, "w") as f:
        json.dump(document, f, indent=1)


def main():
    sys.stdout.reconfigure(line_buffering=True)
    if sys.argv[1] == "--without-crashes":
        return without_crashes(sys.argv[2], sys.argv[3])
    catalogue_path, live_path, work = sys.argv[1:4]
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    data = json.load(open(DATA))
    data_sources = {p["uri"]: sorted({s["repo"] for s in p["sources"]}) for p in data["poly_plugins"]}
    beebo = {}
    for m in data["beebo_mappings"]:
        beebo.setdefault(m["uri"], []).append(m["key"])
    catalogue = {m["uri"]: m for m in json.load(open(catalogue_path))["modules"]}
    live = live_results(live_path)
    world = lilv.World()
    world.load_all()
    installed = {str(p.get_uri()): p for p in each(world.get_all_plugins())}
    engine_features = set()
    with open(os.path.join(ROOT, "tools", "catalogue-export", "main.cpp")) as f:
        text = f.read()
        block = text[text.index("engine_features[]"): text.index("};", text.index("engine_features[]"))]
        engine_features = set(re.findall(r'"([^"]+)"', block))

    rig = Rig()
    c = Client()
    for name, kind in (("ain", "lv2:InputPort , lv2:AudioPort"), ("aout", "lv2:OutputPort , lv2:AudioPort"),
                       ("cin", "lv2:InputPort , lv2:CVPort"), ("cout", "lv2:OutputPort , lv2:CVPort")):
        c.put("ingen:/main/" + name, "a " + kind)
    time.sleep(0.5)
    rig.client.connect("compat-probe:src", "ingen:ain")
    rig.client.connect("compat-probe:src", "ingen:cin")

    records = []
    for uri in inventory():
        record = {"uri": uri, "beebo": beebo.get(uri, []), "pi": "not tested"}
        plugin = installed.get(uri)
        if plugin is None:
            record["installed"] = False
            record["source"] = "Poly: " + ", ".join(data_sources[uri]) if uri in data_sources else "no public source in the Poly collection"
            known = next((why for prefix, why in MISSING.items() if uri.startswith(prefix)), None)
            record["why_missing"] = (known or ("archived Digit source (digit_carla), not built" if "digit_carla" in data_sources.get(uri, [])
                                               else "not built here: see 14-more-plugins.sh" if uri in data_sources
                                               else "no public implementation known; Beebo maps it without a source" if uri.startswith("http://polyeffects.com")
                                               else "not installed: no package or source build provides it here"))
        else:
            record["installed"] = True
            record.update(metadata(world, plugin))
            record["source"] = origin_of(uri, record["bundle"], data_sources)
            library = plugin.get_library_uri()
            record.update(binary_facts(urllib.parse.unquote(urllib.parse.urlparse(str(library)).path) if library is not None else None))
            unsupported = [f for f in record["required_features"] if f not in engine_features]
            if unsupported:
                record["unsupported_features"] = unsupported
        if uri in catalogue:
            entry = catalogue[uri]
            record["export"] = {"availability": entry["availability"], "reason": entry["reason"]}
            if uri in live:
                record["live"] = list(live[uri])
            if entry["availability"] == "available" and live.get(uri, ("",))[0] == "PASS":
                record.update(exercise(c, rig, entry, work))
        record["status"], record["detail"] = classify(record)
        print("%-17s %s -- %s" % (record["status"], uri, record["detail"]))
        records.append(record)
        if subprocess.run(["pgrep", "-x", "ingen"], capture_output=True).returncode != 0:
            print("the engine went away after %s; stopping" % uri)
            break
    c.close()
    rig.client.deactivate()
    rig.client.close()

    counts = {}
    for r in records:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    meta = {"generated_by": "tools/ingen-spike/15-compat.py", "date": time.strftime("%Y-%m-%d"), "machine": os.uname().machine,
            "lv2_path": os.environ.get("LV2_PATH", ""), "counts": counts,
            "statuses": {"supported-pc": "every host check passes on the PC engine; becomes supported after the Pi checks",
                         "needs-adaptation": "installed, but something must change before it can be offered as working",
                         "unavailable": "not installed here: no source, or no build yet",
                         "not-tested": "installed and loadable, but the generic checks could not judge it"}}
    with open(OUT_JSON, "w") as f:
        json.dump({"meta": meta, "entries": records}, f, indent=1)
        f.write("\n")
    with open(OUT_MD, "w") as f:
        f.write("# Compatibility matrix\n\nGenerated by `tools/ingen-spike/15-compat.sh` on %s (%s). One row per inventory entry; "
                "the full record is [compatibility.json](compatibility.json). Pi: not tested for any entry.\n\n" % (meta["date"], meta["machine"]))
        f.write("| Status | Count |\n| --- | --- |\n")
        for status in ("supported-pc", "needs-adaptation", "not-tested", "unavailable"):
            f.write("| %s | %d |\n" % (status, counts.get(status, 0)))
        f.write("\n| Plugin | Beebo | Source | Status | Detail |\n| --- | --- | --- | --- | --- |\n")
        for r in records:
            f.write("| %s<br>`%s` | %s | %s | %s | %s |\n" % (r.get("name", ""), r["uri"], ", ".join(r["beebo"]), r["source"], r["status"],
                                                              r["detail"].replace("|", "/")))
    print("==== %s" % ", ".join("%s %d" % kv for kv in sorted(counts.items())))


if __name__ == "__main__":
    main()
