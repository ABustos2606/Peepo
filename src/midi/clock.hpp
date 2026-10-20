// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "core/model.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

// MIDI clock out: 24 clock messages per beat at the patch tempo, each at the
// frame it falls on inside an audio period. Nothing here allocates or locks,
// so it runs in the audio thread. Peepo has no transport: Start is sent once,
// with the first clock message, and Stop when the clock ends.
namespace peepo::midi {
inline constexpr std::uint8_t clock_tick = 0xf8, clock_start = 0xfa, clock_stop = 0xfc;
inline constexpr int ticks_per_beat = 24;

class ClockOut {
public:
    explicit ClockOut(double sample_rate) : rate_(sample_rate) {}
    // Takes effect from the next clock message: the one already due keeps its
    // place, and the interval after it is the new tempo's.
    void set_tempo(double bpm) { if (std::isfinite(bpm)) tempo_ = std::clamp(bpm, minimum_tempo, maximum_tempo); }
    double tempo() const { return tempo_; }
    // Frames between two clock messages at the current tempo.
    double interval() const { return rate_ * 60.0 / (tempo_ * ticks_per_beat); }
    // Start with the next period, then clock; Stop at the start of the next one.
    void start() { if (state_ == State::Stopped) state_ = State::Starting; }
    void stop() { if (state_ != State::Stopped) state_ = State::Stopping; }
    bool running() const { return state_ == State::Running || state_ == State::Starting; }
    // One audio period of `frames`: calls emit(offset, status) in time order,
    // each offset inside the period.
    template <class Emit> void process(std::uint32_t frames, Emit&& emit) {
        if (frames == 0) return;
        if (state_ == State::Stopping) { emit(std::uint32_t{0}, clock_stop); state_ = State::Stopped; return; }
        if (state_ == State::Stopped) return;
        if (state_ == State::Starting) { emit(std::uint32_t{0}, clock_start); next_ = 0; state_ = State::Running; }
        // `next_` is the time of the next message from the period's first frame.
        while (next_ < frames) {
            emit(std::min(static_cast<std::uint32_t>(next_), frames - 1), clock_tick);
            next_ += interval();
        }
        next_ -= frames;
    }
private:
    enum class State { Stopped, Starting, Running, Stopping };
    double rate_, tempo_ = default_tempo, next_ = 0;
    State state_ = State::Stopped;
};
}
