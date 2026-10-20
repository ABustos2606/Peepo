# SPDX-License-Identifier: GPL-3.0-or-later
"""Move retired standalone bundles out of a build prefix, retaining recovery copies.

Shared system bundles (notably mda.lv2) are never modified. Inventory selection
excludes their retired identities. Run before packaging, including skip-engine builds.
"""
import argparse
import json
from pathlib import Path
import shutil
import tempfile


def prune(prefix, manifest, backup):
    prefix = Path(prefix).resolve()
    backup = Path(backup).resolve()
    targets = []
    for module in manifest['modules']:
        name = module.get('bundle')
        if not name:
            continue
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts or relative.parts[:2] != ('lib', 'lv2') or len(relative.parts) != 3:
            raise ValueError('Invalid standalone bundle path: ' + name)
        target = prefix / relative
        if target.is_symlink() or target.resolve().parent != (prefix / 'lib/lv2').resolve():
            raise ValueError('Bundle escapes the prefix: ' + name)
        if target.exists():
            if not target.is_dir() or not (target / 'manifest.ttl').is_file():
                raise ValueError('Not an LV2 bundle: ' + name)
            targets.append(target)
    if not targets:
        return None
    if backup.is_relative_to((prefix / 'lib').resolve()):
        raise ValueError('Recovery copies must be outside plugin search paths')
    backup.mkdir(parents=True, exist_ok=True)
    record = Path(tempfile.mkdtemp(prefix='catalogue-purge-', dir=backup))
    moved = []
    try:
        for target in targets:
            shutil.move(str(target), record / target.name)
            moved.append(target)
    except Exception:
        for target in reversed(moved):
            shutil.move(str(record / target.name), target)
        raise
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--backup', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, default=Path(__file__).resolve().parents[1] / 'config/catalogue/retired.json')
    args = parser.parse_args()
    print('Recovery copies:', prune(args.prefix, json.loads(args.manifest.read_text()), args.backup))
