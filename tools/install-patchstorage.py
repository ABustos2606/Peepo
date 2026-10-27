# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage the explicitly selected, checksum-pinned Patchstorage LV2 plugins.

Run on Linux: python3 tools/install-patchstorage.py --prefix /path/to/stage
This never changes services, the live graph, or catalogue selection. Existing
bundles must have identical contents; differing bundles are never overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request


def require(condition, message):
    # Checked whatever Python's optimisation: an assert would vanish under -O.
    if not condition:
        raise RuntimeError(message)

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(tar, destination):
    """Data-only extraction, including Debian 12's original Python 3.11.2."""
    destination = Path(destination).resolve()
    members = tar.getmembers()
    for member in members:
        target = (destination / member.name).resolve()
        if not target.is_relative_to(destination) or not (member.isfile() or member.isdir()):
            raise RuntimeError(f'Unsafe archive member: {member.name}')
        member.mode &= 0o777
    if hasattr(tarfile, 'data_filter'):
        tar.extractall(destination, members=members, filter='data')
    else:
        tar.extractall(destination, members=members)


def install_bundle(source, destination):
    if destination.exists():
        source_files = {p.relative_to(source): digest(p) for p in source.rglob('*') if p.is_file()}
        dest_files = {p.relative_to(destination): digest(p) for p in destination.rglob('*') if p.is_file()}
        if source_files != dest_files:
            raise RuntimeError(f'Refusing to overwrite differing bundle: {destination}')
        return
    shutil.copytree(source, destination)


def cached_archive(cache, target, artifact):
    archive = cache / target / artifact['filename']
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        partial = archive.with_suffix(archive.suffix + '.partial')
        with urllib.request.urlopen(artifact['url'], timeout=90) as response, partial.open('wb') as output:
            shutil.copyfileobj(response, output)
        if digest(partial) != artifact['sha256']:
            partial.unlink()
            raise RuntimeError(f'Checksum mismatch: {artifact["filename"]}')
        partial.replace(archive)
    require(digest(archive) == artifact['sha256'], f'Checksum mismatch: {archive}')
    return archive


def rebuild_detune(recipe, cache, prefix):
    source = cache / recipe['archive_filename']
    if not source.exists():
        import io
        with tarfile.open(source, 'w:gz') as tar:
            for name, expected in recipe['source_files'].items():
                with urllib.request.urlopen(recipe['raw_base'] + name, timeout=60) as response:
                    data = response.read()
                require(hashlib.sha256(data).hexdigest() == expected, name)
                info = tarfile.TarInfo(name)
                info.size = len(data)
                info.mode = 0o644
                tar.addfile(info, io.BytesIO(data))
    with tarfile.open(source) as tar:
        files = {m.name: hashlib.sha256(tar.extractfile(m).read()).hexdigest() for m in tar if m.isfile()}
    require(files == recipe['source_files'], 'Detune source package content changed')
    output = prefix / 'build/patchstorage-detune'
    subprocess.run(['bash', str(ROOT / 'tools/build-guitarix-detune.sh'), str(cache), str(output)], check=True)
    corresponding = prefix / 'share/peepo/sources/patchstorage'
    corresponding.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, corresponding / source.name)
    shutil.copy2(ROOT / 'tools/build-guitarix-detune.sh', corresponding / 'build-guitarix-detune.sh')
    return output / 'gx_detune.so'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--cache', type=Path, default=ROOT / 'build/scratch/expanded-lv2-cache')
    parser.add_argument('--manifest', type=Path, default=ROOT / 'config/catalogue/patchstorage.json')
    parser.add_argument('--available-only', action='store_true', help='For PC checks: skip publications without a build for this architecture')
    args = parser.parse_args()
    machine = platform.machine()
    target = {'x86_64': 'linux-amd64', 'aarch64': 'rpi-aarch64'}[machine]
    expected_machine = {'x86_64': 62, 'aarch64': 183}[machine]
    args.prefix = args.prefix.resolve()
    lv2dir = args.prefix / 'lib/lv2'
    lv2dir.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    result = []
    for entry in manifest['entries']:
        package_target = entry.get('package_targets', {}).get(target)
        if target not in entry['archives'] and not package_target and args.available_only:
            print('NO BUILD ' + entry['title'], flush=True)
            continue
        artifact = (dict(manifest['packages'][package_target['package']], bundles=package_target['bundles'])
                    if package_target else entry['archives'][target])
        archive = cached_archive(args.cache, target, artifact)
        checksum = digest(archive)
        require(checksum == artifact['sha256'], f"Checksum mismatch: {archive}")
        with tempfile.TemporaryDirectory(prefix='peepo-lv2-') as work:
            if package_target:
                subprocess.run(['dpkg-deb', '--extract', str(archive), work], check=True)
                found = [Path(work) / 'usr/lib/lv2' / name for name in artifact['bundles']]
            else:
                with tarfile.open(archive) as tar:
                    extract(tar, work)
                found = sorted(Path(work).glob('*.lv2'))
            require(found and all((b / 'manifest.ttl').is_file() for b in found), entry['title'])
            bundles = [b.name for b in found]
            require(set(bundles) == set(artifact['bundles']), entry['title'])
            for override in entry.get('metadata_overrides', []):
                if override['target'] != target:
                    continue
                companion = cached_archive(args.cache, override['from_target'], entry['archives'][override['from_target']])
                with tarfile.open(companion) as tar:
                    for name in override['files']:
                        relative = override['bundle'] + '/' + name
                        (Path(work) / relative).write_bytes(tar.extractfile(relative).read())
            if entry.get('binary_rebuild'):
                rebuilt = rebuild_detune(entry['binary_rebuild'], args.cache, args.prefix)
                shutil.copy2(rebuilt, Path(work) / 'gx_detune.lv2/gx_detune.so')
            for bundle in found:
                for repair in entry.get('manifest_additions', []):
                    if repair['bundle'] == bundle.name:
                        manifest_file = bundle / 'manifest.ttl'
                        text = manifest_file.read_text()
                        if repair['uri'] not in text:
                            manifest_file.write_text(text + '\n# Peepo: expose the second descriptor already present in this binary.\n' + repair['turtle'] + '\n')
                for binary in bundle.rglob('*.so'):
                    header = binary.read_bytes()[:20]
                    require(header[:4] == b'\x7fELF' and int.from_bytes(header[18:20], 'little') == expected_machine, binary)
                install_bundle(bundle, lv2dir / bundle.name)
        result.append({'id': entry['id'], 'title': entry['title'], 'bundles': bundles, 'artifact_or_source': checksum})
        print('STAGED ' + entry['title'], flush=True)
    evidence = args.prefix / 'share/peepo'
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / 'patchstorage-install.json').write_text(json.dumps({'machine': machine, 'target': target, 'entries': result}, indent=2) + '\n')
    shutil.copy2(args.manifest, evidence / 'patchstorage-manifest.json')
    notices = args.prefix / 'share/peepo/licenses/patchstorage'
    notices.mkdir(parents=True, exist_ok=True)
    lines = ['# Selected Patchstorage plugins', '', 'Upstream authors retain copyright. Publication and upstream source links:', '']
    for entry in manifest['entries']:
        license_info = entry.get('license') or {}
        lines.append(f"- {entry['title']}: {license_info.get('name', 'See upstream license')}. {entry['url']} Source: {entry['source_url']}")
    (notices / 'NOTICE.md').write_text('\n'.join(lines) + '\n')
    print(f'STAGED {len(result)} publications for {machine}')


if __name__ == '__main__':
    main()
