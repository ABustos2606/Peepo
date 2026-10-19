# SPDX-License-Identifier: GPL-3.0-or-later
"""Activate a verified expansion and engine, preserving the current graph.

Run as root on the Pi: install-patchstorage.py WORK STAGE ENGINE RESULTS
WORK contains config/catalogue/{inventory.txt,overlay.json,patchstorage.json}.
ENGINE is the staged /opt/peepo prefix. JACK is left running. The UI and engine
restart once; an independent graph snapshot and rollback files are retained.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ingen-spike'))
from ingenprobe import Client


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def files(path):
    return {str(p.relative_to(path)): sha(p) for p in path.rglob('*') if p.is_file()}


def atomic_copy(source, destination):
    temporary = destination.with_name(destination.name + '.peepo-update')
    shutil.copy2(source, temporary)
    os.replace(temporary, destination)


def socket_ready():
    for _ in range(100):
        if Path('/run/peepo/ingen.sock').is_socket():
            return
        time.sleep(.1)
    raise RuntimeError('The engine did not create its socket')


def copy_graph(source, destination):
    client = Client('/run/peepo/ingen.sock')
    try:
        assert client.copy(source, destination, timeout=60)[0] == 0
        assert client.get('ingen:/main', timeout=20)[0] == 0
    finally:
        client.close()


def graph_equal(before, after):
    # Public IDs make bundle-relative node addresses comparable between copies.
    import rdflib
    from rdflib.compare import isomorphic
    def graph(bundle):
        return rdflib.Graph().parse(bundle / 'main.ttl', publicID='file:///peepo-graph-compare/main.ttl')
    return isomorphic(graph(before), graph(after))


def main():
    assert os.geteuid() == 0, 'Run as root'
    work, stage, engine, results = map(lambda p: Path(p).resolve(), sys.argv[1:])
    prefix = Path('/opt/peepo')
    manifest = json.loads((work / 'config/catalogue/patchstorage.json').read_text())
    expected = {p['uri'] for e in manifest['entries'] for p in e['plugins']}
    result = json.loads(results.read_text())
    assert result['machine'] == 'aarch64' and result['total'] == result['passed'] == len(expected)
    assert {p['uri'] for p in result['entries']} == expected
    assert all(p['passed'] for p in result['entries'])
    assert result['control_recheck'] is True
    assert result['manifest_sha256'] == sha(work / 'config/catalogue/patchstorage.json')
    assert result['engine_server_sha256'] == sha(engine / 'lib/aarch64-linux-gnu/libingen_server.so')
    tree = hashlib.sha256()
    for item in sorted((stage / 'lib/lv2').rglob('*')):
        if item.is_file():
            tree.update(item.relative_to(stage / 'lib/lv2').as_posix().encode())
            tree.update(hashlib.sha256(item.read_bytes()).digest())
    assert result['plugin_tree_sha256'] == tree.hexdigest()
    # Fail before touching services if comparison support or any bundle is absent.
    import rdflib
    bundles = {p['bundle'] for e in manifest['entries'] for p in e['plugins']}
    for bundle in bundles:
        assert Path(bundle).name == bundle and bundle.endswith('.lv2')
        assert (stage / 'lib/lv2' / bundle / 'manifest.ttl').is_file()
        target = prefix / 'lib/lv2' / bundle
        assert not target.exists() or files(target) == files(stage / 'lib/lv2' / bundle), f'Existing bundle differs: {bundle}'
    engine_files = ['bin/ingen', 'lib/aarch64-linux-gnu/libingen_server.so',
                    'lib/aarch64-linux-gnu/libingen_jack.so', 'lib/aarch64-linux-gnu/libingen-0.so.0.5.1']
    assert all((engine / name).is_file() for name in engine_files)
    record = Path(tempfile.mkdtemp(prefix='patchstorage-update-', dir='/var/lib/peepo'))
    shutil.chown(record, user='peepo', group='peepo')
    backup = record / 'backup'
    backup.mkdir()
    metadata = {'config/catalogue/inventory.txt': 'share/peepo/catalogue-inventory.txt',
                'config/catalogue/overlay.json': 'share/peepo/catalogue-overlay.json',
                'config/catalogue/patchstorage.json': 'share/peepo/patchstorage-manifest.json'}
    targets = engine_files + list(metadata.values()) + ['share/peepo/catalogue.json']
    existed = []
    for name in targets:
        target = prefix / name
        if target.exists():
            destination = backup / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, destination)
            existed.append(name)
    before_units = {u: command('systemctl', 'show', '-p', 'MainPID', '--value', u) for u in ('peepo-jack', 'peepo-engine', 'peepo-ui')}
    before = record / 'before.ingen'
    stopped = False
    try:
        # All heavy copying and export happen before the audio engine restart.
        for bundle in sorted(bundles):
            destination = prefix / 'lib/lv2' / bundle
            if not destination.exists():
                temporary = destination.with_name(destination.name + '.peepo-update')
                shutil.copytree(stage / 'lib/lv2' / bundle, temporary)
                os.replace(temporary, destination)
        for source, destination in metadata.items():
            atomic_copy(work / source, prefix / destination)
        subprocess.run(['/opt/peepo/bin/peepo-export-catalogue'], check=True)
        catalogue = json.loads((prefix / 'share/peepo/catalogue.json').read_text())
        assert expected <= {p['uri'] for p in catalogue['modules']}
        subprocess.run(['systemctl', 'stop', 'peepo-ui', 'peepo-route'], check=True)
        copy_graph('ingen:/main', before.as_uri())
        subprocess.run(['systemctl', 'stop', 'peepo-engine'], check=True)
        stopped = True
        assert command('systemctl', 'show', '-p', 'MainPID', '--value', 'peepo-engine') == '0'
        for name in engine_files:
            atomic_copy(engine / name, prefix / name)
        subprocess.run(['systemctl', 'start', 'peepo-engine'], check=True)
        socket_ready()
        copy_graph(before.as_uri(), 'ingen:/main')
        after = record / 'after.ingen'
        copy_graph('ingen:/main', after.as_uri())
        assert graph_equal(before, after), 'Restored graph differs from the captured graph'
        subprocess.run(['systemctl', 'start', 'peepo-route', 'peepo-ui'], check=True)
        time.sleep(2)
        after_ui = record / 'after-ui.ingen'
        copy_graph('ingen:/main', after_ui.as_uri())
        assert graph_equal(before, after_ui), 'UI restart changed the restored graph'
        for unit in ('peepo-jack', 'peepo-engine', 'peepo-route', 'peepo-ui'):
            assert command('systemctl', 'is-active', unit) == 'active', unit
        assert command('systemctl', 'show', '-p', 'MainPID', '--value', 'peepo-jack') == before_units['peepo-jack']
        for directory in ('sources', 'licenses'):
            source = stage / 'share/peepo' / directory
            if source.exists():
                shutil.copytree(source, prefix / 'share/peepo' / directory, dirs_exist_ok=True)
        report = {'date': time.strftime('%Y-%m-%d'), 'installed_identities': len(expected),
                  'catalogue_identities': len(catalogue['modules']), 'graph_unchanged': True,
                  'jack_pid_unchanged': True, 'before_pids': before_units,
                  'after_pids': {u: command('systemctl', 'show', '-p', 'MainPID', '--value', u) for u in before_units},
                  'record': str(record), 'engine_sha256': {name: sha(prefix / name) for name in engine_files}}
        (record / 'installation.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    except Exception:
        if stopped:
            subprocess.run(['systemctl', 'stop', 'peepo-ui', 'peepo-engine'])
        for name in existed:
            atomic_copy(backup / name, prefix / name)
        if stopped:
            subprocess.run(['systemctl', 'start', 'peepo-engine'], check=True)
            socket_ready()
            if before.is_dir():
                copy_graph(before.as_uri(), 'ingen:/main')
        subprocess.run(['systemctl', 'start', 'peepo-route', 'peepo-ui'])
        print('Restored previous engine/catalogue; recovery record: ' + str(record), file=sys.stderr)
        raise


if __name__ == '__main__':
    main()
