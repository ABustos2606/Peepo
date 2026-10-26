// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "core/model.hpp"
#include "midi/settings.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

// MIDI clock out: 24 clock messages per beat at the patch tempo, each at the
// frame it falls on inside an audio period. Nothing here allocates or locks,
// so it runs in the audio thread. Peepo has no transport: Start is sent once,
// with the first clock message, and Stop when the clock ends.
namespace peepo::midi {
inline constexpr std::uint8_t clock_tick = 0xf8, clock_start = 0xfa, clock_continue = 0xfb, clock_stop = 0xfc;
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
    // Whether Start and Stop are sent; off, the clock begins and ends silently.
    void set_transport(bool on) { transport_ = on; }
    void stop() { if (state_ != State::Stopped) state_ = State::Stopping; }
    bool running() const { return state_ == State::Running || state_ == State::Starting; }
    // One audio period of `frames`: calls emit(offset, status) in time order,
    // each offset inside the period.
    template <class Emit> void process(std::uint32_t frames, Emit&& emit) {
        if (frames == 0) return;
        if (state_ == State::Stopping) { if (transport_) emit(std::uint32_t{0}, clock_stop); state_ = State::Stopped; return; }
        if (state_ == State::Stopped) return;
        if (state_ == State::Starting) { if (transport_) emit(std::uint32_t{0}, clock_start); next_ = 0; state_ = State::Running; }
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
    bool transport_ = true;
};

// MIDI clock in: the tempo of incoming clock messages, from the frames they
// arrive at. The mean interval over the last beat smooths USB jitter; the
// tempo is known after a quarter of a beat and lost when the clock stops.
// Nothing here allocates or locks.
class ClockIn {
public:
    explicit ClockIn(double sample_rate) : rate_(sample_rate) {}
    // A message at an absolute frame, counted from any start.
    void message(std::uint64_t frame, std::uint8_t status) {
        if (status == clock_stop || status == clock_start) { count_ = 0; return; }
        if (status != clock_tick) return;
        if (count_ && frame - times_[(count_ - 1) % times_.size()] > rate_ * lost_seconds) count_ = 0;
        times_[count_ % times_.size()] = frame;
        ++count_;
    }
    // The tempo at `now`, while a clock is heard and steady enough to tell.
    std::optional<double> tempo(std::uint64_t now) const {
        if (count_ < 7 || now - times_[(count_ - 1) % times_.size()] > rate_ * lost_seconds) return std::nullopt;
        const auto span = std::min<std::uint64_t>(count_ - 1, times_.size() - 1);
        const auto newest = times_[(count_ - 1) % times_.size()], oldest = times_[(count_ - 1 - span) % times_.size()];
        if (newest <= oldest) return std::nullopt;
        const double bpm = rate_ * 60.0 * static_cast<double>(span) / (static_cast<double>(newest - oldest) * ticks_per_beat);
        return std::isfinite(bpm) ? std::optional<double>{bpm} : std::nullopt;
    }
private:
    // The slowest clock, at 30 BPM, ticks every 83 ms.
    static constexpr double lost_seconds = 0.5;
    double rate_;
    // The last beat and one: 24 intervals.
    std::array<std::uint64_t, ticks_per_beat + 1> times_{};
    std::uint64_t count_ = 0;
};
// The program a message selects, 0 to 127, when it is a program change on
// `channel` (1 to 16; any_channel for every one; anything else never).
inline std::optional<int> program_change(const std::uint8_t* data, std::size_t size, int channel) {
    if (size < 2 || (data[0] & 0xf0) != 0xc0 || data[1] > 127) return std::nullopt;
    if (channel != any_channel && (channel < 1 || channel > 16 || (data[0] & 0x0f) != channel - 1)) return std::nullopt;
    return data[1];
}
// The scene a message selects, counted from 0, when it is control change
// `controller` on `channel` (as for program_change) with a value 0 to 7.
inline std::optional<std::size_t> scene_change(const std::uint8_t* data, std::size_t size, int channel, int controller) {
    if (size < 3 || (data[0] & 0xf0) != 0xb0 || data[1] != controller || data[2] >= max_scenes) return std::nullopt;
    if (channel != any_channel && (channel < 1 || channel > 16 || (data[0] & 0x0f) != channel - 1)) return std::nullopt;
    return data[2];
}
}
