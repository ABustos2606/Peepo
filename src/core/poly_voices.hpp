// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "core/model.hpp"
#include <string>

// MIDI to CV poly plays its notes on its first Voices voices, but cannot see
// which of its outputs are cabled: notes sent to a voice with no cables are
// silent. Peepo sets Voices from the cables instead (docs/39-midi.md, P12.9).
namespace peepo {
inline constexpr const char* midi_cv_poly_uri = "https://github.com/ABustos2606/midi-cv#poly";
inline constexpr int midi_cv_poly_max_voices = 4;

// The highest voice with a cable on its pitch or gate output, or 0 when none is.
inline int cabled_voices(const Graph& graph, const Module& module) {
    int highest = 0;
    for (const auto& c : graph.connections) {
        if (c.from != module.id) continue;
        for (int voice = highest + 1; voice <= midi_cv_poly_max_voices; ++voice) {
            const auto n = std::to_string(voice);
            if (c.output == "pitch_" + n || c.output == "gate_" + n) highest = voice;
        }
    }
    return highest;
}
}
