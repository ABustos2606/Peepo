# SPDX-License-Identifier: GPL-3.0-or-later
"""Refresh only the selected Patchstorage expansion's presentation rules."""
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'config/catalogue/patchstorage.json').read_text(encoding='utf-8'))
path=ROOT/'config/catalogue/overlay.json'
text=path.read_text(encoding='utf-8')
plugins=[p for e in manifest['entries'] for p in e['plugins']]
uris={p['uri'] for p in plugins}
assert len(uris)==len(plugins)
start=re.search(r'"rules"\s*:\s*\[',text).end()
position=start
decoder=json.JSONDecoder()
kept=[]
while True:
 while text[position].isspace() or text[position]==',':position+=1
 if text[position]==']':break
 rule,end=decoder.raw_decode(text,position)
 if rule.get('uri') not in uris:kept.append(text[position:end])
 position=end
for plugin in plugins:
 rule={k:plugin[k] for k in ('uri','name','category','description','parameters','tempo','bypass') if k in plugin}
 kept.append(json.dumps(rule,ensure_ascii=False))
path.write_text(text[:start]+'\n'+',\n'.join('    '+raw for raw in kept)+'\n  '+text[position:],encoding='utf-8')
print(f'Updated presentation for {len(plugins)} expansion identities')
