// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "catalogue/catalogue.hpp"
#include "catalogue/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace peepo {
namespace {
using json::Value;
constexpr std::size_t max_file_bytes = std::size_t{16} << 20;
constexpr const char* kinds[] = {"continuous", "integer", "toggle", "enumeration"};
constexpr const char* types[] = {"audio", "control", "midi", "cv", "atom"};
constexpr const char* availabilities[] = {"available", "untested", "unavailable"};
template <std::size_t N> int index_of(const char* const (&names)[N], std::string_view name) {
    for (std::size_t i = 0; i < N; ++i) if (name == names[i]) return static_cast<int>(i);
    return -1;
}
std::string color_text(std::uint32_t color) {
    static const char digits[] = "0123456789abcdef";
    std::string text = "#";
    for (int shift = 20; shift >= 0; shift -= 4) text += digits[(color >> shift) & 15];
    return text;
}
bool parse_color(std::string_view text, std::uint32_t& color) {
    if (text.size() != 7 || text[0] != '#') return false;
    color = 0;
    for (const char c : text.substr(1)) {
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return false;
        color = color * 16 + static_cast<std::uint32_t>(digit);
    }
    return true;
}
bool plain(std::string_view text, std::size_t limit) {
    if (text.size() > limit) return false;
    for (const unsigned char c : text) if (c < 0x20 || c == 0x7f) return false;
    return true;
}
bool address(std::string_view uri) {
    if (uri.empty() || uri.size() > 1024 || uri.find(':') == std::string_view::npos) return false;
    for (const unsigned char c : uri) if (c <= 0x20 || c == 0x7f || c == '<' || c == '>' || c == '"' || c == '\\') return false;
    return true;
}
bool symbol(std::string_view text) {
    if (text.empty() || text.size() > 256 || (text[0] >= '0' && text[0] <= '9')) return false;
    for (const char c : text) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
// Typed access that names the missing or mistyped key.
struct Fields {
    const Value& object;
    std::string where;
    std::string issue;
    const Value* find(std::string_view key, Value::Kind kind, bool required) {
        const auto* value = object.get(key);
        if (!value) { if (required && issue.empty()) issue = where + ": \"" + std::string(key) + "\" is missing"; return nullptr; }
        if (value->kind != kind) { if (issue.empty()) issue = where + ": \"" + std::string(key) + "\" has the wrong type"; return nullptr; }
        return value;
    }
    std::string text(std::string_view key, bool required = true, std::size_t limit = 512) {
        const auto* value = find(key, Value::Kind::String, required);
        if (value && !plain(value->text, limit)) { if (issue.empty()) issue = where + ": \"" + std::string(key) + "\" is too long or has control characters"; return {}; }
        return value ? value->text : std::string();
    }
    double number(std::string_view key) { const auto* value = find(key, Value::Kind::Number, true); return value ? value->number : 0; }
    bool flag(std::string_view key) { const auto* value = find(key, Value::Kind::Boolean, false); return value && value->boolean; }
    template <std::size_t N> int choice(std::string_view key, const char* const (&names)[N]) {
        const auto* value = find(key, Value::Kind::String, true);
        if (!value) return 0;
        const int found = index_of(names, value->text);
        if (found < 0 && issue.empty()) issue = where + ": \"" + std::string(key) + "\" is \"" + value->text + "\", which is not known";
        return found < 0 ? 0 : found;
    }
};
std::string header(const Value& root, std::string_view format) {
    if (root.kind != Value::Kind::Object) return "not a JSON object";
    const auto* name = root.get("format");
    const auto* version = root.get("version");
    if (!name || name->kind != Value::Kind::String || name->text != format) return "not a " + std::string(format) + " file";
    if (!version || version->kind != Value::Kind::Number || version->number != std::floor(version->number) || version->number < 1) return "file has no usable version";
    if (version->number > catalogue_version) return "file was written by a newer version of the project";
    return {};
}
// Per-control presentation. Each entry names a control the module must have:
// it may rename it, give it a unit, show it as whole numbers or as an on/off
// switch, choose another default inside the plugin's range, or hide it.
// Listed controls come first, in the order listed; the rest keep theirs.
// Ranges stay the plugin's: the engine neither reports nor checks them.
std::string apply_parameter_rules(const Value& entries, const std::string& where, ModuleDefinition& d) {
    std::vector<Parameter> listed;
    std::set<std::string> hidden, seen;
    for (const auto& item : entries.items) {
        if (item.kind != Value::Kind::Object) return where + ": a parameter rule is not an object";
        Fields entry{item, where + " parameter", {}};
        const auto symbol = entry.text("symbol");
        entry.where += " " + symbol;
        if (!entry.issue.empty()) return entry.issue;
        if (!seen.insert(symbol).second) return entry.where + ": listed twice";
        const auto found = std::find_if(d.parameters.begin(), d.parameters.end(), [&](const Parameter& p) { return p.symbol == symbol; });
        if (found == d.parameters.end()) return entry.where + ": " + d.uri + " has no such control";
        if (entry.flag("hidden")) { hidden.insert(symbol); continue; }
        auto p = *found;
        if (const auto name = entry.text("name", false, 96); !name.empty()) p.name = name;
        if (item.get("unit")) p.unit = entry.text("unit", false, 16);
        if (item.get("kind")) {
            const auto kind = static_cast<ParameterKind>(entry.choice("kind", kinds));
            if (kind == ParameterKind::Enumeration) return entry.where + ": name the choices instead";
            p.kind = kind;
        }
        // Names for the values of a whole-number control that counts from 0,
        // which the plugin gives only as numbers: shown as a list of choices.
        if (const auto* choices = entry.find("choices", Value::Kind::Array, false)) {
            if (p.kind != ParameterKind::Integer || p.minimum != 0 || p.maximum + 1 != static_cast<double>(choices->items.size()))
                return entry.where + ": choices need a whole-number control from 0 with one name per value";
            p.choices.clear();
            for (const auto& choice : choices->items) {
                if (choice.kind != Value::Kind::String || choice.text.empty() || !plain(choice.text, 48)) return entry.where + ": a choice is not a short name";
                p.choices.push_back(choice.text);
            }
            p.kind = ParameterKind::Enumeration;
        }
        if (item.get("logarithmic")) p.logarithmic = entry.flag("logarithmic");
        const bool has_default = item.get("default") != nullptr;
        if (has_default) p.value = entry.number("default");
        if (!entry.issue.empty()) return entry.issue;
        if (auto issue = validate_parameter(p); !issue.empty()) return entry.where + ": " + issue;
        if (has_default && std::find(d.project_defaults.begin(), d.project_defaults.end(), symbol) == d.project_defaults.end()) d.project_defaults.push_back(symbol);
        listed.push_back(std::move(p));
    }
    std::vector<Parameter> ordered = listed;
    for (const auto& p : d.parameters)
        if (!seen.contains(p.symbol)) ordered.push_back(p);
    std::erase_if(d.project_defaults, [&](const std::string& symbol) { return hidden.contains(symbol); });
    d.parameters = std::move(ordered);
    return {};
}
std::string read_file(const std::string& path, std::string& out) {
    // The path is UTF-8; a narrow open would misread it on Windows.
    std::ifstream file(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary);
    if (!file) return "cannot open " + path;
    std::ostringstream text;
    text << file.rdbuf();
    out = text.str();
    if (out.size() > max_file_bytes) return path + " is too large";
    if (out.starts_with("\xef\xbb\xbf")) out.erase(0, 3);
    return {};
}
}
std::uint32_t category_color(std::string_view category) {
    static const std::pair<std::string_view, std::uint32_t> colors[] = {
        {"Drive", 0xf0aa72}, {"Amp", 0xf0aa72}, {"Cabinet", 0xf0aa72}, {"Modulation", 0xf0aa72}, {"Texture", 0xbfa1ed}, {"Pitch", 0xbfa1ed},
        {"Delay", 0x7bbaed}, {"Sound source", 0x7bbaed}, {"Reverb", 0x81c9b8}, {"Resonator", 0x81c9b8},
        {"Filter", 0xa6c8ec}, {"Dynamics", 0xa6c8ec}, {"Control", 0xe3c770}, {"Utility", 0x93a4b5}};
    for (const auto& [name, color] : colors) if (name == category) return color;
    return 0x93a4b5;
}
std::string write_catalogue(const CatalogueDocument& document) {
    std::string out = "{\n  \"format\": \"peepo-catalogue\",\n  \"version\": " + std::to_string(catalogue_version) + ",\n  \"provenance\": {";
    for (std::size_t i = 0; i < document.provenance.size(); ++i)
        out += std::string(i ? "," : "") + "\n    " + json::quote(document.provenance[i].first) + ": " + json::quote(document.provenance[i].second);
    out += document.provenance.empty() ? "},\n  \"modules\": [" : "\n  },\n  \"modules\": [";
    for (std::size_t i = 0; i < document.modules.size(); ++i) {
        const auto& d = document.modules[i];
        const auto origin = document.origins.find(d.uri);
        out += std::string(i ? "," : "") + "\n    {\n      \"uri\": " + json::quote(d.uri) + ",\n      \"name\": " + json::quote(d.name) +
               ",\n      \"category\": " + json::quote(d.category) + ",\n      \"description\": " + json::quote(d.description) +
               ",\n      \"color\": \"" + color_text(d.color) + "\",\n      \"availability\": \"" + availabilities[static_cast<int>(d.availability)] +
               "\",\n      \"reason\": " + json::quote(d.availability_reason) +
               ",\n      \"origin\": " + json::quote(origin == document.origins.end() ? std::string() : origin->second) +
               ",\n      \"enable\": " + json::quote(d.enable_symbol) + ",\n      \"ports\": [";
        for (std::size_t p = 0; p < d.ports.size(); ++p)
            out += std::string(p ? "," : "") + "\n        {\"symbol\": " + json::quote(d.ports[p].symbol) + ", \"direction\": \"" +
                   (d.ports[p].direction == Direction::Input ? "input" : "output") + "\", \"type\": \"" + types[static_cast<int>(d.ports[p].type)] + "\"}";
        out += d.ports.empty() ? "],\n      \"parameters\": [" : "\n      ],\n      \"parameters\": [";
        for (std::size_t p = 0; p < d.parameters.size(); ++p) {
            const auto& parameter = d.parameters[p];
            out += std::string(p ? "," : "") + "\n        {\"symbol\": " + json::quote(parameter.symbol) + ", \"name\": " + json::quote(parameter.name) +
                   ", \"unit\": " + json::quote(parameter.unit) + ", \"kind\": \"" + kinds[static_cast<int>(parameter.kind)] +
                   "\", \"minimum\": " + json::number(parameter.minimum) + ", \"maximum\": " + json::number(parameter.maximum) +
                   ", \"default\": " + json::number(parameter.value) + ", \"logarithmic\": " + (parameter.logarithmic ? "true" : "false") + ", \"choices\": [";
            for (std::size_t c = 0; c < parameter.choices.size(); ++c) out += std::string(c ? ", " : "") + json::quote(parameter.choices[c]);
            out += "]}";
        }
        out += d.parameters.empty() ? "]\n    }" : "\n      ]\n    }";
    }
    return out + (document.modules.empty() ? "]\n}\n" : "\n  ]\n}\n");
}
std::string read_catalogue(std::string_view text, CatalogueDocument& out) {
    Value root;
    if (auto issue = json::parse(text, root); !issue.empty()) return "catalogue is not valid JSON: " + issue;
    if (auto issue = header(root, "peepo-catalogue"); !issue.empty()) return issue;
    // Version 1 called every atom port "midi", file inputs included. Which of
    // them take MIDI is unknown, so none becomes a MIDI connector.
    const bool atoms_unsorted = root.get("version")->number < 2;
    CatalogueDocument document;
    if (const auto* provenance = root.get("provenance"); provenance && provenance->kind == Value::Kind::Object)
        for (const auto& [key, value] : provenance->members) if (value.kind == Value::Kind::String) document.provenance.emplace_back(key, value.text);
    const auto* modules = root.get("modules");
    if (!modules || modules->kind != Value::Kind::Array) return "catalogue has no module list";
    std::set<std::string> seen;
    for (const auto& entry : modules->items) {
        if (entry.kind != Value::Kind::Object) return "a module entry is not an object";
        Fields fields{entry, "module " + std::to_string(document.modules.size() + 1), {}};
        ModuleDefinition d{};
        d.uri = fields.text("uri", true, 1024);
        if (!fields.issue.empty()) return fields.issue;
        fields.where = "module " + d.uri;
        if (!address(d.uri)) return fields.where + ": not a usable plugin address";
        if (!seen.insert(d.uri).second) return fields.where + ": listed twice";
        d.name = fields.text("name", true, 96);
        d.category = fields.text("category", true, 48);
        d.description = fields.text("description", false);
        d.availability = static_cast<Availability>(fields.choice("availability", availabilities));
        d.availability_reason = fields.text("reason", false);
        // Read from LV2 metadata, as opposed to an invented demo fixture.
        d.reference = true;
        const auto color = fields.text("color", false);
        if (color.empty()) d.color = category_color(d.category);
        else if (!parse_color(color, d.color)) return fields.where + ": \"color\" must look like #a6c8ec";
        if (const auto origin = fields.text("origin", false, 2048); !origin.empty()) document.origins[d.uri] = origin;
        d.enable_symbol = fields.text("enable", false, 256);
        const auto* ports = fields.find("ports", Value::Kind::Array, true);
        const auto* parameters = fields.find("parameters", Value::Kind::Array, true);
        if (!fields.issue.empty()) return fields.issue;
        if (d.name.empty() || d.category.empty() || d.category == "Endpoint") return fields.where + ": needs a name and a category of its own";
        std::set<std::string> symbols;
        if (!d.enable_symbol.empty() && (!symbol(d.enable_symbol) || !symbols.insert(d.enable_symbol).second)) return fields.where + ": \"enable\" is not a usable symbol";
        for (const auto& item : ports->items) {
            if (item.kind != Value::Kind::Object) return fields.where + ": a port is not an object";
            Fields port{item, fields.where + " port", {}};
            Port made{port.text("symbol"), {}, 1, {}, {}};
            port.where += " " + made.symbol;
            const auto* direction = port.find("direction", Value::Kind::String, true);
            if (direction && direction->text != "input" && direction->text != "output") return port.where + ": direction must be input or output";
            made.direction = direction && direction->text == "input" ? Direction::Input : Direction::Output;
            made.type = static_cast<PortType>(port.choice("type", types));
            if (atoms_unsorted && made.type == PortType::Midi) made.type = PortType::Atom;
            if (!port.issue.empty()) return port.issue;
            if (!symbol(made.symbol) || !symbols.insert(made.symbol).second) return port.where + ": symbol is invalid or repeated";
            d.ports.push_back(std::move(made));
        }
        for (const auto& item : parameters->items) {
            if (item.kind != Value::Kind::Object) return fields.where + ": a parameter is not an object";
            Fields parameter{item, fields.where + " parameter", {}};
            Parameter made;
            made.symbol = parameter.text("symbol");
            parameter.where += " " + made.symbol;
            made.name = parameter.text("name", true, 96);
            made.unit = parameter.text("unit", false, 16);
            made.kind = static_cast<ParameterKind>(parameter.choice("kind", kinds));
            made.minimum = parameter.number("minimum");
            made.maximum = parameter.number("maximum");
            made.value = parameter.number("default");
            made.logarithmic = parameter.flag("logarithmic");
            if (const auto* choices = parameter.find("choices", Value::Kind::Array, false))
                for (const auto& choice : choices->items) {
                    if (choice.kind != Value::Kind::String || !plain(choice.text, 96)) return parameter.where + ": a choice is not plain text";
                    made.choices.push_back(choice.text);
                }
            if (!parameter.issue.empty()) return parameter.issue;
            // A port and a parameter are both ports of the plugin: one namespace.
            if (!symbol(made.symbol) || !symbols.insert(made.symbol).second) return parameter.where + ": symbol is invalid or repeated";
            if (made.name.empty()) return parameter.where + ": needs a name";
            if (auto issue = validate_parameter(made); !issue.empty()) return parameter.where + ": " + issue;
            d.parameters.push_back(std::move(made));
        }
        document.modules.push_back(std::move(d));
    }
    out = std::move(document);
    return {};
}
std::string apply_catalogue_overlay(std::string_view text, std::vector<ModuleDefinition>& modules) {
    Value root;
    if (auto issue = json::parse(text, root); !issue.empty()) return "overlay is not valid JSON: " + issue;
    if (auto issue = header(root, "peepo-catalogue-overlay"); !issue.empty()) return issue;
    const auto* rules = root.get("rules");
    if (!rules || rules->kind != Value::Kind::Array) return "overlay has no rule list";
    auto changed = modules;
    std::size_t number = 0;
    for (const auto& item : rules->items) {
        if (item.kind != Value::Kind::Object) return "an overlay rule is not an object";
        Fields rule{item, "overlay rule " + std::to_string(++number), {}};
        const auto uri = rule.text("uri", false, 1024), match = rule.text("match", false, 1024);
        if (uri.empty() == match.empty()) return rule.where + ": needs exactly one of \"uri\" and \"match\"";
        const auto name = rule.text("name", false, 96), category = rule.text("category", false, 48), description = rule.text("description", false);
        const auto color = rule.text("color", false), reason = rule.text("reason", false), prefix = rule.text("strip_prefix", false, 48);
        const auto output = rule.text("output", false, 256), tempo = rule.text("tempo", false, 256), bypass = rule.text("bypass", false, 16);
        const bool has_availability = item.get("availability") != nullptr;
        const bool has_hidden = item.get("hidden") != nullptr;
        const bool hidden = rule.flag("hidden");
        const auto availability = has_availability ? static_cast<Availability>(rule.choice("availability", availabilities)) : Availability::Available;
        std::uint32_t parsed = 0;
        if (!rule.issue.empty()) return rule.issue;
        if (!color.empty() && !parse_color(color, parsed)) return rule.where + ": \"color\" must look like #a6c8ec";
        if (category == "Endpoint") return rule.where + ": that category is reserved";
        if (!bypass.empty() && bypass != "engine" && bypass != "port" && bypass != "spill") return rule.where + ": \"bypass\" must be engine, port or spill";
        // A rule may not hand out what the export withheld: only the engine's
        // own metadata can say a plugin is loadable.
        if (has_availability && availability == Availability::Available) return rule.where + ": an overlay can restrict availability, not grant it";
        const auto* parameters = rule.find("parameters", Value::Kind::Array, false);
        const auto* group_list = rule.find("groups", Value::Kind::Array, false);
        const auto* file_entry = rule.find("file", Value::Kind::Object, false);
        if (!rule.issue.empty()) return rule.issue;
        // The plugin's file parameter: which patch:writable path it is, how the
        // page names it, and which files of which folder to offer.
        FileParameter file;
        if (file_entry) {
            Fields entry{*file_entry, rule.where + " file", {}};
            file.property = entry.text("property", true, 1024);
            file.label = entry.text("label", true, 24);
            file.folder = entry.text("folder", true, 32);
            const auto* listed = entry.find("extensions", Value::Kind::Array, true);
            if (!entry.issue.empty()) return entry.issue;
            if (!address(file.property)) return entry.where + ": \"property\" is not a URI";
            // A folder of the file store, one level deep at most: nam, ir/cab.
            const auto slash = file.folder.find('/');
            const bool plain_folder = slash == std::string::npos ? symbol(file.folder) :
                symbol(file.folder.substr(0, slash)) && symbol(file.folder.substr(slash + 1));
            if (file.label.empty() || !plain_folder) return entry.where + ": needs a label and a plain folder name";
            for (const auto& type : listed->items) {
                if (type.kind != Value::Kind::String || !symbol(type.text) || type.text.size() > 16) return entry.where + ": an extension is not a plain word";
                file.extensions.push_back(type.text);
            }
            if (file.extensions.empty()) return entry.where + ": names no extension";
        }
        std::vector<ParameterGroup> groups;
        if (group_list) for (const auto& entry : group_list->items) {
            if (entry.kind != Value::Kind::Object) return rule.where + ": a group is not an object";
            Fields group{entry, rule.where + " group", {}};
            ParameterGroup made;
            made.name = group.text("name", true, 24);
            const auto* symbols = group.find("parameters", Value::Kind::Array, true);
            if (!group.issue.empty()) return group.issue;
            if (made.name.empty()) return group.where + ": needs a name";
            for (const auto& s : symbols->items) {
                if (s.kind != Value::Kind::String || !symbol(s.text)) return group.where + " " + made.name + ": a control is not a symbol";
                made.symbols.push_back(s.text);
            }
            groups.push_back(std::move(made));
        }
        for (auto& d : changed) {
            if (uri.empty() ? d.uri.find(match) == std::string::npos : d.uri != uri) continue;
            if (parameters) if (auto issue = apply_parameter_rules(*parameters, rule.where, d); !issue.empty()) return issue;
            if (group_list) {
                // A rule for one plugin must name its controls; a rule matching
                // several leaves out what a module does not have.
                d.groups = groups;
                for (auto& g : d.groups)
                    std::erase_if(g.symbols, [&](const std::string& s) {
                        return !uri.empty() ? false : s != general_output_symbol && std::none_of(d.parameters.begin(), d.parameters.end(), [&](const Parameter& p) { return p.symbol == s; });
                    });
                std::erase_if(d.groups, [](const ParameterGroup& g) { return g.symbols.empty(); });
            }
            // A maker's name in front of every module says nothing on a small square.
            if (!prefix.empty() && d.name.size() > prefix.size() && d.name.starts_with(prefix)) d.name.erase(0, prefix.size());
            if (!name.empty()) d.name = name;
            if (!description.empty()) d.description = description;
            if (has_hidden) d.hidden = hidden;
            if (!category.empty()) { d.category = category; d.color = category_color(category); }
            if (!color.empty()) d.color = parsed;
            if (has_availability && static_cast<int>(availability) > static_cast<int>(d.availability)) { d.availability = availability; d.availability_reason = reason; }
            if (!output.empty()) d.output_symbol = output;
            if (file_entry) d.file = file;
            if (!tempo.empty()) d.tempo_symbol = tempo;
            // A module bypasses through its own switch or spills through the
            // engine's, never both: "engine" clears either, but one rule may
            // not turn the one into the other.
            if ((bypass == "spill" && d.port_bypass) || (bypass == "port" && d.spill_bypass))
                return rule.where + ": " + d.uri + " cannot be bypassed both through its own switch and by spilling";
            if (!bypass.empty()) { d.port_bypass = bypass == "port"; d.spill_bypass = bypass == "spill"; }
        }
    }
    // Families: plugins doing one job, offered in Add as one card with a pill
    // for each. A listed plugin that is not installed is left out, as a rule
    // for one is ignored.
    if (const auto* families = root.get("families")) {
        if (families->kind != Value::Kind::Array) return "overlay families are not a list";
        std::set<std::string> listed;
        number = 0;
        for (const auto& item : families->items) {
            if (item.kind != Value::Kind::Object) return "an overlay family is not an object";
            Fields family{item, "overlay family " + std::to_string(++number), {}};
            const auto name = family.text("name", true, 48), description = family.text("description", false);
            const auto* variants = family.find("variants", Value::Kind::Array, true);
            if (!family.issue.empty()) return family.issue;
            if (name.empty() || variants->items.size() < 2) return family.where + ": needs a name and two variants or more";
            std::set<std::string> labels;
            std::string category;
            int order = 0;
            for (const auto& entry : variants->items) {
                if (entry.kind != Value::Kind::Object) return family.where + " " + name + ": a variant is not an object";
                Fields variant{entry, family.where + " " + name + " variant", {}};
                const auto uri = variant.text("uri", true, 1024), label = variant.text("label", true, 16);
                if (!variant.issue.empty()) return variant.issue;
                if (!address(uri) || label.empty()) return variant.where + ": needs a URI and a label";
                if (!labels.insert(label).second) return variant.where + ": " + label + " is listed twice";
                if (!listed.insert(uri).second) return variant.where + ": " + uri + " is already in a family";
                const auto d = std::find_if(changed.begin(), changed.end(), [&](const ModuleDefinition& m) { return m.uri == uri; });
                if (d == changed.end()) continue;
                // One card has one category tag and sits under one filter.
                if (category.empty()) category = d->category;
                else if (d->category != category) return variant.where + ": " + uri + " is in " + d->category + ", not " + category;
                d->family = name; d->family_description = description; d->variant = label; d->variant_order = order++;
            }
        }
    }
    // Every module that makes sound has one Output: the plugin's own level
    // control where the overlay names one, the general one otherwise. "none"
    // is for modules that only pass their input on, such as a tuner.
    for (auto& d : changed) {
        if (!d.output_symbol.empty() && d.output_symbol != "none") {
            const auto own = std::find_if(d.parameters.begin(), d.parameters.end(), [&](const Parameter& p) { return p.symbol == d.output_symbol; });
            if (own == d.parameters.end()) return d.uri + ": its output control " + d.output_symbol + " is not one of its shown controls";
            own->name = "Output";
        } else if (d.output_symbol.empty() && std::any_of(d.ports.begin(), d.ports.end(), [](const Port& p) { return p.type == PortType::Audio && p.direction == Direction::Output; }) &&
                   std::none_of(d.parameters.begin(), d.parameters.end(), [](const Parameter& p) { return p.symbol == general_output_symbol; })) {
            if (std::any_of(d.ports.begin(), d.ports.end(), [](const Port& p) { return p.symbol == general_output_symbol; }))
                return d.uri + ": a port is named " + general_output_symbol + ", which the general Output needs";
            d.parameters.push_back(general_output_parameter());
        }
        if (d.port_bypass && d.enable_symbol.empty()) return d.uri + ": it is bypassed through its own switch but has none";
        // Spilling adds the plugin's audio to the dry signal; it needs some.
        if (d.spill_bypass && std::none_of(d.ports.begin(), d.ports.end(), [](const Port& p) { return p.type == PortType::Audio && p.direction == Direction::Output; }))
            return d.uri + ": it spills when bypassed but has no audio output";
        // A tempo control: a shown number whose range reaches into the tempo range.
        if (!d.tempo_symbol.empty()) {
            const auto it = std::find_if(d.parameters.begin(), d.parameters.end(), [&](const Parameter& p) { return p.symbol == d.tempo_symbol; });
            if (it == d.parameters.end() || it->kind == ParameterKind::Toggle || it->kind == ParameterKind::Enumeration ||
                it->maximum < minimum_tempo || it->minimum > maximum_tempo)
                return d.uri + ": its tempo control " + d.tempo_symbol + " is not a shown BPM control";
        }
        // The file is shown before the parameter groups and is sent on the
        // plugin's atom input.
        if (!d.file.property.empty()) {
            if (std::none_of(d.ports.begin(), d.ports.end(), [](const Port& p) { return (p.type == PortType::Atom || p.type == PortType::Midi) && p.direction == Direction::Input; }))
                return d.uri + ": it reads a file but has no input for messages";
        }
        // Groups are headings of shown controls, none in two groups.
        std::set<std::string> grouped;
        for (const auto& g : d.groups) {
            for (const auto& s : g.symbols) {
                if (std::none_of(d.parameters.begin(), d.parameters.end(), [&](const Parameter& p) { return p.symbol == s; }))
                    return d.uri + ": group " + g.name + " names " + s + ", which is not one of its shown controls";
                if (!grouped.insert(s).second) return d.uri + ": " + s + " is in two groups";
            }
        }
    }
    modules = std::move(changed);
    return {};
}
std::string load_catalogue(const std::string& path, const std::string& overlay_path, std::vector<ModuleDefinition>& out) {
    std::string text;
    if (auto issue = read_file(path, text); !issue.empty()) return issue;
    CatalogueDocument document;
    if (auto issue = read_catalogue(text, document); !issue.empty()) return path + ": " + issue;
    if (!overlay_path.empty()) {
        if (auto issue = read_file(overlay_path, text); !issue.empty()) return issue;
        if (auto issue = apply_catalogue_overlay(text, document.modules); !issue.empty()) return overlay_path + ": " + issue;
    }
    out = std::move(document.modules);
    return {};
}
}
