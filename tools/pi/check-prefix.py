# SPDX-License-Identifier: GPL-3.0-or-later
"""Refuse to package a prefix that lacks selected plugins or holds a stale engine.

Run by build.sh after pruning: check-prefix.py PREFIX
Plugins added on the Pi alone, or an engine rebuilt but not installed, would
otherwise be silently replaced by the next package install (20 October 2026).
"""
import json
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parents[2]


def main():
    prefix = Path(sys.argv[1])
    catalogue = REPO / 'config/catalogue'
    retired = {Path(m['bundle']).name for m in json.loads((catalogue / 'retired.json').read_text())['modules'] if m.get('bundle')}
    wanted = {p['bundle'] for p in json.loads((catalogue / 'gxplugins.json').read_text())['plugins']}
    wanted |= {p['bundle'] for e in json.loads((catalogue / 'patchstorage.json').read_text())['entries'] for p in e['plugins']}
    missing = sorted(b for b in wanted - retired if not (prefix / 'lib/lv2' / b / 'manifest.ttl').is_file())
    problems = ['missing bundle ' + b for b in missing]
    built = prefix / 'src/ingen/build/src/server/libingen_server.so'
    installed = prefix / 'lib' / ('aarch64-linux-gnu' if (prefix / 'lib/aarch64-linux-gnu').is_dir() else '') / 'libingen_server.so'
    # Not compared byte for byte: meson install removes the build tree's
    # RUNPATH, so a correctly installed engine always differs. A rebuild
    # after the last install is newer than the installed copy.
    if built.is_file() and (not installed.is_file() or built.stat().st_mtime > installed.stat().st_mtime):
        problems.append(f'engine built but not installed: {built} differs from {installed}')
    if problems:
        print('\n'.join(problems), file=sys.stderr)
        sys.exit(f'{len(problems)} problem(s); the prefix is not packaged')
    print(f'{len(wanted - retired)} selected Gx/Patchstorage bundles present; engine installed')


if __name__ == '__main__':
    main()
