// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "midi/settings.hpp"
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

namespace peepo::midi {
namespace {
// Lines of key=value; a line starting with # is a comment.
template <class Each> void lines(std::string_view text, Each&& each) {
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const auto equals = line.find('=');
        if (line.empty() || line[0] == '#' || equals == std::string_view::npos) continue;
        each(line.substr(0, equals), line.substr(equals + 1));
    }
}
bool on(std::string_view value) { return value == "on"; }
// A device name must stay on one line of the file.
bool storable(std::string_view name) { return !name.empty() && name.find_first_of("\r\n") == std::string_view::npos; }
std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}
std::string replace(const std::filesystem::path& path, const std::string& text) {
    std::error_code error;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
    auto temporary = path;
    temporary += ".new";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << text;
        if (!out.flush()) return "Could not write " + temporary.generic_string();
    }
    std::filesystem::rename(temporary, path, error);
    return error ? "Could not replace " + path.generic_string() + ": " + error.message() : std::string();
}
}
Settings parse_settings(std::string_view text) {
    Settings s;
    lines(text, [&](std::string_view key, std::string_view value) {
        if (key == "clock_out") s.clock_out = on(value);
        else if (key == "start_stop") s.start_stop = on(value);
        else if (key == "tempo_source") s.tempo_source = value == "clock" ? TempoSource::Clock : TempoSource::Patch;
        else if (key == "program_change") {
            int channel = 0;
            if (value == "any") s.program_channel = any_channel;
            else if (const auto r = std::from_chars(value.data(), value.data() + value.size(), channel); r.ec == std::errc{} && r.ptr == value.data() + value.size() && channel >= 1 && channel <= 16)
                s.program_channel = channel;
            else s.program_channel = program_off;
        } else if (key == "scene_cc") {
            int number = 0;
            if (const auto r = std::from_chars(value.data(), value.data() + value.size(), number); r.ec == std::errc{} && r.ptr == value.data() + value.size() && number >= 0 && number <= 127)
                s.scene_controller = number;
        } else if (key == "input_off" && storable(value)) s.inputs_off.emplace(value);
        else if (key == "output_off" && storable(value)) s.outputs_off.emplace(value);
    });
    return s;
}
std::string format_settings(const Settings& s) {
    std::string text = "# Peepo MIDI settings, written by the interface (Settings > MIDI).\n";
    text += std::string("clock_out=") + (s.clock_out ? "on" : "off") + "\n";
    text += std::string("start_stop=") + (s.start_stop ? "on" : "off") + "\n";
    text += std::string("tempo_source=") + (s.tempo_source == TempoSource::Clock ? "clock" : "patch") + "\n";
    text += "program_change=" + (s.program_channel == any_channel ? std::string("any") : s.program_channel >= 1 && s.program_channel <= 16 ? std::to_string(s.program_channel) : std::string("off")) + "\n";
    text += "scene_cc=" + std::to_string(s.scene_controller >= 0 && s.scene_controller <= 127 ? s.scene_controller : default_scene_controller) + "\n";
    for (const auto& name : s.inputs_off) if (storable(name)) text += "input_off=" + name + "\n";
    for (const auto& name : s.outputs_off) if (storable(name)) text += "output_off=" + name + "\n";
    return text;
}
Status parse_status(std::string_view text) {
    Status s;
    lines(text, [&](std::string_view key, std::string_view value) {
        double bpm = 0;
        if (key == "clock_in") {
            if (const auto r = std::from_chars(value.data(), value.data() + value.size(), bpm); r.ec == std::errc{} && std::isfinite(bpm) && bpm > 0) s.clock_in = bpm;
        } else if (key == "input" && storable(value)) s.inputs.emplace_back(value);
        else if (key == "output" && storable(value)) s.outputs.emplace_back(value);
    });
    return s;
}
std::string format_status(const Status& s) {
    std::string text;
    if (s.clock_in) {
        char buffer[32];
        const auto r = std::to_chars(buffer, buffer + sizeof buffer, std::round(*s.clock_in * 10) / 10, std::chars_format::fixed, 1);
        text += "clock_in=" + std::string(buffer, r.ptr) + "\n";
    }
    for (const auto& name : s.inputs) if (storable(name)) text += "input=" + name + "\n";
    for (const auto& name : s.outputs) if (storable(name)) text += "output=" + name + "\n";
    return text;
}
std::string device_name(std::string_view port) {
    std::string name(port);
    // " [24]" after the client's name.
    if (const auto open = name.find(" ["); open != std::string::npos)
        if (const auto close = name.find(']', open); close != std::string::npos &&
            close > open + 2 && name.find_first_not_of("0123456789", open + 2) == close) name.erase(open, close + 1 - open);
    return name;
}
std::string device_label(std::string_view name) {
    if (name.starts_with("a2j:")) name.remove_prefix(4);
    const auto direction = name.find(" (");
    const auto colon = name.find("): ");
    if (direction == std::string_view::npos || colon == std::string_view::npos || colon < direction) return std::string(name);
    const auto client = name.substr(0, direction), port = name.substr(colon + 3);
    // The port usually repeats the client's name ("Arturia MIDI 1"); say it only when it adds something.
    if (port.starts_with(client) && (port.size() == client.size() || port.substr(client.size()) == " MIDI 1")) return std::string(client);
    return std::string(client) + ": " + std::string(port.starts_with(client) ? port.substr(client.size() + 1) : port);
}
Settings Link::settings() const {
    std::error_code error;
    if (settings_.empty() || !std::filesystem::exists(settings_, error)) return {};
    return parse_settings(read(settings_));
}
std::string Link::save(const Settings& s) const {
    if (settings_.empty()) return "MIDI settings cannot be stored here.";
    return replace(settings_, format_settings(s));
}
Status Link::status() const {
    if (runtime_.empty()) return {};
    return parse_status(read(runtime_ / "status"));
}
void Link::write_status(const Status& s) const {
    if (!runtime_.empty()) replace(runtime_ / "status", format_status(s));
}
void Link::request_restart() const {
    if (!runtime_.empty()) replace(runtime_ / "restart", std::to_string(restart_requests() + 1) + "\n");
}
unsigned Link::restart_requests() const {
    if (runtime_.empty()) return 0;
    const auto text = read(runtime_ / "restart");
    unsigned count = 0;
    std::from_chars(text.data(), text.data() + text.size(), count);
    return count;
}
}
