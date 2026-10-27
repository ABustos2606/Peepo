// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// The wave Flow Envelope (Poly's port of Mutable Instruments Tides 2) plays on
// its first output, drawn on its page. Worked out from the same formulas as
// the module: Tides 2's wave tables (resources/lookup_tables.py), its skewed
// ramp (ramp_shaper.h), its folder and its filter (poly_slope_generator.h),
// by Emilie Gillet, MIT licence. A drawing, not the sound: the CV inputs and
// the frequency are left out.
namespace peepo {
inline bool flow_uri(const std::string& uri) { return uri == "http://polyeffects.com/lv2/polytides"; }

// The module's Ramp and Range values.
enum class FlowRamp { Ad = 0, Looping = 1, Ar = 2 };
inline constexpr int flow_audio_range = 2;

struct FlowSettings {
    double shape = 0.5, slope = 0.5, smoothness = 0.5;
    FlowRamp ramp = FlowRamp::Ad;
    int range = 0;
};

namespace flow_detail {
constexpr double pi = 3.14159265358979323846;
// The half curves of the phasor tables, 0 to 1 over x in 0 to 1.
inline double expo_max() { return 1.0 - std::exp(-5.0 * 511.0 / 512.0); }
inline double half_curve(int curve, double x) {
    switch (curve) {
        case 0: return std::log(1.0 - x * expo_max()) / -5.0;           // log
        case 1: return std::acos(1.0 - 2.0 * x) / pi;                   // inverse sine
        case 2: return x;                                                // linear
        case 3: return (1.0 - std::cos(pi * x)) / 2.0;                   // sine
        default: return (1.0 - std::exp(-5.0 * x)) / expo_max();        // expo
    }
}
// Tables 5 to 11: up the half curve, then down it again, mirrored or flipped.
inline double phasor_table(int table, double t) {
    static constexpr int curve[] = {0, 0, 1, 2, 3, 4, 4};
    static constexpr bool mirrored[] = {false, true, false, false, false, true, false};
    const int i = std::clamp(table - 5, 0, 6);
    if (t < 0.5) return half_curve(curve[i], 2 * t);
    return mirrored[i] ? half_curve(curve[i], 2 - 2 * t) : 1 - half_curve(curve[i], 2 * t - 1);
}
// Tables 0 to 4, for a looping wave at audio rate.
inline double audio_table(int table, double x) {
    switch (std::clamp(table, 0, 4)) {
        case 0: {
            const double scale = std::atan(8.0);
            return x <= 0 ? 0 : std::acos(std::tan(scale * (1 - 2 * x)) / 8) / pi;
        }
        case 1: return std::acos(1 - 2 * x) / pi;
        case 2: return x;
        case 3: return 0.5 - 0.5 * std::cos(x * pi);
        default: {
            const double crop = std::min(1.0, 4 - 4 * x);
            return (1 - std::cos(pi * x * 1.5)) * (1 - std::cos(pi * crop)) / 4.5;
        }
    }
}
inline double bipolar_fold_raw(double x) {
    const double window = std::pow(std::exp(-x * x * 4), 2);
    return std::sin(8 * pi * x) * window + std::atan(3 * x) * (1 - window);
}
inline double unipolar_fold_raw(double x) {
    const double window = std::pow(std::exp(-x * x * 4), 2);
    return (0.38 * std::sin(16 * pi * x) + 4 * x) * window + std::atan(4 * x) * (1 - window);
}
// Each fold table is scaled to its own largest value.
inline double fold_peak(bool bipolar) {
    double peak = 0;
    for (int i = 0; i <= 1024; ++i) {
        const double x = bipolar ? i / 512.0 - 1 : i / 1024.0;
        peak = std::max(peak, std::abs(bipolar ? bipolar_fold_raw(x) : unipolar_fold_raw(x)));
    }
    return peak;
}
}  // namespace flow_detail

// Out 1 across `points` steps, each 0 (bottom) to 1 (top). A looping wave is
// two cycles about the middle; a one-shot rests, plays once and rests again.
inline std::vector<double> flow_wave(const FlowSettings& s, int points) {
    using namespace flow_detail;
    points = std::max(points, 2);
    const bool looping = s.ramp == FlowRamp::Looping;
    const bool audio = s.range == flow_audio_range;
    const bool phasor = !(audio && looping);
    const double shape = std::clamp(s.shape, 0.0, 1.0), smoothness = std::clamp(s.smoothness, 0.0, 1.0);
    double pw = std::clamp(s.slope, 0.0, 1.0);
    // At control rate a short attack gets more of the knob, as in the module.
    if (!audio && pw < 0.5) pw = 0.5 + 0.6 * (pw - 0.5) / (std::abs(pw - 0.5) + 0.1);
    pw = std::clamp(pw, 0.002, 0.998);
    const double table_at = phasor ? shape * 5.9999 + 5 : shape * 3.9999;
    const int table = static_cast<int>(table_at);
    const double between = table_at - table;
    const auto shaped = [&](double t) {
        const auto at = [&](int n) { return phasor ? phasor_table(n, t) : audio_table(n, t); };
        return at(table) + (at(table + 1) - at(table)) * between;
    };
    const double fold = std::max(2 * (smoothness - 0.5), 0.0);
    static const double bipolar_peak = fold_peak(true), unipolar_peak = fold_peak(false);
    const auto folded = [&](double u) {
        if (fold <= 0) return u;
        if (looping) {
            const double b = 2 * u - 1;
            const double f = bipolar_fold_raw(2 * (0.5 + b * (0.03 + 0.46 * fold)) - 1) / bipolar_peak;
            return (b + (f - b) * fold + 1) / 2;
        }
        const double f = unipolar_fold_raw(u * fold) / unipolar_peak;
        return u + (f - u) * fold;
    };
    // Where the ramp is at each step, and how many steps one ramp takes.
    std::vector<double> phase(static_cast<std::size_t>(points));
    std::vector<bool> resting(phase.size(), false);
    double ramp_steps = points;
    for (int i = 0; i < points; ++i) {
        const double x = static_cast<double>(i) / (points - 1);
        auto& p = phase[static_cast<std::size_t>(i)];
        if (looping) {
            ramp_steps = points / 2.0;
            const double c = std::fmod(x * 2, 1.0);
            // Control rate: a ramp skewed by the slope into the phasor tables;
            // audio rate: a triangle into the plain tables.
            if (phasor) p = c < pw ? c * 0.5 / pw : (c - pw) * 0.5 / (1 - pw) + 0.5;
            else p = c < pw ? c / pw : 1 - (c - pw) / (1 - pw);
        } else if (s.ramp == FlowRamp::Ad) {
            // Rest, then attack and decay in the slope's proportion.
            constexpr double start = 0.08, length = 0.62;
            ramp_steps = length * points;
            const double c = (x - start) / length;
            if (c < 0 || c >= 1) { p = 0; resting[static_cast<std::size_t>(i)] = true; }
            else p = c < pw ? c * 0.5 / pw : (c - pw) * 0.5 / (1 - pw) + 0.5;
        } else {
            // Rest, attack, held while the gate is up, release.
            constexpr double start = 0.08, ramps = 0.5, hold = 0.3;
            ramp_steps = ramps * points;
            const double attack = ramps * pw, release = ramps * (1 - pw);
            const double c = x - start;
            if (c < 0 || c >= attack + hold + release) { p = 0; resting[static_cast<std::size_t>(i)] = true; }
            else if (c < attack) p = 0.5 * c / attack;
            else if (c < attack + hold) p = 0.5;
            else p = 0.5 + 0.5 * (c - attack - hold) / release;
        }
    }
    std::vector<double> wave(phase.size());
    for (std::size_t i = 0; i < phase.size(); ++i)
        wave[i] = resting[i] ? (looping ? 0.5 : 0.0) : std::clamp(folded(shaped(phase[i])), 0.0, 1.0);
    // Below noon, Smoothness filters the wave, down to half its own rate.
    if (smoothness < 0.5) {
        double ratio = smoothness * 2; ratio *= ratio; ratio *= ratio;
        const double rate = 1.0 / ramp_steps;
        const double f = std::min(0.5, rate * 0.5 + (1 - rate * 0.5) * ratio);
        const double a = 1 - std::exp(-2 * pi * f);
        // A looping wave runs once before it is drawn, so it starts settled.
        double one = wave[0], two = wave[0];
        const auto filter = [&](double in) { one += a * (in - one); two += a * (one - two); return two; };
        if (looping) for (const double v : wave) filter(v);
        for (auto& v : wave) v = filter(v);
    }
    return wave;
}
}  // namespace peepo
