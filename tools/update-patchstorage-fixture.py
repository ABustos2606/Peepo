# SPDX-License-Identifier: GPL-3.0-or-later
"""Merge an expansion export into the existing editor fixture.

This preserves other modules recorded from previous development environments.
Use the Pi export when the editor targets the instrument: architecture builds
can expose different controls. Usage: python tools/update-patchstorage-fixture.py EXPORT.json
"""
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'config/catalogue/patchstorage.json').read_text(encoding='utf-8'))
uris={p['uri'] for e in manifest['entries'] for p in e['plugins']}
retired={p['uri'] for p in json.loads((ROOT/'config/catalogue/retired.json').read_text(encoding='utf-8'))['modules']}
def parts(text):
 start=re.search(r'"modules"\s*:\s*\[',text).end();pos=start;values=[];decoder=json.JSONDecoder()
 while True:
  while text[pos].isspace() or text[pos]==',':pos+=1
  if text[pos]==']':break
  value,end=decoder.raw_decode(text,pos);values.append((value,text[pos:end]));pos=end
 return start,pos,values
fixture=ROOT/'config/catalogue/wsl-debian.json'
old=fixture.read_text(encoding='utf-8');start,end,kept=parts(old)
_,_,fresh=parts(Path(sys.argv[1]).read_text(encoding='utf-8'))
added=[item for item in fresh if item[0]['uri'] in uris]
assert len(added)==len(uris) and all(p['availability']=='available' for p,_ in added)
result=[item for item in kept if item[0]['uri'] not in uris | retired]+added
assert len(result)==len({p['uri'] for p,_ in result})
fixture.write_text(old[:start]+'\n'+',\n'.join('    '+raw for _,raw in result)+'\n  '+old[end:],encoding='utf-8')
print(f'Editor fixture: preserved {len(result)-len(uris)} existing identities; added {len(uris)} target identities')
