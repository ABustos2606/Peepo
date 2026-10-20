# SPDX-License-Identifier: GPL-3.0-or-later
"""Deploy the first catalogue purge without restarting the audio engine.

Run as root on the Pi: python3 tools/pi/purge-catalogue.py WORK
WORK is the repository (or a transfer containing config/catalogue and tools).
The current graph and prior metadata/bundles are retained for recovery.
"""
from datetime import datetime, timezone
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

WORK = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(WORK / 'tools/ingen-spike'))
from ingenprobe import Client


def load_tool(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def pids():
    return {s: run('systemctl', 'show', '-p', 'MainPID', '--value', s)
            for s in ('peepo-engine', 'peepo-jack')}


def snapshot(destination):
    client = Client('/run/peepo/ingen.sock')
    try:
        assert client.copy('ingen:/main', destination.as_uri(), timeout=60)[0] == 0
    finally:
        client.close()


def equal(a, b):
    import rdflib
    from rdflib.compare import isomorphic
    def graph(path):
        return rdflib.Graph().parse(path / 'main.ttl', publicID='file:///peepo-compare/main.ttl')
    return isomorphic(graph(a), graph(b))


def main():
    assert os.geteuid() == 0, 'Run as root'
    prefix = Path('/opt/peepo')
    share = prefix / 'share/peepo'
    config = WORK / 'config/catalogue'
    retirement = json.loads((config / 'retired.json').read_text())
    retired = {m['uri'] for m in retirement['modules']}
    inventory = {s.split('  #', 1)[0].strip() for s in (config / 'inventory.txt').read_text().splitlines()
                 if s.strip() and not s.startswith('#')}
    assert not retired & inventory
    old = json.loads((share / 'catalogue.json').read_text())
    old_uris = {m['uri'] for m in old['modules']}
    assert retired <= old_uris or retired.isdisjoint(old_uris), 'Catalogue is only partially purged'
    previous_pids = pids()
    assert all(pid != '0' for pid in previous_pids.values())
    record = Path(tempfile.mkdtemp(prefix='catalogue-purge-', dir='/var/lib/peepo'))
    shutil.chown(record, user='peepo', group='peepo')
    snapshot(record / 'before.ingen')
    before_text = (record / 'before.ingen/main.ttl').read_text()
    assert not any(uri in before_text for uri in retired), 'Current patch uses a retiring effect; preserve it before purging'
    metadata = {'inventory.txt': 'catalogue-inventory.txt', 'overlay.json': 'catalogue-overlay.json',
                'patchstorage.json': 'patchstorage-manifest.json', 'retired.json': 'catalogue-retired.json'}
    backup = record / 'metadata'
    backup.mkdir()
    existed = []
    for name in [*metadata.values(), 'catalogue.json']:
        if (share / name).exists():
            shutil.copy2(share / name, backup / name)
            existed.append(name)
    prune = load_tool('prune', WORK / 'tools/prune-retired-bundles.py').prune
    bundles = None
    try:
        bundles = prune(prefix, retirement, record / 'bundles')
        for source, name in metadata.items():
            temporary = share / (name + '.purge-update')
            shutil.copy2(config / source, temporary)
            os.replace(temporary, share / name)
        run(str(prefix / 'bin/peepo-export-catalogue'))
        fresh = json.loads((share / 'catalogue.json').read_text())
        fresh_uris = {m['uri'] for m in fresh['modules']}
        assert fresh_uris == old_uris - retired, 'Unexpected change to retained plugin identities'
        documenter = load_tool('documenter', WORK / 'tools/document-catalogue.py')
        rules = json.loads((config / 'overlay.json').read_text())['rules']
        shown = [documenter.present(m, rules) for m in fresh['modules']]
        visible_ir = [m for m in shown if not m.get('hidden') and '/polyconvo#' in m['uri']]
        assert len(visible_ir) == 6 and all(m['availability'] == 'available' for m in visible_ir)
        assert all(m.get('hidden') for m in shown if '/convoLV2#' in m['uri'] or '/zeroconvolv#' in m['uri'])
        run('systemctl', 'restart', 'peepo-ui')
        time.sleep(2)
        snapshot(record / 'after.ingen')
        assert equal(record / 'before.ingen', record / 'after.ingen'), 'Current patch changed during deployment'
        assert pids() == previous_pids, 'The audio engine or JACK restarted'
        assert run('systemctl', 'is-active', 'peepo-ui') == 'active'
        report = {'timestamp': datetime.now(timezone.utc).isoformat(), 'removed_uris': sorted(retired),
                  'removed_standalone_bundles': sorted(p.name for p in bundles.iterdir()) if bundles else [],
                  'shared_mda_bundle_unchanged': True, 'installed_before': len(old_uris),
                  'installed_after': len(fresh_uris), 'ready_to_add': sum(documenter.state(m) == 'Ready' for m in shown),
                  'hidden': sum(documenter.state(m) == 'Hidden' for m in shown),
                  'ir_families': [m['name'] for m in visible_ir], 'current_patch_unchanged': True,
                  'engine_and_jack_pids': previous_pids, 'ui_active': True, 'backup': str(record)}
        (record / 'purge.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    except Exception:
        for name in [*metadata.values(), 'catalogue.json']:
            if name in existed:
                shutil.copy2(backup / name, share / name)
            elif (share / name).exists():
                (share / name).unlink()
        if bundles:
            for bundle in bundles.iterdir():
                shutil.move(str(bundle), prefix / 'lib/lv2' / bundle.name)
        subprocess.run(['systemctl', 'restart', 'peepo-ui'])
        print('Previous catalogue and bundles restored; recovery: ' + str(record), file=sys.stderr)
        raise


if __name__ == '__main__':
    main()
