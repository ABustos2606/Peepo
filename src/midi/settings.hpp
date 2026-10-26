// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// Instrument-wide MIDI (Settings > MIDI), shared by three programs through
// two small files. The interface writes the settings, kept beside the
// patches; peepo-midi and peepo-route read them again whenever they change.
// peepo-midi writes what it hears (the devices, incoming clock) into its
// runtime folder, where the interface reads it and asks for a clock restart.
namespace peepo::midi {
enum class TempoSource { Patch, Clock };
// Program change: off, one channel (1 to 16), or any.
inline constexpr int program_off = 0, any_channel = 17;
// The control change that selects a scene, on the program-change channel
// (docs/44-scenes.md): Line 6's choice, so foot controllers set up for it work.
inline constexpr int default_scene_controller = 69;
struct Settings {
    bool clock_out = true;
    // Start with the clock and Stop when it ends; off sends the clock alone.
    bool start_stop = true;
    TempoSource tempo_source = TempoSource::Patch;
    int program_channel = program_off;
    // Its value 0 to 7 plays scene 1 to 8; off while program change is.
    int scene_controller = default_scene_controller;
    // Devices left out, by name, as senders (inputs) and as receivers (outputs).
    std::set<std::string> inputs_off, outputs_off;
    bool operator==(const Settings&) const = default;
};
// Unknown lines and values are ignored, so an older program reads a newer file.
Settings parse_settings(std::string_view text);
std::string format_settings(const Settings&);
// What peepo-midi tells the interface.
struct Status {
    // Incoming MIDI clock, in BPM, while one is heard.
    std::optional<double> clock_in;
    // Devices present: those that send MIDI, and those that receive it.
    std::vector<std::string> inputs, outputs;
    bool operator==(const Status&) const = default;
};
Status parse_status(std::string_view text);
std::string format_status(const Status&);
// A device's name: its JACK MIDI port as a2jmidid names it, without ALSA's
// client number, which changes from one plugging to the next:
// "a2j:Arturia [24] (capture): Arturia MIDI 1" is "a2j:Arturia (capture): Arturia MIDI 1".
std::string device_name(std::string_view jack_port);
// What to call it on screen: "Arturia", or "Arturia: Port 2" when the port says more.
std::string device_label(std::string_view name);

// The two files. Either path may be empty: then nothing is read or written there.
class Link {
public:
    Link(std::filesystem::path settings, std::filesystem::path runtime) : settings_(std::move(settings)), runtime_(std::move(runtime)) {}
    const std::filesystem::path& settings_path() const { return settings_; }
    // Defaults when there is no file yet.
    Settings settings() const;
    // Written whole and renamed into place, so a reader never sees half of it. Returns what went wrong, or "".
    std::string save(const Settings&) const;
    Status status() const;
    void write_status(const Status&) const;
    // The interface asks for Stop then Start (the Tempo page's Restart): a
    // number that goes up by one each time.
    void request_restart() const;
    unsigned restart_requests() const;
    // Whether peepo-midi's runtime folder is known here at all.
    bool connected() const { return !runtime_.empty(); }
private:
    std::filesystem::path settings_, runtime_;
};
}
