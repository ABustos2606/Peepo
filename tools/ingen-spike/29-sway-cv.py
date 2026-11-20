# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""What CV range Poly's CV modules respond to, and whether Sway drives them.

Sway's plan (step 4) asks this before any patch saves a Sway: does 0 to 1
cover an input's range, and does it add to the knob? Each of Poly's four
CV-driven modules takes the place of its own LFO with the CV, so the answer is
read from what the module does with a steady CV value:

  Flanger (CV)          the delay of its comb, from the input/output cross-correlation
  Chorus (CV)           the first voice's delay, the same way
  Vibrato (CV)          the lowest notch of its allpass chain, from the transfer function
  Harmonic tremolo (CV) the gain of its low and high bands

White noise goes in; the CV comes from this probe through a CV input of the
graph, so any value can be sent. Then Sway, resting at Point A = Point B,
replaces the probe: its CV must read back as set and give the same response.
Needs a running engine (see env.sh) with PolyLV2's *_ext modules and Sway.
"""
import math
import os
import sys
import threading
import time

import jack
import numpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ingenprobe import Client

POLY = "http://polyeffects.com/lv2/"
SWAY = "https://github.com/ABustos2606/sway#sway"
VALUES = (-1.0, -0.5, 0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0)


class Probe:
    def __init__(self):
        self.client = jack.Client("sway-probe")
        self.send = self.client.outports.register("send")
        self.cvsend = self.client.outports.register("cvsend")
        self.cap = self.client.inports.register("audio")
        self.cv = self.client.inports.register("cv")
        self.lock = threading.Lock()
        self.noise = numpy.random.default_rng(7).uniform(-0.25, 0.25, 1 << 20).astype(numpy.float32)
        self.at, self.cv_value, self.recording = 0, 0.0, False
        self.sent, self.heard, self.control = [], [], []
        self.client.set_process_callback(self.process)
        self.client.activate()
        self.rate = self.client.samplerate

    def process(self, frames):
        out = self.send.get_array()
        n = len(self.noise)
        idx = (self.at + numpy.arange(frames)) % n
        out[:] = self.noise[idx]
        self.at = (self.at + frames) % n
        with self.lock:
            self.cvsend.get_array()[:] = self.cv_value
            if self.recording:
                self.sent.append(out.copy())
                self.heard.append(self.cap.get_array().copy())
                self.control.append(self.cv.get_array().copy())

    def record(self, seconds, settle=0.4):
        time.sleep(settle)
        with self.lock:
            self.sent, self.heard, self.control = [], [], []
            self.recording = True
        time.sleep(seconds)
        with self.lock:
            self.recording = False
            return numpy.concatenate(self.sent), numpy.concatenate(self.heard), numpy.concatenate(self.control)


def xcorr(x, y, lags):
    """Cross-correlation of y against x for lags 0..lags-1, normalised by x's energy."""
    n = 1 << int(math.ceil(math.log2(len(x) + lags)))
    r = numpy.fft.irfft(numpy.conj(numpy.fft.rfft(x, n)) * numpy.fft.rfft(y, n), n)[:lags]
    return r / float(numpy.dot(x, x))


def transfer(x, y, rate, size=8192):
    """|H(f)| from averaged cross and auto spectra (Welch, Hann, half overlap)."""
    w = numpy.hanning(size)
    sxy = numpy.zeros(size // 2 + 1, complex)
    sxx = numpy.zeros(size // 2 + 1)
    for start in range(0, len(x) - size, size // 2):
        fx = numpy.fft.rfft(x[start:start + size] * w)
        fy = numpy.fft.rfft(y[start:start + size] * w)
        sxy += numpy.conj(fx) * fy
        sxx += numpy.abs(fx) ** 2
    return numpy.fft.rfftfreq(size, 1 / rate), numpy.abs(sxy / sxx)


def value(c, path, number):
    c.set(path, "ingen:value", "\"%s\"^^xsd:float" % number)


def main():
    probe = Probe()
    rate = probe.rate
    c = Client()
    for port, kind in (("in_l", "lv2:InputPort , lv2:AudioPort"), ("out_l", "lv2:OutputPort , lv2:AudioPort"),
                       ("cv_in", "lv2:InputPort , lv2:CVPort"), ("cv_out", "lv2:OutputPort , lv2:CVPort")):
        c.put("ingen:/main/" + port, "a " + kind)
    time.sleep(0.3)
    for a, b in (("sway-probe:send", "ingen:in_l"), ("ingen:out_l", "sway-probe:audio"),
                 ("sway-probe:cvsend", "ingen:cv_in"), ("ingen:cv_out", "sway-probe:cv")):
        probe.client.connect(a, b)
    failed = 0

    def report(ok, text):
        nonlocal failed
        failed += 0 if ok else 1
        print("%s  %s" % ("PASS" if ok else "FAIL", text), flush=True)

    # The round trip without a module: where the cross-correlation peaks.
    c.connect("ingen:/main/in_l", "ingen:/main/out_l")
    x, y, _ = probe.record(0.5)
    c.disconnect("ingen:/main/in_l", "ingen:/main/out_l")
    base = int(numpy.argmax(xcorr(x, y, 4096)))
    report(base < 4096, "the measurement itself -- a direct cable returns the noise %d samples later" % base)

    def peak_after(x, y, lo, hi):
        r = xcorr(x, y, base + hi)
        return lo + int(numpy.argmax(numpy.abs(r[base + lo:base + hi])))

    modules = {
        "flanger": (POLY + "flanger_ext", "in1", "out0", "lfo_cv",
                    {"Delay_0": 0.22, "Depth_1": 0.75, "Invert_2": 0, "Feedback_3": 0}),
        "chorus": (POLY + "chorus_ext", "in0", "out0", "in1", {"Delay_0": 0.5, "Depth_1": 1.0, "Deviation_2": 0.5}),
        "vibrato": (POLY + "vibrato_ext", "in1", "out0", "lfo_cv",
                    {"Invert_Internal_Phaser_Sum_0": 0, "Vibrato_Mode_1": 0, "Notch_Depth__Intensity__2": 1,
                     "Feedback_Gain_3": 0, "Notch_width_4": 300, "Min_Notch1_Freq_5": 200, "Max_Notch1_Freq_6": 1600,
                     "Notch_Freq_Ratio_7": 2.0}),
        "tremolo": (POLY + "harm_trem_ext", "in0", "out0", "lfo_in1", {"CrossoverFreq_0": 800, "depth_1": 1.0}),
    }

    def measure(name, x, y):
        if name == "flanger":   # comb delay in samples
            return float(peak_after(x, y, 20, 2200))
        if name == "chorus":    # voice 1 sits near curdel/6 = 682.7 samples; its offset is sigma*sin(cv)
            return float(peak_after(x, y, 400, 960)) - 8192 * 0.5 / 6
        if name == "vibrato":   # the lowest notch, Hz: the first dip below half gain, followed to its bottom
            f, h = transfer(x, y, rate)
            h = numpy.convolve(h, numpy.ones(5) / 5, mode="same")
            start = int(numpy.searchsorted(f, 40))
            below = numpy.flatnonzero(h[start:] < 0.5)
            if not len(below):
                return float("nan")
            i = start + int(below[0])
            while i + 1 < len(h) and h[i + 1] <= h[i]:
                i += 1
            return float(f[i])
        f, h = transfer(x, y, rate)  # tremolo: low and high band gains
        return (float(numpy.median(h[(f > 80) & (f < 250)])), float(numpy.median(h[(f > 4000) & (f < 10000)])))

    results = {}
    only = sys.argv[1:]   # module names to measure; all four by default
    for name, (uri, ain, aout, cvin, params) in modules.items():
        if only and name not in only:
            continue
        block = "ingen:/main/" + name
        status = c.put(block, "a ingen:Block ;\n\t\tlv2:prototype <%s>" % uri)[0]
        report(status == 0, "the engine loads %s (status %s)" % (uri, status))
        for symbol, number in params.items():
            value(c, block + "/" + symbol, number)
        c.connect("ingen:/main/in_l", block + "/" + ain)
        c.connect(block + "/" + aout, "ingen:/main/out_l")
        c.connect("ingen:/main/cv_in", block + "/" + cvin)
        values = VALUES + ((math.pi / 2, math.pi, 3 * math.pi / 2, 2 * math.pi) if name == "chorus" else ())
        results[name] = {}
        for v in values:
            probe.cv_value = v
            x, y, _ = probe.record(1.5, settle=1.0 if name == "chorus" else 0.4)
            results[name][v] = measure(name, x, y)
            shown = results[name][v]
            shown = "%.3f / %.3f" % shown if isinstance(shown, tuple) else "%.1f" % shown
            print("    %-8s cv %6.3f -> %s" % (name, v, shown), flush=True)
        c.disconnect("ingen:/main/cv_in", block + "/" + cvin)

        # Sway in place of the probe, resting at A = B.
        status = c.put("ingen:/main/sway", "a ingen:Block ;\n\t\tlv2:prototype <%s>" % SWAY)[0]
        report(status == 0, "the engine loads Sway (status %s)" % status)
        value(c, "ingen:/main/sway/start", 0)
        value(c, "ingen:/main/sway/shape", 1)
        c.connect("ingen:/main/sway/out", block + "/" + cvin)
        c.connect("ingen:/main/sway/out", "ingen:/main/cv_out")
        # Below 0 the flanger's delay would be negative: no defined response to compare.
        for v in ((0.0, 0.5, 1.0) if name == "flanger" else (-1.0, 0.0, 0.5, 1.0)):
            value(c, "ingen:/main/sway/point_a", v)
            value(c, "ingen:/main/sway/point_b", v)
            x, y, cv = probe.record(1.5, settle=1.0 if name == "chorus" else 0.4)
            got = measure(name, x, y)
            same = (all(abs(a - b) <= 0.02 + 0.05 * abs(b) for a, b in zip(got, results[name][v])) if isinstance(got, tuple)
                    else abs(got - results[name][v]) <= max(2.0, 0.03 * abs(results[name][v])))
            report(abs(float(numpy.median(cv)) - v) < 1e-4 and same,
                   "%s: Sway at %.2f reads back %.4f and gives the probe's response" % (name, v, float(numpy.median(cv))))
        c.delete("ingen:/main/sway")
        c.delete(block)
        time.sleep(0.3)

    print("RESULTS", repr(results), flush=True)
    c.delete("ingen:/main/cv_out")
    time.sleep(0.2)
    c.close()
    probe.client.deactivate()
    probe.client.close()
    sys.exit(1 if failed else 0)


main()
