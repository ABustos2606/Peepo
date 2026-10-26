# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Who marks the patch edited? A passive client of the instrument's engine:
it sends nothing, follows peepo:modified in what the engine announces, and
when the mark turns true and stays so for 3 s (a load sets it from the file
for a moment, then the interface clears it) writes the 300 messages before
it, and the 30 after, to the log. Follows the engine across restarts. Run as
the instrument's user:

    python3 watch-modified.py [LOG] [SOCKET]
"""
import collections
import re
import socket
import sys
import time

LOG = sys.argv[1] if len(sys.argv) > 1 else "/tmp/peepo-scenes/modified.log"
SOCKET = sys.argv[2] if len(sys.argv) > 2 else "/run/peepo/ingen.sock"
# In a Put of the graph, or a Set of the property.
IN_PUT = re.compile(r"<urn:peepo:ns#modified> (true|false)")
IN_SET = re.compile(r"patch:property <urn:peepo:ns#modified> ;\s*patch:value (true|false)")


def note(text):
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(time.strftime("%Y-%m-%d %H:%M:%S ") + text + "\n")


def watch():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET)
    s.settimeout(1.0)
    note("connected to " + SOCKET)
    recent = collections.deque(maxlen=300)
    modified, since, held, after = None, 0.0, None, 0
    buffer = b""
    while True:
        try:
            chunk = s.recv(65536)
            if not chunk:
                note("engine closed the socket")
                return
            buffer += chunk
        except socket.timeout:
            pass
        while b"\0" in buffer:
            message, buffer = buffer.split(b"\0", 1)
            body = "\n".join(l for l in message.decode(errors="replace").splitlines() if l and not l.startswith("@prefix"))
            stamped = "%.3f %s" % (time.time(), body)
            if after:
                note("AFTER " + stamped)
                after -= 1
            recent.append(stamped)
            found = IN_SET.findall(body) + IN_PUT.findall(body)
            if found:
                now = found[-1] == "true"
                if now != modified:
                    note("modified %s" % found[-1])
                    if now:
                        since, held = time.time(), list(recent)
                modified = now
        if modified and held is not None and time.time() - since > 3:
            note("MODIFIED STAYED TRUE; the %d messages up to it:" % len(held))
            for m in held:
                note("BEFORE " + m)
            held, after = None, 30


while True:
    try:
        watch()
    except OSError:
        pass
    time.sleep(2)
