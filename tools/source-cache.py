#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Keep every source Peepo builds, so a build works without GitHub or GitLab.

Run as root under Debian (WSL):
  python3 tools/source-cache.py save [--cache DIR]   mirror, bundle and archive every pin
  python3 tools/source-cache.py verify [--cache DIR] check the cache's checksums and bundles
  python3 tools/source-cache.py offline ROOT          serve git clones in ROOT from the cache
  python3 tools/source-cache.py online ROOT           undo offline

Pins are read from the build scripts (01-build.sh, 02-plugins.sh, 12-nam.sh,
14-more-plugins.sh) and config/catalogue/gxplugins.json, so the cache follows
every pin change. Submodules are found at each pinned commit and cached too.

The cache (default /mnt/c/peepo-sources, outside WSL) holds:
  git/<host>/<path>.bundle     whole repository, every branch and tag
  tarballs/<name>-<commit>.tar.gz  the pinned tree with its submodules (GPL handover)
  patchstorage/                copy of the Patchstorage download cache
  manifest.json, SHA256SUMS

`offline ROOT` restores the bundles into ROOT/var/cache/peepo-git and adds a
marked block to ROOT/etc/gitconfig that rewrites https:// clones to it, so the
unchanged build scripts (and submodule updates) read the cache. ROOT is / for
the WSL build or /opt/peepo-arm64 for the Pi's build root.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parent.parent
SPIKE = REPO / 'tools/ingen-spike'
MIRRORS = Path('/var/cache/peepo-git')  # working mirrors on the WSL side
BEGIN, END = '# peepo source cache: begin', '# peepo source cache: end'
os.environ['GIT_TERMINAL_PROMPT'] = '0'  # a private or missing repo fails at once


def git(*args, cwd=None, check=True, capture=True):
    result = subprocess.run(['git', *args], cwd=cwd, text=True,
                            stdout=subprocess.PIPE if capture else None, stderr=subprocess.PIPE)
    if check and result.returncode:
        raise RuntimeError(f"git {' '.join(args)}: {result.stderr.strip()}")
    return result.stdout.strip() if capture else ''


def key(url):
    """https://github.com/owner/repo.git -> github.com/owner/repo"""
    url = re.sub(r'^[a-z+]+://', '', url.strip()).rstrip('/')
    return url[:-4] if url.endswith('.git') else url


def pins():
    """(name, url, ref) for every source the build scripts clone."""
    found = []
    more = (SPIKE / '14-more-plugins.sh').read_text()
    poly_base = re.search(r'fetch "(https://\S+)/\$1\.git"', more).group(1)
    found += [(n, f'{poly_base}/{n}.git', c) for n, c in re.findall(r'^poly (\S+) ([0-9a-f]{40})', more, re.M)]
    found += [(n, u, c) for u, n, c in re.findall(r'^fetch (https://\S+) (\S+) ([0-9a-f]{40})', more, re.M)]
    found += re.findall(r'^third (\S+) (https://\S+) ([0-9a-f]{40})', more, re.M)

    def assigned(text, name):
        return re.search(rf'^{name}=(\S+)', text, re.M).group(1)

    def cloned(text, repo):
        return re.search(rf'(https://\S+/{repo}\.git)', text).group(1)

    build = (SPIKE / '01-build.sh').read_text()
    found += [('ingen', cloned(build, 'ingen'), assigned(build, 'INGEN_BASE')),
              ('ingen', cloned(build, 'ingen'), assigned(build, 'INGEN_PICK'))]
    found += [(n, cloned(build, n), assigned(build, f'{n.upper()}_TAG')) for n in ('raul', 'zix', 'lilv')]
    plugins = (SPIKE / '02-plugins.sh').read_text()
    found.append(('lenticular_lv2', cloned(plugins, 'lenticular_lv2'), assigned(plugins, 'LENTICULAR_COMMIT')))
    nam = (SPIKE / '12-nam.sh').read_text()
    found += [(d, u, assigned(nam, v)) for u, d, v in re.findall(r'^fetch (https://\S+) (\S+) "\$(\w+)"', nam, re.M)]
    gx = json.loads((REPO / 'config/catalogue/gxplugins.json').read_text())['plugins']
    found += [(p['repo'], p['url'], p['commit']) for p in gx]
    return found


def mirror(url):
    """Clone or update the working mirror; returns its path. Update failures
    are tolerated when a mirror already exists (the caller checks the commit)."""
    path = MIRRORS / (key(url) + '.git')
    if path.exists():
        # No pruning: refs/peepo/rescued/* keep commits upstream has dropped
        # (rescued from local checkouts), and history deleted upstream stays.
        result = subprocess.run(['git', '-C', str(path), 'remote', 'update'],
                                capture_output=True, text=True)
        if result.returncode:
            print(f'  warning: could not update {key(url)}; using the cached copy')
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        git('clone', '-q', '--mirror', url, str(path))
    return path


def submodules(path, commit, parent_url):
    """(path, url, commit) of each submodule recorded at commit."""
    if subprocess.run(['git', '-C', str(path), 'cat-file', '-e', f'{commit}:.gitmodules'],
                      capture_output=True).returncode:
        return []
    config = git('-C', str(path), 'config', '--blob', f'{commit}:.gitmodules', '--get-regexp',
                 r'^submodule\..*\.(path|url)$', check=False)
    names = {}
    for line in config.splitlines():
        name_key, value = line.split(' ', 1)
        name, field = name_key[len('submodule.'):].rsplit('.', 1)
        names.setdefault(name, {})[field] = value
    result = []
    for entry in names.values():
        if 'path' not in entry or 'url' not in entry:
            continue
        tree = git('-C', str(path), 'ls-tree', commit, entry['path'], check=False).split()
        if len(tree) < 3 or tree[1] != 'commit':
            continue  # declared but not recorded at this commit
        url = re.sub(r'^git@([^:]+):', r'https://\1/', entry['url'])
        if url.startswith('../') or url.startswith('./'):
            base = parent_url.rstrip('/')
            base = base[:-4] if base.endswith('.git') else base
            for part in url.split('/'):
                if part == '..':
                    base = base.rsplit('/', 1)[0]
                elif part not in ('.', ''):
                    base += '/' + part
            url = base
        result.append((entry['path'], url, tree[2]))
    return result


def collect(url, ref, seen, failures, depth=0):
    """Mirror url, resolve ref, then recurse into its submodules. Returns a tree record."""
    try:
        path = mirror(url)
    except RuntimeError as error:
        failures.append(f'{key(url)}: {error}')
        return None
    commit = git('-C', str(path), 'rev-parse', '-q', '--verify', f'{ref}^{{commit}}', check=False)
    if not commit:
        failures.append(f'{key(url)}: {ref} is not in the repository')
        return None
    record = dict(url=url, key=key(url), ref=ref, commit=commit, submodules=[])
    seen.add(key(url))
    for sub_path, sub_url, sub_commit in submodules(path, commit, url):
        child = collect(sub_url, sub_commit, seen, failures, depth + 1)
        if child:
            child['path'] = sub_path
            record['submodules'].append(child)
    return record


def extract(record, destination):
    path = MIRRORS / (record['key'] + '.git')
    destination.mkdir(parents=True, exist_ok=True)
    archive = subprocess.Popen(['git', '-C', str(path), 'archive', '--format=tar', record['commit']],
                               stdout=subprocess.PIPE)
    subprocess.run(['tar', '-x', '-C', str(destination)], stdin=archive.stdout, check=True)
    if archive.wait():
        raise RuntimeError(f"git archive failed for {record['key']}")
    for child in record['submodules']:
        extract(child, destination / child['path'])


def tarball(name, record, out):
    target = out / f"{name}-{record['commit']}.tar.gz"
    if target.exists():
        return target
    stamp = git('-C', str(MIRRORS / (record['key'] + '.git')), 'log', '-1', '--format=%ct', record['commit'])
    with tempfile.TemporaryDirectory() as temporary:
        extract(record, Path(temporary) / name)
        partial = target.with_suffix('.partial')
        with open(partial, 'wb') as output:
            tar = subprocess.Popen(['tar', '--sort=name', f'--mtime=@{stamp}', '--owner=0', '--group=0',
                                    '--numeric-owner', '-cf', '-', '-C', temporary, name], stdout=subprocess.PIPE)
            subprocess.run(['gzip', '-n', '-6'], stdin=tar.stdout, stdout=output, check=True)
            if tar.wait():
                raise RuntimeError(f'tar failed for {name}')
        partial.rename(target)
    return target


def sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def save(cache):
    sources, seen, failures = [], set(), []
    for name, url, ref in pins():
        print(f'{name} {ref[:12]}')
        record = collect(url, ref, seen, failures)
        if record:
            record['name'] = name
            sources.append(record)
    (cache / 'git').mkdir(parents=True, exist_ok=True)
    for repo in sorted(seen):
        bundle = cache / 'git' / (repo + '.bundle')
        bundle.parent.mkdir(parents=True, exist_ok=True)
        heads = git('-C', str(MIRRORS / (repo + '.git')), 'show-ref')
        stamp = hashlib.sha256(heads.encode()).hexdigest()
        marker = bundle.with_suffix('.refs')
        if bundle.exists() and marker.exists() and marker.read_text() == stamp:
            continue
        print(f'bundle {repo}')
        partial = bundle.with_suffix('.partial')
        git('-C', str(MIRRORS / (repo + '.git')), 'bundle', 'create', '-q', str(partial), '--all')
        partial.rename(bundle)
        marker.write_text(stamp)
    tarballs = cache / 'tarballs'
    tarballs.mkdir(exist_ok=True)
    for record in sources:
        record['tarball'] = tarball(record['name'], record, tarballs).name
    patchstorage = REPO / 'build/scratch/expanded-lv2-cache'
    def refresh(source, target):
        # Skip unchanged files; replace changed ones even when read-only (git packs).
        if os.path.exists(target):
            a, b = os.stat(source), os.stat(target)
            if a.st_size == b.st_size and int(a.st_mtime) == int(b.st_mtime):
                return target
            os.chmod(target, 0o644); os.unlink(target)
        return shutil.copy2(source, target)
    if patchstorage.is_dir():
        shutil.copytree(patchstorage, cache / 'patchstorage', dirs_exist_ok=True, copy_function=refresh)
    else:
        failures.append(f'{patchstorage} is missing: Patchstorage downloads not cached')
    manifest = dict(format=1, repositories=sorted(seen), sources=sources, failures=failures)
    (cache / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    files = sorted(p for p in cache.rglob('*') if p.is_file() and p.suffix not in ('.refs', '.partial')
                   and p.name != 'SHA256SUMS')
    with open(cache / 'SHA256SUMS', 'w', newline='\n') as sums:
        for path in files:
            sums.write(f'{sha256(path)}  {path.relative_to(cache).as_posix()}\n')
    size = sum(p.stat().st_size for p in files)
    print(f'{len(sources)} pins, {len(seen)} repositories, {len(files)} files, {size / 2**30:.2f} GiB in {cache}')
    for failure in failures:
        print(f'FAILED {failure}')
    return 1 if failures else 0


def verify(cache):
    bad = 0
    for line in (cache / 'SHA256SUMS').read_text().splitlines():
        digest, name = line.split('  ', 1)
        if sha256(cache / name) != digest:
            print(f'checksum mismatch: {name}'); bad += 1
    manifest = json.loads((cache / 'manifest.json').read_text())
    with tempfile.TemporaryDirectory() as scratch:  # verify needs a repository to run in
        git('init', '-q', scratch)
        for repo in manifest['repositories']:
            result = subprocess.run(['git', 'bundle', 'verify', '-q', str(cache / 'git' / (repo + '.bundle'))],
                                    cwd=scratch, capture_output=True, text=True)
            if result.returncode:
                print(f'bad bundle: {repo}: {result.stderr.strip()}'); bad += 1
    print('PASS' if not bad else f'FAILED {bad} problems', f"({len(manifest['repositories'])} bundles)")
    return 1 if bad else 0


def offline(cache, root):
    store = root / 'var/cache/peepo-git'
    manifest = json.loads((cache / 'manifest.json').read_text())
    for repo in manifest['repositories']:
        target = store / (repo + '.git')
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            git('clone', '-q', '--mirror', str(cache / 'git' / (repo + '.bundle')), str(target))
        alias = store / repo
        if not alias.exists():
            alias.symlink_to(target.name)
    online(root, quiet=True)
    config = root / 'etc/gitconfig'
    block = (f'{BEGIN}\n[url "/var/cache/peepo-git/"]\n\tinsteadOf = https://\n'
             '[url "/var/cache/peepo-git/github.com/"]\n\tinsteadOf = git@github.com:\n'
             '[url "/var/cache/peepo-git/gitlab.com/"]\n\tinsteadOf = git@gitlab.com:\n'
             f'[protocol "file"]\n\tallow = always\n{END}\n')
    with open(config, 'a') as output:
        output.write(block)
    print(f"offline: {len(manifest['repositories'])} repositories served from {store}")
    return 0


def online(root, quiet=False):
    config = root / 'etc/gitconfig'
    if config.exists():
        text = config.read_text()
        text = re.sub(rf'{re.escape(BEGIN)}.*?{re.escape(END)}\n', '', text, flags=re.S)
        config.write_text(text)
    if not quiet:
        print(f'online: {config} no longer redirects clones')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=('save', 'verify', 'offline', 'online'))
    parser.add_argument('root', nargs='?', type=Path, default=Path('/'))
    parser.add_argument('--cache', type=Path, default=Path(os.environ.get('PEEPO_SOURCE_CACHE', '/mnt/c/peepo-sources')))
    args = parser.parse_args()
    if args.command == 'save':
        return save(args.cache)
    if args.command == 'verify':
        return verify(args.cache)
    if args.command == 'offline':
        return offline(args.cache, args.root)
    return online(args.root)


if __name__ == '__main__':
    sys.exit(main())
