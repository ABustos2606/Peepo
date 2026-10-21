// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// MIDI clock out: where each message lands, period by period.
#include "midi/clock.hpp"
#include <algorithm>
#include <filesystem>
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
void silent_transport() {
    // Start and Stop off: the clock alone, for a follower that starts its pattern on Start.
    ClockOut clock(48000);
    clock.set_transport(false);
    clock.start();
    std::int64_t now = 0;
    auto messages = run(clock, 128, 20, now);
    clock.stop();
    for (const auto& m : run(clock, 128, 2, now)) messages.push_back(m);
    check(!messages.empty() && std::all_of(messages.begin(), messages.end(), [](const Message& m) { return m.status == clock_tick; }) && !clock.running(),
          "Start or Stop sent with them switched off");
}
void clock_in() {
    // 120 BPM is a clock every 1000 frames at 48 kHz; USB moves each by up to 48 frames (1 ms).
    ClockIn in(48000);
    check(!in.tempo(0), "a tempo before any clock");
    std::uint64_t frame = 5000;
    int jitter[] = {0, 48, -48, 20, -30, 5, 40, -10};
    for (int i = 0; i < 6; ++i) in.message(frame + i * 1000 + jitter[i % 8], clock_tick);
    check(!in.tempo(frame + 5000), "a tempo from too few clocks");
    for (int i = 6; i < 200; ++i) in.message(frame + i * 1000 + jitter[i % 8], clock_tick);
    const auto heard = in.tempo(frame + 199 * 1000);
    check(heard && std::abs(*heard - 120) < 0.3, "jittery 120 BPM not heard as 120");
    // A change shows within a beat.
    frame += 199 * 1000;
    for (int i = 1; i <= 26; ++i) in.message(frame + i * 2000, clock_tick);
    check(std::abs(in.tempo(frame + 26 * 2000).value_or(0) - 60) < 0.01, "a change to 60 BPM not heard within a beat");
    // Lost when the clock stops, or when it falls silent.
    check(!in.tempo(frame + 26 * 2000 + 48000), "a silent clock still has a tempo");
    for (int i = 0; i < 10; ++i) in.message(frame + 200000 + i * 400, clock_tick);
    check(std::abs(in.tempo(frame + 200000 + 3600).value_or(0) - 300) < 0.01, "a clock that came back not heard afresh");
    in.message(frame + 204000, clock_stop);
    check(!in.tempo(frame + 204000), "a stopped clock still has a tempo");
    in.message(frame + 300000, 0x90);
    check(!in.tempo(frame + 300000), "a note made a tempo");
}
void program_changes() {
    const std::uint8_t on_three[] = {0xc2, 5}, note[] = {0x92, 60, 100};
    check(program_change(on_three, 2, 3) == 5 && program_change(on_three, 2, any_channel) == 5, "program change on its channel");
    check(!program_change(on_three, 2, 1) && !program_change(on_three, 2, program_off) && !program_change(on_three, 1, 3) && !program_change(note, 3, any_channel),
          "a program change taken from another channel, while off, cut short, or from a note");
}
void settings_files() {
    Settings s;
    check(parse_settings("") == s && s.clock_out && s.start_stop && s.tempo_source == TempoSource::Patch && s.program_channel == program_off, "defaults");
    s.clock_out = false; s.tempo_source = TempoSource::Clock; s.program_channel = 16;
    s.inputs_off = {"a2j:Arturia (capture): Arturia MIDI 1"}; s.outputs_off = {"a2j:KO-2 (playback): KO-2"};
    check(parse_settings(format_settings(s)) == s, "settings do not read back");
    s.program_channel = any_channel;
    check(parse_settings(format_settings(s)).program_channel == any_channel, "any channel does not read back");
    check(parse_settings("program_change=17\nprogram_change=0x3\nsomething=else\r\n").program_channel == program_off, "a bad channel or an unknown line was taken");
    Status status{119.96, {"a2j:A (capture): A"}, {"a2j:B (playback): B"}};
    const auto back = parse_status(format_status(status));
    check(back.clock_in && std::abs(*back.clock_in - 120) < 1e-9 && back.inputs == status.inputs && back.outputs == status.outputs, "status does not read back");
    check(!parse_status("clock_in=nan\n").clock_in && !parse_status("").clock_in, "no clock read as a clock");
    check(device_name("a2j:Arturia MiniLab mkII [24] (capture): Arturia MiniLab mkII MIDI 1") == "a2j:Arturia MiniLab mkII (capture): Arturia MiniLab mkII MIDI 1" &&
          device_name("a2j:Odd [x] (capture): Odd") == "a2j:Odd [x] (capture): Odd", "ALSA's client number not removed");
    check(device_label("a2j:Arturia MiniLab mkII (capture): Arturia MiniLab mkII MIDI 1") == "Arturia MiniLab mkII" &&
          device_label("a2j:UM-ONE (playback): UM-ONE") == "UM-ONE" &&
          device_label("a2j:Virtual Raw MIDI 1-0 (capture): VirMIDI 1-0") == "Virtual Raw MIDI 1-0: VirMIDI 1-0" &&
          device_label("a2j:MIDI4x4 (capture): MIDI4x4 MIDI 2") == "MIDI4x4: MIDI 2", "device labels");
    // The files: written whole, read back; restart requests count up.
    const auto folder = std::filesystem::temp_directory_path() / "peepo-midi-tests";
    std::filesystem::remove_all(folder);
    const Link link(folder / "midi.conf", folder / "run");
    check(link.settings() == Settings{} && link.status() == Status{} && link.restart_requests() == 0, "missing files are not the defaults");
    check(link.save(s).empty() && link.settings() == s, "saved settings do not read back");
    link.write_status(status);
    check(link.status().inputs == status.inputs, "status file");
    link.request_restart(); link.request_restart();
    check(link.restart_requests() == 2, "restart requests");
    check(!Link({}, {}).save(s).empty() && !Link({}, {}).connected(), "a link with no files claims to store");
    std::filesystem::remove_all(folder);
}
}
int main() {
    try {
        intervals();
        tempo_changes();
        stopping();
        silent_transport();
        clock_in();
        program_changes();
        settings_files();
        std::cout << "MIDI clock out and in, Start and Stop, program change and the settings files passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
