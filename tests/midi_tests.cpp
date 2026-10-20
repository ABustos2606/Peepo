// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// MIDI clock out: where each message lands, period by period.
#include "midi/clock.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace peepo::midi;
namespace {
void check(bool result, const char* why) { if (!result) throw std::runtime_error(why); }
struct Message { std::int64_t frame; std::uint8_t status; };
// Runs the clock for `periods` of `frames`, recording absolute frames.
std::vector<Message> run(ClockOut& clock, std::uint32_t frames, int periods, std::int64_t& now) {
    std::vector<Message> out;
    for (int p = 0; p < periods; ++p) {
        std::int64_t last = -1;
        clock.process(frames, [&](std::uint32_t offset, std::uint8_t status) {
            check(offset < frames && static_cast<std::int64_t>(offset) >= last, "a message outside its period or out of order");
            last = offset;
            out.push_back({now + offset, status});
        });
        now += frames;
    }
    return out;
}
std::vector<std::int64_t> ticks(const std::vector<Message>& messages) {
    std::vector<std::int64_t> at;
    for (const auto& m : messages) if (m.status == clock_tick) at.push_back(m.frame);
    return at;
}
void intervals() {
    // 48 kHz: 1000 frames per clock at 120 BPM, 4000 at 30, 400 at 300, in
    // periods that do not divide them.
    for (const auto& [bpm, expected] : {std::pair{120.0, 1000}, std::pair{30.0, 4000}, std::pair{300.0, 400}})
        for (const std::uint32_t frames : {64u, 128u, 256u, 1000u, 333u}) {
            ClockOut clock(48000);
            clock.set_tempo(bpm); clock.start();
            std::int64_t now = 0;
            const auto messages = run(clock, frames, 2000, now);
            check(messages.size() > 2 && messages[0].status == clock_start && messages[0].frame == 0 && messages[1].status == clock_tick && messages[1].frame == 0,
                  "Start does not come once, just before the first clock");
            const auto at = ticks(messages);
            for (std::size_t i = 1; i < at.size(); ++i) check(at[i] - at[i - 1] == expected, "a clock interval is wrong");
            check(std::count_if(messages.begin(), messages.end(), [](const Message& m) { return m.status == clock_start; }) == 1, "Start sent more than once");
        }
    // A tempo whose interval is not whole frames neither drifts nor jitters by
    // more than a frame: 133 BPM is 902.255... frames, over an hour.
    ClockOut clock(48000);
    clock.set_tempo(133); clock.start();
    std::int64_t now = 0;
    const auto at = ticks(run(clock, 128, 48000 * 3600 / 128, now));
    const double exact = 48000.0 * 60 / (133 * 24);
    for (std::size_t i = 0; i < at.size(); ++i) check(std::abs(static_cast<double>(at[i]) - std::floor(i * exact)) <= 1, "the clock drifts");
    for (std::size_t i = 1; i < at.size(); ++i) check(at[i] - at[i - 1] == 902 || at[i] - at[i - 1] == 903, "the clock jitters by more than a frame");
}
void tempo_changes() {
    // A change keeps the message already due and spaces the next ones anew.
    ClockOut clock(48000);
    clock.start();
    std::int64_t now = 0;
    auto before = ticks(run(clock, 128, 40, now));   // 5120 frames: clocks at 0, 1000 ... 5000
    check(before.back() == 5000, "setup");
    clock.set_tempo(60);                             // 2000 frames from the next clock on
    const auto after = ticks(run(clock, 128, 80, now));
    check(after.size() >= 3 && after[0] == 6000 && after[1] == 8000 && after[2] == 10000, "a tempo change did not take effect from the next clock message");
    clock.set_tempo(1000); check(clock.tempo() == 300, "a tempo above the range was not held at 300");
    clock.set_tempo(std::nan("")); check(clock.tempo() == 300, "a non-number changed the tempo");
    clock.set_tempo(1); check(clock.tempo() == 30, "a tempo below the range was not held at 30");
}
void stopping() {
    ClockOut clock(48000);
    std::int64_t now = 0;
    check(run(clock, 128, 10, now).empty() && !clock.running(), "a clock never started sent something");
    clock.start();
    run(clock, 128, 10, now);
    clock.stop();
    const auto end = run(clock, 128, 20, now);
    check(end.size() == 1 && end[0].status == clock_stop && end[0].frame == 2560 && !clock.running(), "Stop is not alone at the start of the next period");
    clock.start();
    const auto again = run(clock, 128, 1, now);
    check(again.size() == 2 && again[0].status == clock_start && again[1].status == clock_tick && again[0].frame == again[1].frame, "the clock does not start again");
}
}
int main() {
    try {
        intervals();
        tempo_changes();
        stopping();
        std::cout << "MIDI clock intervals, drift, tempo changes, Start and Stop passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
