# SPDX-License-Identifier: GPL-3.0-or-later
"""Signal, wet-path and saved-control checks on the isolated test engine."""
import importlib.util
import json
import math
import os
from pathlib import Path
import sys
import time

spec = importlib.util.spec_from_file_location('compat', Path(__file__).with_name('15-compat.py'))
compat = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compat)

WET = {
    'urn:dragonfly:early': {'dry_level': 0, 'early_level': 100},
    'https://github.com/michaelwillis/dragonfly-reverb': {'dry_level': 0, 'early_level': 0, 'late_level': 100},
    'urn:dragonfly:plate': {'dry_level': 0, 'early_level': 100},
    'urn:dragonfly:room': {'dry_level': 0, 'early_level': 0, 'late_level': 100},
}


def wet_probe(client, rig, entry):
    settings = WET.get(entry['uri'])
    if settings is None:
        return None
    block = 'ingen:/main/wetprobe'
    assert client.put(block, f'a ingen:Block ; lv2:prototype <{entry["uri"]}>')[0] == 0
    try:
        controls = {p['symbol']: p for p in entry['parameters']}
        for symbol, value in settings.items():
            assert symbol in controls, (entry['name'], symbol)
            p = controls[symbol]
            assert p['minimum'] <= value <= p['maximum'], (symbol, value, p)
            assert client.set(block + '/' + symbol, 'ingen:value', json.dumps(str(value)) + '^^xsd:float')[0] == 0
        if entry.get('enable'):
            assert client.set(block + '/' + entry['enable'], 'ingen:value', '"1.0"^^xsd:float')[0] == 0
        for port in entry['ports']:
            if port['type'] == 'audio' and port['direction'] == 'input':
                assert client.connect('ingen:/main/ain', block + '/' + port['symbol'])[0] == 0
        channels = []
        for port in entry['ports']:
            if port['type'] != 'audio' or port['direction'] != 'output':
                continue
            assert client.connect(block + '/' + port['symbol'], 'ingen:/main/aout')[0] == 0
            rig.client.connect('ingen:aout', 'compat-probe:cap')
            samples = rig.record(0.5, settle=1.2)
            finite = bool(compat.numpy.isfinite(samples).all())
            rms = float(compat.numpy.sqrt(compat.numpy.mean(samples.astype(compat.numpy.float64)**2))) if finite else None
            channels.append({'port': port['symbol'], 'finite': finite, 'rms': rms, 'passed': finite and rms > 0.0001})
            rig.client.disconnect('ingen:aout', 'compat-probe:cap')
            assert client.disconnect(block + '/' + port['symbol'], 'ingen:/main/aout')[0] == 0
        return {'settings': settings, 'channels': channels, 'passed': bool(channels) and all(c['passed'] for c in channels)}
    finally:
        client.delete(block)
        time.sleep(0.2)


def main():
    catalogue_path, live_path, result_path, work, inventory = sys.argv[1:]
    catalogue = json.load(open(catalogue_path))['modules']
    expected = set(Path(inventory).read_text().splitlines())
    assert {p['uri'] for p in catalogue} == expected, ('Missing identities', expected - {p['uri'] for p in catalogue})
    live = compat.live_results(live_path)
    os.makedirs(work, exist_ok=True)
    rig, client = compat.Rig(), compat.Client()
    results = []
    try:
        for name, kind in (('ain', 'lv2:InputPort , lv2:AudioPort'), ('aout', 'lv2:OutputPort , lv2:AudioPort'),
                           ('cin', 'lv2:InputPort , lv2:CVPort'), ('cout', 'lv2:OutputPort , lv2:CVPort')):
            assert client.put('ingen:/main/' + name, 'a ' + kind)[0] == 0
        time.sleep(0.5)
        for entry in catalogue:
            result = {'uri': entry['uri'], 'name': entry['name'], 'live': live.get(entry['uri'], ['MISSING', ''])}
            try:
                result.update(compat.exercise(client, rig, entry, work))
                result['wet'] = wet_probe(client, rig, entry)
                result['passed'] = (entry['availability'] == 'available' and result['live'][0] == 'PASS'
                                    and result.get('finite') is True and result.get('signal_ok') is True
                                    and result.get('restore_ok') is not False
                                    and (result['wet'] is None or result['wet']['passed']))
            except Exception as exc:
                result.update(passed=False, error=repr(exc))
                client.delete('ingen:/main/probe')
                client.delete('ingen:/main/restored')
            print(('PASS ' if result['passed'] else 'FAIL ') + entry['name'] + ': ' + result.get('signal', result.get('error', '')), flush=True)
            results.append(result)
    finally:
        client.close()
        rig.client.deactivate()
        rig.client.close()
    report = {'date': time.strftime('%Y-%m-%d'), 'machine': os.uname().machine, 'sample_rate': rig.rate,
              'block_size': 128, 'passed': sum(r['passed'] for r in results), 'total': len(results), 'entries': results}
    Path(result_path).write_text(json.dumps(report, indent=2) + '\n')
    print(f"SUMMARY {report['passed']}/{report['total']} passed")
    return 0 if report['passed'] == report['total'] else 1


if __name__ == '__main__':
    sys.exit(main())
