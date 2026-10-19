# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the Patchstorage/Peepo comparison from recorded catalogue evidence.

Run: python tools/compare-patchstorage.py
This reads snapshots only; it does not install plugins or contact the Pi.
"""
from collections import Counter
import importlib.util
import json
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT = ROOT / 'docs/evidence/patchstorage-lv2-20261019.json'
REPORT = ROOT / 'docs/34-patchstorage-peepo-comparison.md'
PI_EXPORT = ROOT / 'docs/evidence/catalogue-pi-20261019.json'
if not PI_EXPORT.exists():
    PI_EXPORT = ROOT / 'docs/evidence/catalogue-pi-20261018.json'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def cell(value):
    return str(value).replace('|', '\\|').replace('\n', ' ').replace('\r', ' ')


def row(*values):
    return '| ' + ' | '.join(cell(v) for v in values) + ' |'


def main():
    spec = importlib.util.spec_from_file_location('document_catalogue', ROOT / 'tools/document-catalogue.py')
    presentation = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(presentation)
    snapshot = read(SNAPSHOT)
    reference = read(ROOT / 'docs/lv2-plugin-comparison-data.json')
    rules = read(ROOT / 'config/catalogue/overlay.json')['rules']
    installed = {m['uri']: presentation.present(m, rules) for m in
                 read(PI_EXPORT)['modules']}
    pi_date = datetime.strptime(PI_EXPORT.stem.rsplit('-', 1)[-1], '%Y%m%d').strftime('%d %B %Y').lstrip('0')
    selected = {line.split('  #', 1)[0].strip() for line in
                (ROOT / 'config/catalogue/inventory.txt').read_text(encoding='utf-8').splitlines()
                if line.strip() and not line.startswith('#')}
    previous = {e['id']: e for e in reference['patchstorage_entries']}
    inspections = {e['id']: e for e in reference['patchstorage_download_inspection']}
    relationships = reference['counterpart_review']['relationships']
    entries = sorted(snapshot['entries'], key=lambda e: (e['title'].casefold(), e['id']))
    assert len(entries) == len({e['id'] for e in entries}) == 386
    groups = {'All ready': [], 'Partial bundle': [], 'Installed, restricted': [], 'No exact plugin installed': []}
    identities = {}
    all_uris = set()
    advertised = set()
    for entry in entries:
        names = {uri: uri.rsplit('#', 1)[-1].rsplit('/', 1)[-1] for uri in entry['uids']}
        historical = inspections.get(entry['id'])
        # Reuse archive inventory only when the publication metadata is unchanged.
        stable = entry['id'] in previous and all(entry.get(k) == previous[entry['id']].get(k)
                                                for k in ('revision', 'updated_at', 'uids', 'targets'))
        if historical and stable:
            names.update({p['uri']: p['name'] for p in historical['plugins']})
        uris = set(names)
        assert uris, f"Entry has no identities: {entry['title']}"
        identities[entry['id']] = (names, historical if stable else None)
        advertised.update(entry['uids'])
        all_uris.update(uris)
        present = uris & installed.keys()
        if not present:
            group = 'No exact plugin installed'
        elif present != uris:
            group = 'Partial bundle'
        elif all(presentation.state(installed[u]) == 'Ready' for u in uris):
            group = 'All ready'
        else:
            group = 'Installed, restricted'
        groups[group].append(entry)
    counts = Counter(presentation.state(installed[u]) if u in installed else 'Not installed' for u in all_uris)
    assert sum(counts.values()) == len(all_uris)
    assert sum(map(len, groups.values())) == len(entries)
    lines = ['# Patchstorage LV2 plugins compared with Peepo', '',
             f'**Patchstorage checked 19 October 2026. Peepo installation evidence: {pi_date}.**', '',
             'Every current Patchstorage LV2 entry is listed below. Peepo status uses the recorded Pi export plus its current presentation overlay. This is a comparison with the saved instrument evidence, not a fresh query of the running Pi.', '',
             'Blokas documents Patchstorage as MODEP’s plugin cloud since MODEP 1.12. These are plugins available to install through MODEP, not a claim that every plugin is preinstalled or tested in Peepo. [Blokas MODEP](https://blokas.io/modep/); [Patchstorage platform](https://patchstorage.com/platform/lv2-plugins/).', '',
             '## Summary', '', '| Patchstorage entries | Count | Meaning |', '| --- | ---: | --- |',
             row('All ready', len(groups['All ready']), 'All inventoried identities are installed and offered in Peepo Add.'),
             row('Partial bundle', len(groups['Partial bundle']), 'Some identities are installed; others are missing.'),
             row('Installed, restricted', len(groups['Installed, restricted']), 'All identities are installed, but at least one is disabled or unavailable.'),
             row('No exact plugin installed', len(groups['No exact plugin installed']), 'No inventoried identity appears in the recorded Peepo export.'),
             row('Total', len(entries), 'One row per Patchstorage publication, including bundle publications.'), '',
             f"The publications advertise **{len(advertised)} distinct LV2 identities**. Including additional identities found in the earlier archive inspection gives **{len(all_uris)} inventoried identities**: **{counts['Ready']} ready**, **{counts['Needs adaptation']} needing adaptation**, **{counts['Unavailable']} unavailable in the host**, and **{counts['Not installed']} not installed**. Identities are deduplicated for these totals; a bundle can contain many identities.", '',
             '**Why are Gx effects missing when Peepo has all 43 GxPlugins?** The installed collection is [GxPlugins.lv2](https://github.com/brummer10/GxPlugins.lv2), upstream’s extra plugin set. The [main Guitarix project](https://github.com/brummer10/guitarix) also ships LV2 effects. Patchstorage includes both sets, so installing the whole extra collection does not install the whole Guitarix library. [Installed collection and native results](32-gxplugins.md).', '',
             '## How to read the list', '',
             '- **Ready** means selectable in Peepo, not a listening test or identical version of the Patchstorage build.',
             '- **Pi64** means the publication advertises an `rpi-aarch64` download. It does not establish Ingen compatibility. Other advertised builds are shown too.',
             '- **Ready/WIP** in the publication column is Patchstorage’s state, independent of Peepo’s status.',
             '- An exact match uses the LV2 URI, not a similar name. Different builds with the same URI can differ in version or features.',
             '- Related AMS/CV implementations are explicitly noted, with the earlier source review retained below. Other missing plugins have not received a functional-equivalence review.',
             '- “Not installed” means absent from Peepo’s exported catalogue. It does not audit unselected bundles elsewhere on the Pi filesystem.', '',
             'Build key: **Pi64** = `rpi-aarch64`; **PB32** = `patchbox-os-arm32`; **x64** = `linux-amd64`.', '']

    def counterparts(uris):
        matches = [r for r in relationships if r['patchstorage_uri'] in uris and r['poly_uri'] in installed]
        return '; '.join(sorted({installed[r['poly_uri']]['name'] + ' (' +
                                r['kind'].replace('_', ' ') + '; ' + presentation.state(installed[r['poly_uri']]) + ')'
                                for r in matches}))

    build_names = {'rpi-aarch64': 'Pi64', 'patchbox-os-arm32': 'PB32', 'linux-amd64': 'x64'}
    for group, members in groups.items():
        lines += [f'## {group} ({len(members)})', '',
                  '| Patchstorage plugin / bundle | Type | Advertised builds | Publication | Peepo module / detail |',
                  '| --- | --- | --- | --- | --- |']
        for entry in members:
            names, _ = identities[entry['id']]
            uris = set(names)
            present = uris & installed.keys()
            if present:
                if len(present) > 4:
                    states = Counter(presentation.state(installed[u]) for u in present)
                    details = '; '.join(f'{n} {state.lower()}' for state, n in sorted(states.items()))
                    details += ' (see identity details below)'
                else:
                    details = '; '.join(sorted(installed[u]['name'] + ' — ' + presentation.state(installed[u]) for u in present))
                missing = len(uris - present)
                if missing:
                    noun = 'identity' if missing == 1 else 'identities'
                    details += f'; {missing} {noun} not installed (see bundle detail below)'
            else:
                details = 'Not installed'
                if uris & selected:
                    details += '; includes selected reference identities without an installed implementation'
                related = counterparts(uris)
                if related:
                    details += '; related Peepo modules: ' + related
            state = entry.get('state', {}).get('name', 'Unspecified').replace('Ready to Go', 'Ready').replace('Work in Progress', 'WIP')
            builds = ', '.join(build_names.get(t['slug'], t['slug']) for t in entry.get('targets', [])) or 'None advertised'
            lines.append(row(f"[{entry['title']}]({entry['url']})", ', '.join(c['name'] for c in entry.get('categories', [])), builds, state, details))
        lines.append('')
    lines += ['## Bundle and restriction details', '',
              'Every identity in a multi-plugin entry is listed here, as well as single-plugin entries installed with restrictions. **Extra** marks an identity found in the inspected archive but absent from the publication’s advertised URI list. Extra identities are confirmed only for the inspected target and date; see the evidence section.', '']
    detailed = []
    for entry in entries:
        names, inspection = identities[entry['id']]
        if len(names) == 1 and not any(u in installed and presentation.state(installed[u]) != 'Ready' for u in names):
            continue
        detailed.append(entry['id'])
        lines += [f"### {entry['title']}", '', f"[Publication]({entry['url']})" +
                  (f"; archive inventory: {inspection['target']}, 13 October 2026." if inspection else '; advertised identities only.'), '',
                  '| Plugin / Peepo name | Peepo status | Note | LV2 identity |', '| --- | --- | --- | --- |']
        for uri, name in sorted(names.items(), key=lambda p: (p[1].casefold(), p[0])):
            status = 'Not installed'
            note = 'Extra; ' if uri not in entry['uids'] else ''
            if uri in installed:
                module = installed[uri]
                name = module['name']
                status = presentation.state(module)
                if status != 'Ready':
                    note += module.get('reason', '')
            lines.append(row(name, status, note.rstrip('; '), f'`{uri}`'))
        lines.append('')
    lines += ['## Related implementations already in Peepo', '',
              'These relationships come from the source review of 13 October. They do not make the Patchstorage identities installed, and they are not guaranteed drop-in replacements. [Full source comparison](../LV2%20Plugin%20Comparison.md#corrected-comparison-ams-and-cv-counterparts).', '',
              '| Patchstorage identity | Peepo module | Relationship | Difference / evidence |', '| --- | --- | --- | --- |']
    for relationship in relationships:
        uri = relationship['poly_uri']
        if uri not in installed:
            continue
        lines.append(row(f"`{relationship['patchstorage_uri']}`", installed[uri]['name'] + ' — ' + presentation.state(installed[uri]),
                         relationship['kind'].replace('_', ' '), relationship['notes'] +
                         f" [Poly source]({relationship['poly_source']}); [MOD source]({relationship['patchstorage_source']})."))
    lines += ['', '## Evidence and limitations', '',
              '- Current publication metadata: [Patchstorage snapshot](evidence/patchstorage-lv2-20261019.json), fetched through the public API in four pages of up to 100 entries. [API page 1](https://patchstorage.com/api/beta/patches/?platforms=8046&per_page=100&page=1).',
              f'- Installation evidence: [Pi catalogue, {pi_date}](evidence/{PI_EXPORT.name}), [overlay](../config/catalogue/overlay.json), [selected inventory](../config/catalogue/inventory.txt). [Complete Peepo module list](33-module-list.md) supplies descriptions and connections.',
              '- Bundle expansion: [13 October download inspection](lv2-plugin-comparison-data.json). One archive per entry was inspected then; this report reuses it only for entries whose revision, update timestamp, advertised identities and targets remain unchanged. It does not claim an inspection of every architecture or a new download inspection today.',
              '- The new **16 Pads** entry and updated **JTBassMIDI** entry use current advertised identities only. They have no fresh archive inspection in this comparison.',
              '- None of the missing entries has been built or tested in Peepo for this comparison. An advertised Pi64 binary can still need host features, file handling, MIDI routing or other adaptation.',
              '- Generated by `python tools/compare-patchstorage.py`. The historical Poly/Beebo comparison and its input data remain unchanged.', '']
    REPORT.write_text('\n'.join(lines), encoding='utf-8', newline='\n')
    print(json.dumps({'entries': {k: len(v) for k, v in groups.items()}, 'identities': dict(counts),
                      'advertised_identities': len(advertised), 'expanded_identities': len(all_uris),
                      'bundle_or_restriction_sections': len(detailed)}, indent=2))


if __name__ == '__main__':
    main()
