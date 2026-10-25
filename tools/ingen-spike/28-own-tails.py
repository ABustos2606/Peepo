# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Trails in saved patches (P13.5). Run by 28-own-tails.sh.

    python3 28-own-tails.py save BUNDLE   # engine with Discipline 0.4.0
    python3 28-own-tails.py load BUNDLE   # engine with the pinned Discipline

save: a new Discipline block, Trails untouched, saved to BUNDLE.
load: BUNDLE loaded, its Trails must still be 0; a new block must get 1, and
both values must be saved again as they are.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ingenprobe import Client

URI = "https://github.com/ABustos2606/discipline#discipline"
failed = False


def value(c, port):
    status, reply, others = c.get(port)
    m = re.search(r"ingen:value\s+\"?([-0-9.eE]+)", reply + "".join(others))
    return float(m.group(1)) if m else None


def check(what, ok, detail):
    global failed
    failed |= not ok
    print("%s  %s: %s" % ("PASS" if ok else "FAIL", what, detail), flush=True)


def saved(bundle, block):
    text = open(os.path.join(bundle, "main.ttl"), encoding="utf-8").read()
    m = re.search(r"^<%s/trails>\s+ingen:value\s+([-0-9.eE]+)" % block, text, re.M)
    return float(m.group(1)) if m else None


def create(c, path):
    status = c.put(path, "a ingen:Block ;\n\t\tlv2:prototype <%s>" % URI, timeout=20)[0]
    time.sleep(0.5)
    return status


mode, bundle = sys.argv[1], sys.argv[2]
c = Client()
c.drain()
if mode == "save":
    check("Discipline 0.4.0 added", create(c, "ingen:/main/d1") == 0, "")
    check("its Trails at the old default", value(c, "ingen:/main/d1/trails") == 0, value(c, "ingen:/main/d1/trails"))
    check("saved", c.copy("ingen:/main", "file://" + bundle, timeout=10)[0] == 0, bundle)
    check("the bundle holds Trails", saved(bundle, "d1") == 0, saved(bundle, "d1"))
else:
    check("old patch loaded", c.copy("file://" + bundle, "ingen:/main", timeout=10)[0] == 0, bundle)
    check("its Trails kept", value(c, "ingen:/main/d1/trails") == 0, value(c, "ingen:/main/d1/trails"))
    check("a new block added", create(c, "ingen:/main/d2") == 0, "")
    check("its Trails at the new default", value(c, "ingen:/main/d2/trails") == 1, value(c, "ingen:/main/d2/trails"))
    again = bundle[:-len(".ingen")] + "-again.ingen"
    check("saved again", c.copy("ingen:/main", "file://" + again, timeout=10)[0] == 0, again)
    check("both values saved as they are", (saved(again, "d1"), saved(again, "d2")) == (0, 1), (saved(again, "d1"), saved(again, "d2")))
sys.exit(1 if failed else 0)
