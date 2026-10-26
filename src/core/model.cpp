// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "core/model.hpp"
#include "core/looper.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_set>
#include <limits>

namespace peepo {
const std::vector<ModuleDefinition>& demo_catalogue() {
    static const std::vector<ModuleDefinition> catalogue = {
        // Beside the audio, IN brings every MIDI device's messages, merged,
        // and OUT sends to them.
        {"urn:peepo:demo:input", "IN", "Endpoint", "Audio input", 0x93a4b5,
         {{"out", Direction::Output}, {"out_r", Direction::Output}, {"midi", Direction::Output, 1, PortType::Midi}}, {}},
        {"urn:peepo:demo:output", "OUT", "Endpoint", "Audio output", 0x93a4b5,
         {{"in", Direction::Input}, {"in_r", Direction::Input}, {"midi", Direction::Input, 1, PortType::Midi}}, {}},
        {"urn:peepo:demo:drive", "Drive", "Drive", "Warm saturation and tone", 0xf0aa72,
         {{"in", Direction::Input}, {"in_r", Direction::Input}, {"out", Direction::Output}, {"out_r", Direction::Output}},
         {{"drive", "Drive", "%", ParameterKind::Continuous, 0, 100, 32},
          {"tone", "Tone", "Hz", ParameterKind::Continuous, 200, 8000, 2400, true},
          {"level", "Level", "dB", ParameterKind::Continuous, -24, 12, -3}}},
        {"urn:peepo:demo:clouds", "Mist Demo", "Texture", "Granular texture playground", 0xbfa1ed,
         {{"in", Direction::Input}, {"in_r", Direction::Input}, {"out", Direction::Output}, {"out_r", Direction::Output}},
         {{"position", "Position", "%", ParameterKind::Continuous, 0, 100, 45},
          {"size", "Size", "ms", ParameterKind::Continuous, 10, 1000, 180, true},
          {"mix", "Mix", "%", ParameterKind::Continuous, 0, 100, 40},
          {"freeze", "Freeze", "", ParameterKind::Toggle, 0, 1, 0},
          {"density", "Density", "%", ParameterKind::Continuous, 0, 100, 50},
          {"pitch", "Pitch", "st", ParameterKind::Integer, -24, 24, 0}}},
        {"urn:peepo:demo:delay", "Delay", "Delay", "Echoes with space to breathe", 0x7bbaed,
         {{"in", Direction::Input}, {"in_r", Direction::Input}, {"out", Direction::Output}, {"out_r", Direction::Output}},
         {{"time", "Time", "ms", ParameterKind::Continuous, 20, 2000, 380, true},
          {"feedback", "Feedback", "%", ParameterKind::Continuous, 0, 95, 35},
          {"mix", "Mix", "%", ParameterKind::Continuous, 0, 100, 28},
          {"mode", "Mode", "", ParameterKind::Enumeration, 0, 2, 0, false, {"Clean", "Tape", "Ping pong"}}}},
        {"urn:peepo:demo:reverb", "Reverb", "Reverb", "Room, hall and a little shimmer", 0x81c9b8,
         {{"in", Direction::Input}, {"in_r", Direction::Input}, {"out", Direction::Output}, {"out_r", Direction::Output}},
         {{"decay", "Decay", "s", ParameterKind::Continuous, 0.1, 15, 3.2, true},
          {"mix", "Mix", "%", ParameterKind::Continuous, 0, 100, 25},
          {"predelay", "Pre-delay", "ms", ParameterKind::Integer, 0, 200, 20}}},
        {"urn:peepo:demo:mono", "Mono utility", "Utility", "Mono routing fixture / no audio", 0xa6c8ec,
         {{"in", Direction::Input, 1}, {"out", Direction::Output, 1}},
         {{"gain", "Gain", "dB", ParameterKind::Continuous, -24, 12, 0}}},
        {"urn:peepo:demo:nam", "NAM fixture", "Amp", "Untested catalogue example", 0xf0aa72,
         {{"in", Direction::Input, 1}, {"out", Direction::Output, 1}}, {}, Availability::Untested,
         "Simulator fixture: compatibility has not been established."},
        {"urn:peepo:demo:cabinet", "Cabinet fixture", "Cabinet", "Unavailable catalogue example", 0x93a4b5,
         {{"in", Direction::Input, 1}, {"out", Direction::Output, 1}}, {}, Availability::Unavailable,
         "Simulator fixture: required cabinet asset is unavailable."}
    };
    return catalogue;
}
namespace {
// A deque: registering an entry must not move the ones already handed out.
std::deque<ModuleDefinition>& entries() {
    static std::deque<ModuleDefinition> merged = [] {
        std::deque<ModuleDefinition> all(demo_catalogue().begin(), demo_catalogue().end());
        const auto& reference = reference_catalogue();
        all.insert(all.end(), reference.begin(), reference.end());
        return all;
    }();
    return merged;
}
}
const std::deque<ModuleDefinition>& catalogue() { return entries(); }
void register_definitions(std::vector<ModuleDefinition> added) {
    for (auto& d : added) if (!definition(d.uri)) entries().push_back(std::move(d));
}
void replace_catalogue(std::vector<ModuleDefinition> offered) {
    std::erase_if(entries(), [](const ModuleDefinition& d) { return d.category != "Endpoint"; });
    register_definitions(std::move(offered));
}
const ModuleDefinition* definition(const std::string& uri) {
    for (const auto& d : catalogue()) if (d.uri == uri) return &d;
    return nullptr;
}
Parameter general_output_parameter() {
    return {general_output_symbol, "Output", "dB", ParameterKind::Continuous, -24, 12, 0, false, {}};
}
const Module* find_module(const Graph& g, InstanceId id) {
    for (const auto& m : g.modules) if (m.id == id) return &m;
    return nullptr;
}
Module* find_module(Graph& g, InstanceId id) {
    for (auto& m : g.modules) if (m.id == id) return &m;
    return nullptr;
}
Graph demo_graph() {
    Graph g;
    const int xs[] = {32, 664, 184, 352, 520};
    const int ys[] = {132, 132, 132, 132, 132};
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& d = demo_catalogue()[i];
        g.modules.push_back({i + 1, d.uri, d.name, xs[i], ys[i], i < 2, false, d.parameters, d.ports});
    }
    // Left lane first, then the right lane: every cable is one channel.
    g.connections = {{1, 3}, {3, 4}, {4, 5}, {5, 2}};
    for (std::size_t i = 0; i < 4; ++i) g.connections.push_back({g.connections[i].from, g.connections[i].to, "out_r", "in_r"});
    return g;
}
double tempo_value(const Parameter& p, double tempo) {
    const auto value = std::clamp(tempo, p.minimum, p.maximum);
    return p.kind == ParameterKind::Continuous ? value : std::round(value);
}
const std::vector<NoteValue>& note_values() {
    static const std::vector<NoteValue> notes = {
        {"1/32", 0.125}, {"1/16 trip", 1.0 / 6}, {"1/16", 0.25}, {"1/8 trip", 1.0 / 3}, {"1/16 dot", 0.375}, {"1/8", 0.5},
        {"1/4 trip", 2.0 / 3}, {"1/8 dot", 0.75}, {"1/4", 1}, {"1/2 trip", 4.0 / 3}, {"1/4 dot", 1.5}, {"1/2", 2},
        {"1/2 dot", 3}, {"1 bar", 4}, {"2 bars", 8}, {"4 bars", 16}};
    return notes;
}
std::string note_name(double beats) {
    for (const auto& note : note_values()) if (std::abs(beats - note.beats) < 0.0005) return note.name;
    return {};
}
double step_note(double beats, int steps, double minimum, double maximum) {
    // Between two notes, the first step lands on the nearer one in its direction.
    std::vector<double> usable;
    for (const auto& note : note_values()) if (note.beats >= minimum && note.beats <= maximum) usable.push_back(note.beats);
    if (usable.empty() || !steps) return beats;
    const int last = static_cast<int>(usable.size()) - 1;
    int at = 0;
    if (steps > 0) {
        while (at <= last && usable[static_cast<std::size_t>(at)] <= beats + 0.0005) ++at;
        at += steps - 1;
    } else {
        at = last;
        while (at >= 0 && usable[static_cast<std::size_t>(at)] >= beats - 0.0005) --at;
        at += steps + 1;
    }
    return usable[static_cast<std::size_t>(std::clamp(at, 0, last))];
}
Graph empty_graph(std::string name) {
    auto g = demo_graph(); g.name = std::move(name); g.modules.resize(2); g.connections = {{1, 2}, {1, 2, "out_r", "in_r"}}; g.dirty = true;
    return g;
}
Graph parallel_demo_graph() {
    auto g = demo_graph();
    g.name = "Parallel texture"; g.dirty = true;
    const int xs[] = {24, 672, 192, 408, 408};
    const int ys[] = {132, 132, 132, 24, 240};
    for (std::size_t i = 0; i < g.modules.size(); ++i) {
        g.modules[i].x = xs[i]; g.modules[i].y = ys[i];
    }
    g.connections = {{1, 3}, {3, 4}, {3, 5}, {4, 2}, {5, 2}};
    for (std::size_t i = 0; i < 5; ++i) g.connections.push_back({g.connections[i].from, g.connections[i].to, "out_r", "in_r"});
    return g;
}
Graph missing_module_graph() {
    auto g = demo_graph();
    auto* m = find_module(g, 4); m->uri = "urn:peepo:missing:texture"; m->name = "Missing texture";
    g.name = "Missing module example";
    return g;
}
const std::vector<Port>& module_ports(const Module& m) {
    if (!m.ports.empty()) return m.ports;
    if (const auto* d = definition(m.uri)) return d->ports;
    static const std::vector<Port> empty; return empty;
}
int card_height(const std::vector<Port>& ports) {
    unsigned inputs = 0, outputs = 0;
    for (const auto& p : ports)
        if (p.type == PortType::Audio || p.type == PortType::Midi) (p.direction == Direction::Input ? inputs : outputs) += p.channels;
    const int most = static_cast<int>(std::max(inputs, outputs));
    return most <= 2 ? module_height : most * connector_pitch + 8;
}
int module_height_of(const Module& m) { return card_height(module_ports(m)); }
std::string numbered_name(const Graph& g, const std::string& name) {
    std::set<std::string> taken;
    for (const auto& m : g.modules) taken.insert(m.name);
    if (!taken.contains(name)) return name;
    for (int n = 2;; ++n)
        if (auto candidate = name + " " + std::to_string(n); !taken.contains(candidate)) return candidate;
}
bool module_available(const Module& m) {
    const auto* d = definition(m.uri);
    return d && d->availability == Availability::Available;
}
std::string validate_patch_name(const std::string& name) {
    if (name.empty() || name.find_first_not_of(' ') == std::string::npos) return "Enter a patch name.";
    if (name.size() > 96) return "Patch name is too long (maximum 96 UTF-8 bytes).";
    for (unsigned char c : name) if (c < 32 || c == 127) return "Patch name contains a control character.";
    return {};
}
bool valid_control_id(const std::string& id) {
    return !id.empty() && id.size() <= 32 && std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}
const Module* assignment_target(const Graph& g, const Assignment& a) {
    const auto* m = find_module(g, a.module);
    if (!m || m->fixed || !module_available(*m) || (!a.plugin.empty() && a.plugin != m->uri)) return nullptr;
    if (a.action == ControlAction::Parameter &&
        std::none_of(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == a.parameter; })) return nullptr;
    if (a.action == ControlAction::Looper && (!looper_uri(m->uri) || !looper_assignment_command(a))) return nullptr;
    return m;
}
std::optional<int> looper_assignment_command(const Assignment& a) {
    int command = -1;
    const auto end = a.parameter.data() + a.parameter.size();
    const auto parsed = std::from_chars(a.parameter.data(), end, command);
    if (a.action != ControlAction::Looper || parsed.ec != std::errc{} || parsed.ptr != end || !looper_command_supported(command) ||
        a.minimum != a.maximum || a.minimum != std::round(a.minimum) || a.minimum < 0 || a.minimum > 9) return std::nullopt;
    return command;
}
const Parameter* assignment_parameter(const Graph& g, const Assignment& a) {
    const auto* m = a.action == ControlAction::Parameter ? assignment_target(g, a) : nullptr;
    if (!m) return nullptr;
    return &*std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == a.parameter; });
}
namespace {
std::string number_text(double value) {
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
    return std::string(buffer, result.ptr);
}
}
std::string encode_assignment(const Assignment& a, const std::string& block) {
    // action|block|plugin|parameter|minimum|maximum|inverted|mode. No field can
    // hold a '|': blocks and parameters are LV2 symbols, plugins safe IRIs.
    return std::string(a.action == ControlAction::Bypass ? "bypass" : a.action == ControlAction::Looper ? "looper" : "parameter") + "|" + block + "|" + a.plugin + "|" + a.parameter + "|" +
           number_text(a.minimum) + "|" + number_text(a.maximum) + "|" + (a.inverted ? "1" : "0") + "|" + (a.mode == SwitchMode::Momentary ? "momentary" : "toggle");
}
bool decode_assignment(const std::string& control, const std::string& text, Assignment& a, std::string& block) {
    std::vector<std::string> fields;
    for (std::size_t start = 0;;) {
        const auto bar = text.find('|', start);
        fields.push_back(text.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
        if (bar == std::string::npos) break;
        start = bar + 1;
    }
    if (fields.size() != 8 || !valid_control_id(control) || fields[1].empty()) return false;
    auto read = [](const std::string& field, double& out) {
        const auto result = std::from_chars(field.data(), field.data() + field.size(), out);
        return result.ec == std::errc{} && result.ptr == field.data() + field.size() && std::isfinite(out);
    };
    a = {};
    a.control = control;
    if (fields[0] == "bypass") a.action = ControlAction::Bypass;
    else if (fields[0] == "looper" && !fields[3].empty()) a.action = ControlAction::Looper;
    else if (fields[0] != "parameter" || fields[3].empty()) return false;
    block = fields[1];
    a.plugin = fields[2];
    a.parameter = fields[3];
    if (!read(fields[4], a.minimum) || !read(fields[5], a.maximum) || a.minimum > a.maximum) return false;
    a.inverted = fields[6] == "1";
    a.mode = fields[7] == "momentary" ? SwitchMode::Momentary : SwitchMode::Toggle;
    return true;
}
std::vector<std::string> assignment_parts(const std::string& text) {
    std::vector<std::string> parts;
    for (std::size_t start = 0; start < text.size();) {
        const auto space = text.find(' ', start);
        const auto end = space == std::string::npos ? text.size() : space;
        if (end > start) parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}
std::vector<Assignment> assignments_after(const Graph& g, const Assignment& a, bool add) {
    std::vector<Assignment> result;
    if (add && a.action == ControlAction::Bypass)
        for (const auto& old : g.assignments)
            if (old.control == a.control && old.action == ControlAction::Bypass && old.module != a.module) result.push_back(old);
    result.push_back(a);
    return result;
}
std::vector<const Assignment*> bypass_assignments(const Graph& g, const std::string& control) {
    std::vector<const Assignment*> found;
    for (const auto& a : g.assignments) if (a.control == control && a.action == ControlAction::Bypass) found.push_back(&a);
    return found;
}
bool valid_patch_id(const std::string& id) {
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}
std::string validate_parameter(const Parameter& p) {
    if (p.symbol.empty() || !std::isfinite(p.minimum) || !std::isfinite(p.maximum) || !std::isfinite(p.value) ||
        p.minimum > p.maximum || p.value < p.minimum || p.value > p.maximum) return "Invalid parameter range or value";
    if (p.logarithmic && (p.minimum <= 0 || p.maximum <= p.minimum)) return "Invalid logarithmic range";
    if (p.kind != ParameterKind::Continuous && (p.value != std::round(p.value) || p.minimum != std::round(p.minimum) ||
        p.maximum != std::round(p.maximum) || std::abs(p.minimum) > 9007199254740991.0 || std::abs(p.maximum) > 9007199254740991.0))
        return "Parameter requires exact discrete values";
    if (p.kind == ParameterKind::Toggle && (p.minimum != 0 || p.maximum != 1)) return "Invalid toggle range";
    if (p.kind == ParameterKind::Enumeration && (p.choices.empty() || p.minimum != 0 || p.maximum != p.choices.size() - 1))
        return "Invalid enumeration choices";
    return {};
}
std::string validate_connection(const Graph& g, const Connection& c, bool retain_unavailable) {
    const auto* from = find_module(g, c.from);
    const auto* to = find_module(g, c.to);
    if (!from || !to) return "Module is missing";
    if (c.from == c.to) return "Feedback loops are disabled";
    if (!retain_unavailable && (!module_available(*from) || !module_available(*to))) return "Module is unavailable; cable is inactive";
    auto port = [](const Module& m, const std::string& symbol, Direction dir) -> const Port* {
        for (const auto& p : module_ports(m)) if (p.symbol == symbol && p.direction == dir) return &p;
        return nullptr;
    };
    const auto* a = port(*from, c.output, Direction::Output); const auto* b = port(*to, c.input, Direction::Input);
    if (!a || !b) return "Connect an output to an input";
    // Audio joins audio, control joins control and MIDI joins MIDI. The engine
    // would also mix audio and control; the interface keeps them apart until
    // that is a deliberate feature.
    if (a->type != b->type) return "Audio, control and MIDI connectors cannot be joined to each other";
    if (a->type != PortType::Audio && a->type != PortType::Cv && a->type != PortType::Midi) return "Only audio, control and MIDI cables are supported";
    if (a->channels < 1 || a->channels > 2 || b->channels < 1 || b->channels > 2) return "Only mono and stereo ports are supported";
    for (const auto* p : {a, b}) if (!p->channel_symbols.empty()) {
        if (p->channel_symbols.size() != p->channels) return "Channel symbols do not match channel count";
        std::unordered_set<std::string> symbols;
        for (const auto& s : p->channel_symbols) if (s.empty() || !symbols.insert(s).second) return "Invalid channel symbol";
    }
    if (std::find(g.connections.begin(), g.connections.end(), c) != g.connections.end())
        return "Cable already exists";
    std::unordered_set<InstanceId> seen;
    std::vector<InstanceId> todo{c.to};
    while (!todo.empty()) {
        const auto id = todo.back(); todo.pop_back();
        if (id == c.from) return "Feedback loops are disabled";
        if (!seen.insert(id).second) continue;
        for (const auto& edge : g.connections) if (edge.from == id) todo.push_back(edge.to);
    }
    return {};
}
std::string validate_graph(const Graph& g) {
    if (auto issue = validate_patch_name(g.name); !issue.empty()) return issue;
    std::unordered_set<InstanceId> ids;
    unsigned inputs = 0, outputs = 0;
    for (const auto& m : g.modules) {
        if (!m.id || m.id == std::numeric_limits<InstanceId>::max() || !ids.insert(m.id).second || m.uri.empty()) return "Invalid module identity";
        const auto* d = definition(m.uri);
        const bool endpoint = d && d->category == "Endpoint";
        if (m.fixed != endpoint || (m.fixed && m.bypassed)) return "Invalid fixed endpoint";
        if (m.uri == "urn:peepo:demo:input") ++inputs;
        if (m.uri == "urn:peepo:demo:output") ++outputs;
        if (m.x < 0 || m.x > canvas_width - module_width || m.y < 0 || m.y > canvas_height - module_height) return "Module outside canvas";
        std::unordered_set<std::string> symbols;
        for (const auto& p : module_ports(m)) {
            if (p.symbol.empty() || !symbols.insert(p.symbol).second || p.channels < 1 || p.channels > 2) return "Invalid port metadata";
            if (!p.channel_symbols.empty()) {
                if (p.channel_symbols.size() != p.channels) return "Channel symbols do not match channel count";
                std::unordered_set<std::string> channels;
                for (const auto& s : p.channel_symbols) if (s.empty() || !channels.insert(s).second) return "Invalid channel symbol";
            }
        }
        symbols.clear();
        for (const auto& p : m.parameters) {
            if (!symbols.insert(p.symbol).second) return "Duplicate parameter symbol";
            if (auto issue = validate_parameter(p); !issue.empty()) return issue;
        }
    }
    if (inputs != 1 || outputs != 1) return "Patch must contain one IN and one OUT endpoint";
    Graph checked = g; checked.connections.clear();
    for (const auto& c : g.connections) {
        if (auto issue = validate_connection(checked, c, true); !issue.empty()) return issue;
        checked.connections.push_back(c);
    }
    // One assignment per control, except a control switching the bypass of
    // several modules: each module once, and nothing else on that control.
    for (const auto& a : g.assignments) {
        const auto shared = std::count_if(g.assignments.begin(), g.assignments.end(), [&](const Assignment& b) { return b.control == a.control; });
        const bool bypasses = std::all_of(g.assignments.begin(), g.assignments.end(), [&](const Assignment& b) {
            return b.control != a.control || (b.action == ControlAction::Bypass && (&b == &a || b.module != a.module));
        });
        if (!valid_control_id(a.control) || (shared > 1 && !bypasses) || !std::isfinite(a.minimum) || !std::isfinite(a.maximum) || a.minimum > a.maximum)
            return "Invalid control assignment";
        // An orphan is kept for repair; it is never applied, so its range is not checked.
        if (const auto* p = assignment_parameter(g, a); p && (a.minimum < p->minimum || a.maximum > p->maximum)) return "Invalid assignment parameter range";
    }
    return validate_scenes(g);
}
RoutingPlan expand_connection(const Graph& g, const Connection& c) {
    // Existing cables may be expanded; ignore just this edge for duplicate checking.
    auto probe = g; std::erase(probe.connections, c);
    if (auto issue = validate_connection(probe, c); !issue.empty()) return {{}, issue};
    const auto& a = module_ports(*find_module(g, c.from)); const auto& b = module_ports(*find_module(g, c.to));
    const auto& output = *std::find_if(a.begin(), a.end(), [&](const Port& p) { return p.symbol == c.output; });
    const auto& input = *std::find_if(b.begin(), b.end(), [&](const Port& p) { return p.symbol == c.input; });
    auto symbol = [](const Port& p, unsigned channel) {
        if (!p.channel_symbols.empty()) return p.channel_symbols[channel];
        return p.channels == 1 ? p.symbol : p.symbol + (channel == 0 ? "_L" : "_R");
    };
    RoutingPlan result;
    const unsigned count = std::max(output.channels, input.channels);
    for (unsigned i = 0; i < count; ++i)
        result.connections.push_back({c.from, c.to, symbol(output, output.channels == 1 ? 0 : i), symbol(input, input.channels == 1 ? 0 : i)});
    return result;
}
double normalized_value(const Parameter& p) {
    if (p.maximum <= p.minimum) return 0;
    const auto v = std::clamp(p.value, p.minimum, p.maximum);
    if (p.logarithmic && p.minimum > 0)
        return std::log(v / p.minimum) / std::log(p.maximum / p.minimum);
    return (v - p.minimum) / (p.maximum - p.minimum);
}
double value_from_normalized(const Parameter& p, double n) {
    n = std::clamp(n, 0.0, 1.0);
    auto value = p.logarithmic && p.minimum > 0
        ? p.minimum * std::pow(p.maximum / p.minimum, n)
        : p.minimum + n * (p.maximum - p.minimum);
    if (p.kind != ParameterKind::Continuous) value = std::round(value);
    return std::clamp(value, p.minimum, p.maximum);
}
std::string format_value(const Parameter& p) {
    if (p.kind == ParameterKind::Toggle) return p.value >= 0.5 ? "On" : "Off";
    if (p.kind == ParameterKind::Enumeration && p.value >= 0 && p.value < p.choices.size())
        return p.choices[static_cast<std::size_t>(p.value)];
    if (p.unit == beats_unit) if (auto note = note_name(p.value); !note.empty()) return note;
    std::ostringstream out;
    // Narrow ranges, such as 0-1 plugin controls, need decimals to show movement.
    const int digits = p.unit == "s" ? 1 : p.unit == beats_unit || (p.kind == ParameterKind::Continuous && p.maximum - p.minimum <= 10) ? 2 : 0;
    out << std::fixed << std::setprecision(digits) << p.value;
    if (!p.unit.empty()) out << " " << p.unit;
    return out.str();
}
std::string encode_control_changes(const std::vector<ControlChange>& messages) {
    std::string text;
    for (const auto& m : messages)
        text += (text.empty() ? "" : " ") + std::to_string(m.channel) + ":" + std::to_string(m.number) + ":" + std::to_string(m.value);
    return text;
}
std::optional<std::vector<ControlChange>> decode_control_changes(const std::string& text) {
    std::vector<ControlChange> out;
    std::istringstream in(text);
    for (std::string part; in >> part;) {
        ControlChange m;
        char a = 0, b = 0;
        std::istringstream fields(part);
        if (!(fields >> m.channel >> a >> m.number >> b >> m.value) || a != ':' || b != ':' || fields.peek() != EOF) return std::nullopt;
        if (m.channel < 1 || m.channel > 16 || m.number < 0 || m.number > 127 || m.value < 0 || m.value > 127) return std::nullopt;
        out.push_back(m);
    }
    if (out.size() > max_load_messages) return std::nullopt;
    return out;
}
std::string default_scene_name(std::size_t index) { return "Scene " + std::to_string(index + 1); }
std::string validate_scene_name(const std::string& name) {
    if (name.empty() || name.find_first_not_of(' ') == std::string::npos) return "Enter a scene name.";
    if (name.size() > max_scene_name) return "Scene name is too long (maximum 12 characters).";
    for (unsigned char c : name) if (c < 32 || c > 126) return "Scene names use plain letters, digits and punctuation.";
    return {};
}
namespace {
std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> fields;
    for (std::size_t start = 0;;) {
        const auto at = text.find(separator, start);
        fields.push_back(text.substr(start, at == std::string::npos ? std::string::npos : at - start));
        if (at == std::string::npos) return fields;
        start = at + 1;
    }
}
bool read_number(const std::string& field, double& out) {
    const auto result = std::from_chars(field.data(), field.data() + field.size(), out);
    return !field.empty() && result.ec == std::errc{} && result.ptr == field.data() + field.size() && std::isfinite(out);
}
// No field may hold an entry or field separator.
bool scene_field(const std::string& field) { return field.find_first_of(" |") == std::string::npos; }
const Parameter* scene_parameter(const Graph& g, const SceneTarget& t) {
    const auto* m = scene_target(g, t);
    if (!m || t.symbol.empty()) return nullptr;
    return &*std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == t.symbol; });
}
bool marked(const Scenes& s, const SceneTarget& t) {
    return std::any_of(s.parameters.begin(), s.parameters.end(), [&](const SceneTarget& p) { return p.same(t); });
}
SceneTarget live_target(const Module& m, const std::string& symbol) { return {m.id, {}, m.uri, symbol}; }
// Writes the entry's value, adding the entry when the scene lacks it.
bool put_entry(Scene& scene, const SceneEntry& entry) {
    for (auto& e : scene.entries)
        if (e.target.same(entry.target)) {
            if (e.value == entry.value) return false;
            e.value = entry.value;
            return true;
        }
    scene.entries.push_back(entry);
    return true;
}
}
SceneProperties encode_scenes(const Scenes& s, const std::function<std::string(InstanceId)>& block_of) {
    SceneProperties out;
    out.count = static_cast<int>(s.scenes.size());
    out.current = s.scenes.empty() ? 0 : static_cast<int>(s.current) + 1;
    out.mode = !s.scenes.empty() && s.scene_mode ? 1 : 0;
    // A module the engine does not hold keeps the block it was read with.
    auto block = [&](const SceneTarget& t) {
        auto symbol = t.module ? block_of(t.module) : std::string();
        return symbol.empty() ? t.block : symbol;
    };
    auto join = [](std::string& text, const std::string& part) { text += (text.empty() ? "" : " ") + part; };
    for (const auto& p : s.parameters)
        if (const auto b = block(p); !b.empty()) join(out.parameters, b + "|" + p.plugin + "|" + p.symbol);
    for (const auto& scene : s.scenes) {
        std::string text;
        for (const auto& e : scene.entries) {
            const auto b = block(e.target);
            if (b.empty()) continue;
            if (e.target.symbol.empty()) join(text, "b|" + b + "|" + e.target.plugin + "|" + (e.value != 0 ? "1" : "0"));
            else join(text, "p|" + b + "|" + e.target.plugin + "|" + e.target.symbol + "|" + number_text(e.value));
        }
        out.scenes.emplace_back(scene.name, std::move(text));
    }
    return out;
}
Scenes decode_scenes(const SceneProperties& in, const std::function<InstanceId(const std::string&)>& id_of) {
    Scenes s;
    const auto count = std::min({static_cast<std::size_t>(std::max(in.count, 0)), max_scenes, in.scenes.size()});
    if (!count) return s;
    auto target = [&](const std::string& block, const std::string& plugin, const std::string& symbol) {
        return SceneTarget{id_of(block), block, plugin, symbol};
    };
    for (const auto& part : assignment_parts(in.parameters)) {
        const auto f = split(part, '|');
        if (f.size() != 3 || f[0].empty() || f[1].empty() || f[2].empty() || s.parameters.size() == max_scene_parameters) continue;
        auto t = target(f[0], f[1], f[2]);
        if (!marked(s, t)) s.parameters.push_back(std::move(t));
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto& [name, text] = in.scenes[i];
        Scene scene{validate_scene_name(name).empty() ? name : default_scene_name(i), {}};
        for (const auto& part : assignment_parts(text)) {
            const auto f = split(part, '|');
            if (f.size() < 4 || f[1].empty() || f[2].empty()) continue;
            SceneEntry e;
            if (f.size() == 4 && f[0] == "b" && (f[3] == "0" || f[3] == "1")) e = {target(f[1], f[2], {}), f[3] == "1" ? 1.0 : 0.0};
            else if (f.size() == 5 && f[0] == "p" && !f[3].empty() && read_number(f[4], e.value) && marked(s, target(f[1], f[2], f[3])))
                e.target = target(f[1], f[2], f[3]);
            else continue;
            if (std::none_of(scene.entries.begin(), scene.entries.end(), [&](const SceneEntry& o) { return o.target.same(e.target); }))
                scene.entries.push_back(std::move(e));
        }
        s.scenes.push_back(std::move(scene));
    }
    s.current = in.current >= 1 ? std::min(static_cast<std::size_t>(in.current) - 1, count - 1) : 0;
    s.scene_mode = in.mode == 1;
    return s;
}
const Module* scene_target(const Graph& g, const SceneTarget& t) {
    const auto* m = find_module(g, t.module);
    if (!m || m->fixed || !module_available(*m) || t.plugin != m->uri) return nullptr;
    if (!t.symbol.empty() && std::none_of(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == t.symbol; })) return nullptr;
    return m;
}
bool scene_parameter_marked(const Graph& g, InstanceId id, const std::string& symbol) {
    const auto* m = find_module(g, id);
    return m && !symbol.empty() && marked(g.scenes, live_target(*m, symbol));
}
std::vector<SceneChange> scene_changes(const Graph& g, std::size_t index) {
    std::vector<SceneChange> changes;
    if (index >= g.scenes.scenes.size()) return changes;
    for (const auto& e : g.scenes.scenes[index].entries) {
        const auto* m = scene_target(g, e.target);
        if (!m) continue;
        if (e.target.symbol.empty()) {
            if (m->bypassed != (e.value != 0)) changes.push_back({m->id, {}, e.value != 0 ? 1.0 : 0.0});
            continue;
        }
        if (!marked(g.scenes, e.target)) continue;
        const auto& p = *scene_parameter(g, e.target);
        auto value = std::clamp(e.value, p.minimum, p.maximum);
        if (p.kind != ParameterKind::Continuous) value = std::round(value);
        if (value != p.value) changes.push_back({m->id, p.symbol, value});
    }
    return changes;
}
Scene capture_scene(const Graph& g, std::string name) {
    Scene scene{std::move(name), {}};
    for (const auto& m : g.modules)
        if (!m.fixed) scene.entries.push_back({live_target(m, {}), m.bypassed ? 1.0 : 0.0});
    for (const auto& t : g.scenes.parameters)
        if (const auto* p = scene_parameter(g, t)) scene.entries.push_back({live_target(*scene_target(g, t), t.symbol), p->value});
    return scene;
}
std::string add_scene(Graph& g) {
    auto& s = g.scenes;
    if (s.scenes.size() >= max_scenes) return "A patch holds at most 8 scenes.";
    // The lowest default name no scene has.
    std::size_t number = 0;
    while (std::any_of(s.scenes.begin(), s.scenes.end(), [&](const Scene& scene) { return scene.name == default_scene_name(number); })) ++number;
    s.scenes.push_back(capture_scene(g, default_scene_name(number)));
    s.current = s.scenes.size() - 1;
    return {};
}
std::string rename_scene(Graph& g, std::size_t index, const std::string& name) {
    if (index >= g.scenes.scenes.size()) return "No such scene.";
    if (auto issue = validate_scene_name(name); !issue.empty()) return issue;
    g.scenes.scenes[index].name = name;
    return {};
}
std::string copy_scene(Graph& g, std::size_t from, std::size_t to) {
    auto& scenes = g.scenes.scenes;
    if (from >= scenes.size() || to >= scenes.size()) return "No such scene.";
    if (from != to) scenes[to].entries = scenes[from].entries;
    return {};
}
std::string move_scene(Graph& g, std::size_t from, std::size_t to) {
    auto& s = g.scenes;
    if (from >= s.scenes.size() || to >= s.scenes.size()) return "No such scene.";
    auto moved = std::move(s.scenes[from]);
    s.scenes.erase(s.scenes.begin() + static_cast<std::ptrdiff_t>(from));
    s.scenes.insert(s.scenes.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    if (s.current == from) s.current = to;
    else if (from < s.current && to >= s.current) --s.current;
    else if (from > s.current && to <= s.current) ++s.current;
    return {};
}
std::string delete_scene(Graph& g, std::size_t index) {
    auto& s = g.scenes;
    if (index >= s.scenes.size()) return "No such scene.";
    if (s.scenes.size() == 1) {
        s = {};
        return {};
    }
    if (index == s.current) return "Select another scene before deleting this one.";
    s.scenes.erase(s.scenes.begin() + static_cast<std::ptrdiff_t>(index));
    if (index < s.current) --s.current;
    return {};
}
std::string set_scene_mode(Graph& g, bool scene_mode) {
    if (scene_mode && g.scenes.scenes.empty()) return "Add a scene first.";
    g.scenes.scene_mode = scene_mode;
    return {};
}
std::string mark_scene_parameter(Graph& g, InstanceId id, const std::string& symbol) {
    auto& s = g.scenes;
    if (s.scenes.empty()) return "Add a scene first.";
    const auto* m = find_module(g, id);
    if (!m || m->fixed) return "No such module.";
    const auto t = live_target(*m, symbol);
    const auto* p = scene_parameter(g, t);
    if (!p) return "No such parameter.";
    if (marked(s, t)) return {};
    if (s.parameters.size() >= max_scene_parameters) return "At most 16 parameters change per scene.";
    s.parameters.push_back(t);
    for (auto& scene : s.scenes) put_entry(scene, {t, p->value});
    return {};
}
std::string unmark_scene_parameter(Graph& g, InstanceId id, const std::string& symbol) {
    auto& s = g.scenes;
    if (!scene_parameter_marked(g, id, symbol)) return "The parameter is not per scene.";
    const auto t = live_target(*find_module(g, id), symbol);
    std::erase_if(s.parameters, [&](const SceneTarget& p) { return p.same(t); });
    for (auto& scene : s.scenes) std::erase_if(scene.entries, [&](const SceneEntry& e) { return e.target.same(t); });
    return {};
}
bool record_scene_bypass(Graph& g, InstanceId id, bool bypassed) {
    auto& s = g.scenes;
    const auto* m = find_module(g, id);
    if (s.scenes.empty() || !m || m->fixed) return false;
    return put_entry(s.scenes[s.current], {live_target(*m, {}), bypassed ? 1.0 : 0.0});
}
bool record_scene_parameter(Graph& g, InstanceId id, const std::string& symbol, double value) {
    auto& s = g.scenes;
    if (s.scenes.empty() || !scene_parameter_marked(g, id, symbol)) return false;
    return put_entry(s.scenes[s.current], {live_target(*find_module(g, id), symbol), value});
}
void scenes_module_added(Graph& g, InstanceId id) {
    const auto* m = find_module(g, id);
    if (!m || m->fixed) return;
    for (auto& scene : g.scenes.scenes) put_entry(scene, {live_target(*m, {}), m->bypassed ? 1.0 : 0.0});
}
void scenes_module_duplicated(Graph& g, InstanceId original, InstanceId copy) {
    auto& s = g.scenes;
    const auto* from = find_module(g, original);
    const auto* to = find_module(g, copy);
    if (!from || !to || from->fixed || to->fixed || s.scenes.empty()) return;
    auto own = [&](const SceneTarget& t) { return t.module == original && t.plugin == from->uri; };
    auto moved = [&](SceneTarget t) { t.module = copy; t.block.clear(); t.plugin = to->uri; return t; };
    // The copy's parameters follow the scenes only if all of them fit.
    std::vector<SceneTarget> marks;
    for (const auto& p : s.parameters) if (own(p) && scene_parameter(g, moved(p))) marks.push_back(moved(p));
    const bool fit = s.parameters.size() + marks.size() <= max_scene_parameters;
    if (fit) s.parameters.insert(s.parameters.end(), marks.begin(), marks.end());
    for (auto& scene : s.scenes) {
        std::vector<SceneEntry> copied;
        for (const auto& e : scene.entries)
            if (own(e.target) && (e.target.symbol.empty() || (fit && marked(s, moved(e.target))))) copied.push_back({moved(e.target), e.value});
        for (const auto& e : copied) put_entry(scene, e);
    }
    // The copy's live values, in a scene that lacks the original's.
    for (auto& scene : s.scenes) {
        if (std::none_of(scene.entries.begin(), scene.entries.end(), [&](const SceneEntry& e) { return e.target.same(live_target(*to, {})); }))
            put_entry(scene, {live_target(*to, {}), to->bypassed ? 1.0 : 0.0});
        if (fit)
            for (const auto& t : marks)
                if (std::none_of(scene.entries.begin(), scene.entries.end(), [&](const SceneEntry& e) { return e.target.same(t); }))
                    put_entry(scene, {t, scene_parameter(g, t)->value});
    }
}
void scenes_module_removed(Graph& g, const Module& m) {
    auto& s = g.scenes;
    auto own = [&](const SceneTarget& t) { return t.module == m.id && t.plugin == m.uri; };
    std::erase_if(s.parameters, own);
    for (auto& scene : s.scenes) std::erase_if(scene.entries, [&](const SceneEntry& e) { return own(e.target); });
}
void fit_scenes(Graph& g) {
    for (auto& scene : g.scenes.scenes)
        for (auto& e : scene.entries)
            if (const auto* p = scene_parameter(g, e.target)) {
                e.value = std::clamp(e.value, p->minimum, p->maximum);
                if (p->kind != ParameterKind::Continuous) e.value = std::round(e.value);
            }
}
std::string validate_scenes(const Graph& g) {
    const auto& s = g.scenes;
    if (s.scenes.size() > max_scenes) return "Too many scenes";
    if (s.scenes.empty()) return !s.parameters.empty() ? "Parameters marked per scene without scenes" : s.scene_mode ? "Scene mode without scenes" : std::string();
    if (s.current >= s.scenes.size()) return "Invalid current scene";
    if (s.parameters.size() > max_scene_parameters) return "Too many parameters per scene";
    auto valid = [](const SceneTarget& t) {
        return (t.module || !t.block.empty()) && !t.plugin.empty() && scene_field(t.block) && scene_field(t.plugin) && scene_field(t.symbol);
    };
    for (std::size_t i = 0; i < s.parameters.size(); ++i) {
        const auto& p = s.parameters[i];
        if (!valid(p) || p.symbol.empty()) return "Invalid scene parameter";
        for (std::size_t j = 0; j < i; ++j) if (s.parameters[j].same(p)) return "Scene parameter marked twice";
    }
    for (const auto& scene : s.scenes) {
        if (!validate_scene_name(scene.name).empty()) return "Invalid scene name";
        for (std::size_t i = 0; i < scene.entries.size(); ++i) {
            const auto& e = scene.entries[i];
            if (!valid(e.target) || !std::isfinite(e.value)) return "Invalid scene entry";
            for (std::size_t j = 0; j < i; ++j) if (scene.entries[j].target.same(e.target)) return "Scene entry repeated";
            if (e.target.symbol.empty()) {
                if (e.value != 0 && e.value != 1) return "Invalid scene bypass";
                continue;
            }
            if (!marked(s, e.target)) return "Scene entry for a parameter not marked per scene";
            // An orphan is kept for repair; it is never applied, so its value is not checked.
            if (const auto* p = scene_parameter(g, e.target)) {
                auto probe = *p;
                probe.value = e.value;
                if (!validate_parameter(probe).empty()) return "Scene value out of range";
            }
        }
    }
    return {};
}
std::string midi_binding_text(const MidiBinding& b) {
    const auto channel = " / " + std::to_string(b.channel);
    switch (b.kind) {
    case MidiBinding::Kind::Controller: return "CC " + std::to_string(b.number) + channel;
    case MidiBinding::Kind::Note: return "NOTE " + std::to_string(b.number) + channel;
    case MidiBinding::Kind::Bender: return "BEND" + channel;
    case MidiBinding::Kind::Pressure: return "PRESS" + channel;
    }
    return {};
}
}
