// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
//
// Mist Beat Repeat (polyclouds#beat_repeat) after Peepo's Clouds patch, checked
// without an engine: the plugin binary is loaded directly and fed white noise.
// For each window of the output it finds how far back in the input that window
// came from: pass-through gives a constant lag of a few samples, a repeat a
// jump. Built and run by 30-beat-repeat.sh.
#include <dlfcn.h>
#include <lv2/core/lv2.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

static const double RATE = 48000.0;
static std::string so_path;
static int failures = 0;

static void verdict(bool ok, const std::string& what) {
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

struct Run { std::vector<float> in, out; };

static const LV2_Descriptor* find(const std::string& uri) {
    static void* h = nullptr;
    if (!h) h = dlopen(so_path.c_str(), RTLD_NOW);
    if (!h) { fprintf(stderr, "%s\n", dlerror()); exit(2); }
    auto fn = (LV2_Descriptor_Function)dlsym(h, "lv2_descriptor");
    for (uint32_t i = 0;; ++i) {
        const LV2_Descriptor* d = fn(i);
        if (!d) break;
        if (uri == d->URI) return d;
    }
    fprintf(stderr, "no %s in %s\n", uri.c_str(), so_path.c_str());
    exit(2);
}

// Control ports from index 16: 16 Loop start, 17 Loop size, 18 Pitch, 19 Gain,
// 20 Loop shrink, 21 Slice step, 22 Chance, 23 Slice length, 24 Pitch mode,
// 25 Reverb, 26 Repeat, 27 Reverse, 28 enabled, 29 Output; beat repeat only:
// 30 BPM, 31 Clock note, 32 Clock from Trigger in.
static Run run(const std::string& uri, std::vector<float> ctl, size_t total, uint32_t period,
               const std::vector<size_t>& trig = {}, bool sine = false) {
    const LV2_Descriptor* d = find(uri);
    LV2_Handle h = d->instantiate(d, RATE, "", nullptr);
    std::vector<std::vector<float>> cv(12, std::vector<float>(period, 0.0f));
    std::vector<float> il(period), ir(period), ol(period), orr(period);
    const size_t nctl = uri.find("beat_repeat") != std::string::npos ? 17 : 14;
    ctl.resize(17);
    for (int i = 0; i < 12; ++i) d->connect_port(h, i, cv[i].data());
    d->connect_port(h, 12, il.data());
    d->connect_port(h, 13, ir.data());
    d->connect_port(h, 14, ol.data());
    d->connect_port(h, 15, orr.data());
    for (size_t i = 0; i < nctl; ++i) d->connect_port(h, 16 + i, &ctl[i]);
    if (d->activate) d->activate(h);
    std::mt19937 rng(1);
    std::normal_distribution<float> noise(0.0f, 0.1f);
    Run r;
    for (size_t pos = 0; pos < total; pos += period) {
        for (uint32_t i = 0; i < period; ++i) {
            const float x = sine ? 0.1f * std::sin(2 * M_PI * 220.0 * (pos + i) / RATE) : noise(rng);
            il[i] = ir[i] = x;
            r.in.push_back(x);
            float t = 0.0f;
            for (size_t p : trig) if (pos + i >= p && pos + i < p + 48) t = 1.0f;
            cv[1][i] = t;
        }
        d->run(h, period);
        for (uint32_t i = 0; i < period; ++i) r.out.push_back(ol[i]);
    }
    if (d->deactivate) d->deactivate(h);
    d->cleanup(h);
    return r;
}

static double rms(const std::vector<float>& v, size_t a, size_t b) {
    double s = 0;
    for (size_t i = a; i < b; ++i) s += double(v[i]) * v[i];
    return std::sqrt(s / double(b - a));
}

static double gain(const Run& r) { return rms(r.out, 24000, r.out.size()) / rms(r.in, 24000, r.in.size()); }

static bool finite(const Run& r) {
    for (float x : r.out) if (!std::isfinite(x)) return false;
    return true;
}

// How far back in the input out[w, w+W) came from; -1 when nothing matches.
static long lag(const Run& r, size_t w, size_t W, long maxlag = 30000) {
    double eo = 0;
    for (size_t i = w; i < w + W; ++i) eo += double(r.out[i]) * r.out[i];
    if (eo < 1e-9) return -1;
    double best = -1; long at = -1;
    for (long L = 0; L <= maxlag && long(w) - L >= 0; ++L) {
        double c = 0, ei = 0;
        for (size_t i = w; i < w + W; ++i) { c += double(r.out[i]) * r.in[i - L]; ei += double(r.in[i - L]) * r.in[i - L]; }
        const double n = c / std::sqrt(eo * ei + 1e-12);
        if (n > best) { best = n; at = L; }
    }
    return best > 0.5 ? at : -1;
}

static std::vector<long> lags(const Run& r, size_t from, size_t count, size_t W) {
    std::vector<long> v;
    for (size_t k = 0; k < count; ++k) v.push_back(lag(r, from + k * W, W));
    return v;
}

static std::string show(const std::vector<long>& v) {
    std::string s;
    for (long x : v) s += " " + std::to_string(x);
    return s;
}

static bool all_near(const std::vector<long>& v, size_t a, size_t b, long want, long tol) {
    for (size_t i = a; i < b; ++i) if (std::labs(v[i] - want) > tol) return false;
    return true;
}

int main(int argc, char** argv) {
    so_path = argc > 1 ? argv[1] : "/opt/peepo/lib/lv2/polylenticular.lv2/polyclouds.so";
    const std::string BR = "http://polyeffects.com/lv2/polyclouds#beat_repeat";
    // Defaults as in the patched Turtle file: Chance 0.5, Loop size 0.45 (half
    // a slice, forward), 120 BPM, Clock note 1/8 (12000 samples at 48 kHz).
    const std::vector<float> base = {0, 0.45f, 0, 0.5f, 0, 0, 0.5f, 0, 0, 0, 0, 0, 1, 0, 120, 1, 0};
    const size_t N = 48000 * 4;

    { auto c = base; c[6] = 0; const Run r = run(BR, c, N, 128); const auto v = lags(r, 48000, 12, 256);
      printf("Chance 0, lags per 256 samples:%s\n", show(v).c_str());
      verdict(std::fabs(gain(r) - 1.0) < 0.03 && all_near(v, 0, v.size(), 2, 3) && finite(r),
              "Chance 0 passes the input through at its own level (gain " + std::to_string(gain(r)) + ")"); }

    for (uint32_t period : {128u, 100u}) {
        auto c = base; c[10] = 1; const Run r = run(BR, c, N, period); const auto v = lags(r, 48000, 24, 1000);
        printf("Repeat on, period %u, lags per 1000 samples:%s\n", period, show(v).c_str());
        verdict(all_near(v, 0, 6, 3, 3) && all_near(v, 6, 12, 6003, 3) && all_near(v, 12, 18, 3, 3) && all_near(v, 18, 24, 6003, 3) && finite(r),
                "Repeat on, host period " + std::to_string(period) + ": every 1/8 at 120 BPM plays its first half, then repeats it");
    }

    { auto c = base; c[10] = 1; c[16] = 1; const Run r = run(BR, c, N, 128); const auto v = lags(r, 48000, 24, 1000);
      verdict(all_near(v, 0, v.size(), 2, 3), "Clock from Trigger in with nothing cabled: no repeats"); }

    { auto c = base; c[10] = 1; c[16] = 1; std::vector<size_t> t;
      for (size_t p = 0; p < N; p += 6000) t.push_back(p);
      const Run r = run(BR, c, N, 128, t); const auto v = lags(r, 48000, 24, 1000);
      printf("Trigger in every 6000 samples, lags per 1000 samples:%s\n", show(v).c_str());
      bool repeats = false; for (long x : v) if (std::labs(x - 3000) < 40) repeats = true;
      verdict(repeats, "Clock from Trigger in: a trigger every 6000 samples repeats half-slices of 3000"); }

    { const Run r = run(BR, base, 48000 * 20, 128); int repeated = 0, passed = 0;
      for (size_t t = 48000; t + 12000 <= r.out.size(); t += 12000) {
          const long L = lag(r, t + 9000, 512);
          (L >= 0 && L < 200 ? passed : repeated) += 1;
      }
      verdict(repeated >= 25 && repeated <= 51, "Chance 0.5: " + std::to_string(repeated) + " of " + std::to_string(repeated + passed) + " clocks repeated"); }

    { auto c = base; c[10] = 1; c[23 - 16] = 1; c[24 - 16] = 1; c[2] = -12; c[21 - 16] = 7; c[25 - 16] = 0.5f; c[4] = 0.6f;
      const Run r = run(BR, c, N, 128);
      verdict(finite(r) && gain(r) > 0.1, "2-clock slices, Reverse, Pitch -12, Random step, Reverb, Loop shrink: finite and sounding (gain " + std::to_string(gain(r)) + ")"); }

    { auto c = base; c[6] = 0; c[13] = -6; const Run r = run(BR, c, N, 128);
      verdict(std::fabs(gain(r) - 0.501) < 0.02, "Output -6 dB (gain " + std::to_string(gain(r)) + ")"); }

    for (const char* m : {"granular", "stretch", "looping_delay", "spectral", "oliverb", "resonestor"}) {
        const std::vector<float> c = {0.5f, 0.5f, 0, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0, 0, 1, 0};
        const Run r = run(std::string("http://polyeffects.com/lv2/polyclouds#") + m, c, N, 128, {}, true);
        printf("%s: gain %.3f\n", m, gain(r));
        verdict(finite(r) && gain(r) > 0.05, std::string(m) + " still runs");
    }
    printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
