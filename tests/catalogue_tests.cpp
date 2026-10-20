// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// Catalogue files: what is written reads back identically, a damaged or
// hostile file is refused whole, and the overlay can restrict but not grant.
//   peepo_catalogue_tests <folder holding the exported fixture and overlay>
#include "catalogue/catalogue.hpp"
#include "catalogue/json.hpp"
#include "engine/simulated_engine.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace peepo;
namespace {
void check(bool result, const char* why) { if (!result) throw std::runtime_error(why); }
std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    check(at != std::string::npos, "test fixture text changed");
    return text.replace(at, from.size(), to);
}
void json_reading() {
    json::Value v;
    check(json::parse(R"( {"a": [1, -2.5e3, true, null], "b": "x\n\"\u00e9\ud83c\udfb8", "c": {}} )", v).empty(), "valid JSON refused");
    check(v.get("a")->items.size() == 4 && v.get("a")->items[1].number == -2500 && v.get("a")->items[2].boolean, "values misread");
    check(v.get("b")->text == "x\n\"\xc3\xa9\xf0\x9f\x8e\xb8" && v.get("c")->kind == json::Value::Kind::Object && !v.get("d"), "strings or members misread");
    for (const char* bad : {"{\"a\": 1,}", "[1 2]", "{\"a\": 1, \"a\": 2}", "\"open", "{\"a\": nan}", "[1] x", "\"\\ud83c\"", "\"tab\there\"", ""})
        check(!json::parse(bad, v).empty(), "invalid JSON accepted");
    check(!json::parse(std::string(100, '[') + std::string(100, ']'), v).empty(), "unbounded nesting accepted");
    check(json::quote("a\"b\\\n\x01") == "\"a\\\"b\\\\\\n\\u0001\"" && json::number(0.1) == "0.1" && json::number(-90) == "-90", "writer text wrong");
}
void round_trip() {
    CatalogueDocument written;
    for (const auto& d : demo_catalogue()) if (d.category != "Endpoint") written.modules.push_back(d);
    for (const auto& d : reference_catalogue()) written.modules.push_back(d);
    written.provenance = {{"tool", "test"}, {"lv2_path", "/usr/lib/lv2:\"quoted\""}};
    written.origins[written.modules[0].uri] = "class Distortion; bundle file:///x/";
    const auto text = write_catalogue(written);
    CatalogueDocument read;
    const auto issue = read_catalogue(text, read);
    check(issue.empty(), issue.c_str());
    check(read.modules.size() == written.modules.size() && read.provenance == written.provenance && read.origins == written.origins, "document changed in the file");
    for (std::size_t i = 0; i < read.modules.size(); ++i) {
        const auto& a = written.modules[i]; const auto& b = read.modules[i];
        check(a.uri == b.uri && a.name == b.name && a.category == b.category && a.description == b.description && a.color == b.color &&
              a.availability == b.availability && a.availability_reason == b.availability_reason && b.reference, "module changed in the file");
        check(a.ports.size() == b.ports.size() && a.parameters.size() == b.parameters.size(), "ports or parameters lost");
        for (std::size_t p = 0; p < a.ports.size(); ++p)
            check(a.ports[p].symbol == b.ports[p].symbol && a.ports[p].direction == b.ports[p].direction && a.ports[p].type == b.ports[p].type, "port changed in the file");
        for (std::size_t p = 0; p < a.parameters.size(); ++p) {
            const auto& x = a.parameters[p]; const auto& y = b.parameters[p];
            check(x.symbol == y.symbol && x.name == y.name && x.unit == y.unit && x.kind == y.kind && x.minimum == y.minimum && x.maximum == y.maximum &&
                  x.value == y.value && x.logarithmic == y.logarithmic && x.choices == y.choices, "parameter changed in the file");
        }
    }
    check(write_catalogue(read) == text, "the same catalogue wrote different bytes");

    // One bad entry refuses the file, and says which.
    CatalogueDocument untouched = read;
    auto refused = [&](const std::string& damaged, const char* why) {
        const auto message = read_catalogue(damaged, read);
        check(!message.empty() && read.modules.size() == untouched.modules.size(), why);
        return message;
    };
    refused(replaced(text, "\"peepo-catalogue\"", "\"something-else\""), "foreign file accepted");
    refused(replaced(text, "\"version\": 2", "\"version\": 3"), "newer file accepted");
    refused(replaced(text, "\"default\": 32", "\"default\": 320"), "default outside its range accepted");
    refused(replaced(text, "\"symbol\": \"drive\"", "\"symbol\": \"drive> ; x\""), "unusable symbol accepted");
    refused(replaced(text, "\"symbol\": \"drive\"", "\"symbol\": \"in\""), "control sharing a connector's symbol accepted");
    refused(replaced(text, "\"category\": \"Drive\"", "\"category\": \"Endpoint\""), "reserved category accepted");
    refused(replaced(text, "\"kind\": \"continuous\"", "\"kind\": \"wobbly\""), "unknown kind accepted");
    refused(replaced(text, "\"uri\": \"urn:peepo:demo:clouds\"", "\"uri\": \"urn:peepo:demo:drive\""), "repeated module accepted");
    refused(replaced(text, "\"minimum\": 0,", "\"minimum\": \"0\","), "mistyped number accepted");
    check(refused(replaced(text, "\"name\": \"Drive\",", ""), "module without a name accepted").find("urn:peepo:demo:drive") != std::string::npos, "fault not attributed to its module");
    refused(text.substr(0, text.size() / 2), "truncated file accepted");

    // A MIDI connector and a file input survive the file as themselves. A
    // version 1 file called both "midi", so neither becomes a connector.
    CatalogueDocument atoms;
    atoms.modules.push_back({"urn:test:midi", "Chord", "MIDI", "Notes to chords", 0x93a4b5,
                             {{"in", Direction::Input, 1, PortType::Midi}, {"out", Direction::Output, 1, PortType::Midi}, {"files", Direction::Input, 1, PortType::Atom}}, {}});
    const auto atom_text = write_catalogue(atoms);
    check(read_catalogue(atom_text, read).empty() && read.modules[0].ports[0].type == PortType::Midi && read.modules[0].ports[2].type == PortType::Atom,
          "MIDI and message ports changed in the file");
    const auto old = replaced(replaced(atom_text, "\"version\": 2", "\"version\": 1"), "\"type\": \"atom\"", "\"type\": \"midi\"");
    check(read_catalogue(old, read).empty() && std::all_of(read.modules[0].ports.begin(), read.modules[0].ports.end(), [](const Port& p) { return p.type == PortType::Atom; }),
          "a version 1 atom port became a MIDI connector");
}
void overlay() {
    CatalogueDocument document;
    for (const auto& d : reference_catalogue()) document.modules.push_back(d);
    document.modules[0].availability = Availability::Unavailable; document.modules[0].availability_reason = "export said no";
    auto modules = document.modules;
    const auto first = modules[0].uri, second = modules[1].uri;
    const std::string rules = R"({"format": "peepo-catalogue-overlay", "version": 1, "rules": [
        {"match": "polyclouds", "category": "Texture", "description": "Granular", "strip_prefix": "Mist "},
        {"match": "polyclouds", "color": "#102030"},
        {"uri": ")" + second + R"(", "name": "Renamed", "availability": "untested", "reason": "Not heard yet."},
        {"uri": ")" + first + R"(", "availability": "untested", "reason": "weaker than the export's"},
        {"uri": "urn:not:installed", "name": "Ghost"}]})";
    check(apply_catalogue_overlay(rules, modules).empty() && modules.size() == document.modules.size(), "valid overlay refused or added a module");
    for (const auto& d : modules) if (d.uri.find("polyclouds") != std::string::npos) check(d.category == "Texture" && d.color == 0x102030 && d.description == "Granular", "match rule not applied");
    check(modules[1].name == "Renamed" && modules[2].name == "Looping Delay" && modules[0].name == "Beat Repeat" && modules[1].availability == Availability::Untested && modules[1].availability_reason == "Not heard yet.", "exact rule not applied");
    check(modules[0].availability == Availability::Unavailable && modules[0].availability_reason == "export said no", "overlay weakened the export's verdict");
    const auto before = modules;
    for (const char* bad : {R"({"format": "peepo-catalogue-overlay", "version": 1, "rules": [{"match": "poly", "availability": "available"}]})",
                            R"({"format": "peepo-catalogue-overlay", "version": 1, "rules": [{"name": "No selector"}]})",
                            R"({"format": "peepo-catalogue-overlay", "version": 1, "rules": [{"match": "poly", "name": "Applied"}, {"match": "poly", "color": "blue"}]})",
                            R"({"format": "peepo-catalogue-overlay", "version": 1, "rules": [{"match": "poly", "category": "Endpoint"}]})",
                            R"({"format": "peepo-catalogue", "version": 1, "rules": []})"}) {
        check(!apply_catalogue_overlay(bad, modules).empty(), "invalid overlay accepted");
        for (std::size_t i = 0; i < modules.size(); ++i) check(modules[i].name == before[i].name && modules[i].availability == before[i].availability, "refused overlay was half applied");
    }
    check(category_color("Delay") == 0x7bbaed && category_color("Something new") == 0x93a4b5, "category colours changed");

    // A plugin that bypasses itself is bypassed through its own switch, which it must have.
    {
        std::vector<ModuleDefinition> own{{"urn:test:self", "Self", "Delay", "", 0x7bbaed, {}, {}}, {"urn:test:plain", "Plain", "Delay", "", 0x7bbaed, {}, {}}};
        own[0].enable_symbol = "enabled";
        const auto bypass = [](const char* uri, const char* how) {
            return std::string(R"({"format": "peepo-catalogue-overlay", "version": 1, "rules": [{"uri": ")") + uri + R"(", "bypass": ")" + how + R"("}]})";
        };
        check(apply_catalogue_overlay(bypass("urn:test:self", "port"), own).empty() && own[0].port_bypass && !own[1].port_bypass, "bypass through the port not applied");
        check(apply_catalogue_overlay(bypass("urn:test:self", "engine"), own).empty() && !own[0].port_bypass, "bypass could not be given back to the engine");
        check(!apply_catalogue_overlay(bypass("urn:test:plain", "port"), own).empty() && !apply_catalogue_overlay(bypass("urn:test:self", "both"), own).empty() && !own[0].port_bypass,
              "an impossible bypass accepted");
    }

    // Per-control rules: listed controls first, the rest in their own order.
    auto controls = document.modules;
    const std::string granular = "http://polyeffects.com/lv2/polyclouds#granular";
    const auto header = std::string(R"({"format": "peepo-catalogue-overlay", "version": 1, "rules": [)");
    const auto rule = [&](const std::string& parameters) { return header + R"({"uri": ")" + granular + R"(", "parameters": [)" + parameters + "]}]}"; };
    const auto find = [&](const std::vector<ModuleDefinition>& list) { return &*std::find_if(list.begin(), list.end(), [&](const ModuleDefinition& d) { return d.uri == granular; }); };
    const auto original = find(controls)->parameters;
    const auto applied = apply_catalogue_overlay(rule(R"({"symbol": "texture_param", "name": "Grain shape"}, {"symbol": "freeze_param", "kind": "toggle"},
        {"symbol": "pitch_param", "kind": "integer", "unit": "st", "default": 0}, {"symbol": "in_gain_param", "hidden": true})"), controls);
    check(applied.empty(), applied.c_str());
    const auto& changed = find(controls)->parameters;
    // One hidden; and with no rule naming its own output, it gains the general Output last.
    check(changed.size() == original.size() && changed.back().symbol == general_output_symbol && changed.back().unit == "dB" && changed[0].symbol == "texture_param" && changed[0].name == "Grain shape" && changed[1].kind == ParameterKind::Toggle &&
          changed[2].kind == ParameterKind::Integer && changed[2].unit == "st" && changed[2].value == 0 && changed[2].minimum == original[2].minimum, "parameter rule not applied");
    check(changed[3].symbol == "position_param" && changed[4].symbol == "size_param", "unlisted controls lost their order");
    check(find(controls)->project_defaults == std::vector<std::string>{"pitch_param"}, "chosen default not recorded for the adapter");
    const auto kept = controls;
    for (const char* bad : {R"({"symbol": "no_such_control"})", R"({"symbol": "pitch_param", "kind": "toggle"})",
                            R"({"symbol": "position_param", "default": 2})", R"({"symbol": "position_param", "kind": "enumeration"})",
                            R"({"symbol": "position_param"}, {"symbol": "position_param"})", R"({"symbol": "pitch_param", "default": 0.5, "kind": "integer"})",
                            R"({"name": "No symbol"})", R"("position_param")"}) {
        check(!apply_catalogue_overlay(rule(bad), controls).empty(), "invalid parameter rule accepted");
        check(find(controls)->parameters.size() == find(kept)->parameters.size() && find(controls)->parameters[0].symbol == find(kept)->parameters[0].symbol, "refused parameter rule was half applied");
    }

    // Names for a whole-number control's values, and a logarithmic scale.
    auto named = document.modules;
    const auto labelled = apply_catalogue_overlay(rule(R"({"symbol": "pitch_param", "kind": "integer", "default": 0}, {"symbol": "position_param", "logarithmic": false})"), named);
    check(labelled.empty(), labelled.c_str());
    for (const char* bad : {R"({"symbol": "pitch_param", "kind": "integer", "default": 0, "choices": ["Down", "Up"]})",
                            R"({"symbol": "position_param", "choices": ["A", "B"]})",
                            R"({"symbol": "position_param", "logarithmic": true})"}) {
        auto refused = document.modules;
        check(!apply_catalogue_overlay(rule(bad), refused).empty(), "impossible choices or a logarithmic scale through 0 accepted");
    }
    // A two-value control from 0: the Clouds freeze switch, as integer, with names.
    auto modes = document.modules;
    const auto chosen = apply_catalogue_overlay(rule(R"({"symbol": "freeze_param", "kind": "integer", "choices": ["Live", "Held"]})"), modes);
    check(chosen.empty(), chosen.c_str());
    const auto& freeze = find(modes)->parameters[0];
    check(freeze.kind == ParameterKind::Enumeration && freeze.choices == std::vector<std::string>{"Live", "Held"} && format_value([&] { auto p = freeze; p.value = 1; return p; }()) == "Held",
          "named values not shown as choices");

    // Named groups: any number of shown controls, none twice; the general
    // Output can be grouped; a rule matching several modules skips what one lacks.
    const auto groups = [&](const std::string& selector, const std::string& list) { return header + "{" + selector + R"(, "groups": [)" + list + "]}]}"; };
    const auto uri = R"("uri": ")" + granular + R"(")";
    auto grouped = document.modules;
    const auto made = apply_catalogue_overlay(groups(uri, R"({"name": "Grains", "parameters": ["position_param", "size_param"]}, {"name": "Level", "parameters": ["peepo_output"]})"), grouped);
    check(made.empty(), made.c_str());
    const auto& g = find(grouped)->groups;
    check(g.size() == 2 && g[0].name == "Grains" && g[0].symbols == std::vector<std::string>{"position_param", "size_param"} && g[1].symbols[0] == general_output_symbol, "groups not kept");
    auto matched = document.modules;
    check(apply_catalogue_overlay(groups(R"("match": "polyclouds#")", R"({"name": "Grains", "parameters": ["position_param", "not_in_every_mode"]})"), matched).empty() &&
          find(matched)->groups.size() == 1 && find(matched)->groups[0].symbols.size() == 1, "a matching rule did not skip a missing control");
    auto six_controls = document.modules;
    check(apply_catalogue_overlay(groups(uri, R"({"name": "Main", "parameters": ["position_param", "size_param", "density_param", "texture_param", "pitch_param", "blend_param"]})"), six_controls).empty(),
          "a six-control heading was rejected by an obsolete page limit");
    check(find(six_controls)->groups.size() == 1 && find(six_controls)->groups[0].symbols.size() == 6,
          "six controls must stay together under one heading");
    for (const char* bad : {R"({"name": "Twice", "parameters": ["position_param"]}, {"name": "Again", "parameters": ["position_param"]})",
                            R"({"name": "Gone", "parameters": ["no_such_control"]})",
                            R"({"name": "", "parameters": ["position_param"]})",
                            R"({"name": "Bad", "parameters": [3]})"}) {
        auto refused = document.modules;
        check(!apply_catalogue_overlay(groups(uri, bad), refused).empty(), "an impossible group accepted");
    }
    auto hidden_group = document.modules;
    check(!apply_catalogue_overlay(header + "{" + uri + R"(, "parameters": [{"symbol": "in_gain_param", "hidden": true}], "groups": [{"name": "Hidden", "parameters": ["in_gain_param"]}]}]})", hidden_group).empty(),
          "a group naming a hidden control accepted");
}
void fixture(const std::string& folder) {
    std::vector<ModuleDefinition> offered;
    const auto issue = load_catalogue(folder + "/wsl-debian.json", folder + "/overlay.json", offered);
    check(issue.empty(), issue.c_str());
    check(!load_catalogue(folder + "/missing.json", "", offered).empty() && !offered.empty(), "missing file accepted or cleared the catalogue");
    const ModuleDefinition* amp = nullptr;
    for (const auto& d : offered) {
        if (d.uri == "http://lv2plug.in/plugins/eg-amp") amp = &d;
        bool patchable = false;
        for (const auto& port : d.ports) patchable = patchable || port.type == PortType::Audio || port.type == PortType::Cv || port.type == PortType::Midi;
        check(d.availability != Availability::Available || patchable, "an entry with nothing to patch is offered");
        check(d.availability == Availability::Available || !d.availability_reason.empty(), "a withheld entry gives no reason");
    }
    check(amp && amp->availability == Availability::Available && amp->ports.size() == 2 && amp->parameters.size() == 1, "example amplifier missing from the exported fixture");
    unsigned visible_tuners = 0, retained_tuners = 0;
    for (const auto& d : offered) if (d.uri.starts_with("http://gareus.org/oss/lv2/tuna#")) {
        ++retained_tuners;
        if (!d.hidden) {
            ++visible_tuners;
            check(d.uri.ends_with("#mod") && d.parameters.size() == 1 && d.parameters[0].symbol == "tuning", "the offered tuner is not automatic with an A reference");
        }
    }
    check(visible_tuners == 1 && retained_tuners == 3, "duplicate tuners offered, or legacy tuner identities lost");
    const auto& gain = amp->parameters[0];
    check(gain.symbol == "gain" && gain.minimum == -90 && gain.maximum == 24 && gain.value == 0 && gain.unit == "dB" && gain.kind == ParameterKind::Continuous, "exported range or unit wrong");

    // Clouds as the overlay presents it: named per mode, switches for freeze
    // and reverse, whole semitones, the inert input gain hidden, the controls
    // that matter first, and Peepo's Output level beside Blend.
    const ModuleDefinition* shown = nullptr;
    for (const auto& d : offered) if (d.uri == "http://polyeffects.com/lv2/polyclouds#granular") shown = &d;
    check(shown && shown->name == "Mist" && shown->parameters.size() == 12 && shown->enable_symbol == "enabled", "Clouds overlay not applied");
    const char* order[] = {"position_param", "size_param", "density_param", "texture_param", "pitch_param", "blend_param", "output_param", "freeze_param", "reverse_param"};
    for (std::size_t i = 0; i < std::size(order); ++i) check(shown->parameters[i].symbol == order[i], "Clouds controls not in the overlay's order");
    for (const auto& p : shown->parameters) {
        check(p.symbol != "in_gain_param", "Clouds' inert input gain is shown");
        if (p.symbol == "freeze_param" || p.symbol == "reverse_param") check(p.kind == ParameterKind::Toggle, "Clouds freeze or reverse is not a switch");
        if (p.symbol == "output_param") check(p.name == "Output" && p.unit == "dB" && p.minimum == -24 && p.maximum == 12 && p.value == 0, "Clouds Output is not 0 dB in -24..+12 dB");
        if (p.symbol == "pitch_param") check(p.kind == ParameterKind::Integer && p.unit == "st" && p.value == 0 && p.minimum == -48 && p.maximum == 48, "Clouds pitch not whole semitones from 0 in the plugin's range");
    }
    check(shown->project_defaults == std::vector<std::string>{"pitch_param", "freeze_param", "reverse_param"}, "Clouds defaults not marked for the adapter to set");
    check(shown->groups.size() == 3 && shown->groups[0].name == "Main" && shown->groups[1].symbols[2] == "output_param", "Clouds pages not grouped");
    // The largest modules come in named pages, and their gains and filter
    // frequencies move on a logarithmic scale.
    for (const auto& d : offered) {
        const auto has = [&](const std::string& symbol) { return std::find_if(d.parameters.begin(), d.parameters.end(), [&](const Parameter& p) { return p.symbol == symbol; }); };
        if (d.uri == "http://gareus.org/oss/lv2/b_whirl#extended") check(d.groups.size() == 9 && has("filtafreq")->logarithmic, "the rotary speaker is not grouped");
        if (d.uri.starts_with("http://gareus.org/oss/lv2/fil4#")) check(d.groups.size() == 8 && has("peakreset") == d.parameters.end(), "the EQ is not grouped");
        if (d.uri == "http://calf.sourceforge.net/plugins/Vinyl") check(d.groups.size() == 9 && has("level_in")->logarithmic && has("level_out")->logarithmic, "Vinyl is not grouped");
        // Cabinets come from ir/cab, reverbs from ir/reverb, models from nam.
        if (d.uri.starts_with("http://polyeffects.com/lv2/polyconvo#Cab")) check(d.file.folder == "ir/cab" && d.file.label == "Cabinet", "a cabinet does not list ir/cab");
        else if (d.uri.starts_with("http://polyeffects.com/lv2/polyconvo#") || d.uri.starts_with("http://gareus.org/oss/lv2/convoLV2#"))
            check(d.file.folder == "ir/reverb", "a convolution reverb does not list ir/reverb");
        if (d.uri == "http://github.com/mikeoliphant/neural-amp-modeler-lv2") check(d.file.folder == "nam" && d.availability == Availability::Available, "NAM does not list nam");
        if (d.uri == "http://polyeffects.com/lv2/euclidean") {
            check(d.parameters.size() == 16 && d.parameters[0].symbol == "is_enabled1" && d.parameters[0].kind == ParameterKind::Toggle &&
                  d.parameters[1].symbol == "steps1" && d.parameters[2].symbol == "beats1" && d.parameters[3].symbol == "shift1" &&
                  d.parameters[12].symbol == "is_enabled4" && d.parameters[12].kind == ParameterKind::Toggle, "Euclidean loops not ordered or switches not marked");
        }
    }
    for (const auto& d : offered)
        if (d.uri.find("polyclouds#") != std::string::npos)
            check(d.parameters.size() == 12 && std::none_of(d.parameters.begin(), d.parameters.end(), [](const Parameter& p) { return p.kind == ParameterKind::Continuous && (p.symbol == "freeze_param" || p.symbol == "pitch_param"); }),
                  "a Clouds mode missed the shared rules");

    // Two independent readings of the same plugins must agree: the reference
    // entries come from the upstream Turtle files through rdflib, the fixture
    // from the installed bundles through Lilv. Both without the overlay.
    CatalogueDocument raw;
    {
        std::ifstream file(folder + "/wsl-debian.json", std::ios::binary);
        std::ostringstream text; text << file.rdbuf();
        const auto refused = read_catalogue(text.str(), raw);
        check(refused.empty(), refused.c_str());
    }
    // Every module on the Add screen says what it does, in words the
    // interface font can draw, not the export's class-and-author line.
    for (const auto& d : offered) {
        if (d.hidden) continue;
        const auto exported = std::find_if(raw.modules.begin(), raw.modules.end(), [&](const ModuleDefinition& m) { return m.uri == d.uri; });
        check(!d.description.empty() && exported != raw.modules.end() && d.description != exported->description, ("module has no written description: " + d.uri).c_str());
        check(std::all_of(d.description.begin(), d.description.end(), [](char c) { return c >= ' ' && c <= '~'; }), ("description the font cannot draw: " + d.uri).c_str());
    }
    unsigned compared = 0;
    for (const auto& reference : reference_catalogue()) {
        const ModuleDefinition* exported = nullptr;
        for (const auto& d : raw.modules) if (d.uri == reference.uri) exported = &d;
        if (!exported) continue;
        ++compared;
        check(exported->name == "Poly " + reference.name, "exported name differs from the reference reading");
        // Clouds' Output is Peepo's own (tools/plugin-patches/clouds-blend-and-output.patch),
        // added after every upstream control; the reference reads the unpatched files.
        const bool output_added = reference.uri.find("polyclouds#") != std::string::npos;
        check(!output_added || (!exported->parameters.empty() && exported->parameters.back().symbol == "output_param"), "patched Clouds has no Output control");
        check(exported->ports.size() == reference.ports.size() && exported->parameters.size() == reference.parameters.size() + (output_added ? 1 : 0),
              "exported port or control count differs from the reference reading");
        for (std::size_t i = 0; i < reference.ports.size(); ++i)
            check(exported->ports[i].symbol == reference.ports[i].symbol && exported->ports[i].direction == reference.ports[i].direction && exported->ports[i].type == reference.ports[i].type,
                  "exported connector differs from the reference reading");
        for (std::size_t i = 0; i < reference.parameters.size(); ++i) {
            const auto& x = exported->parameters[i]; const auto& y = reference.parameters[i];
            check(x.symbol == y.symbol && x.name == y.name && x.kind == y.kind && x.minimum == y.minimum && x.maximum == y.maximum && x.logarithmic == y.logarithmic &&
                  x.choices == y.choices && std::abs(x.value - y.value) < 1e-6,
                  ("exported control differs from the reference reading: " + reference.uri + " " + y.symbol + " (" + x.name + " / " + y.name + ", " +
                   std::to_string(x.minimum) + ".." + std::to_string(x.maximum) + " / " + std::to_string(y.minimum) + ".." + std::to_string(y.maximum) + ", default " +
                   std::to_string(x.value) + " / " + std::to_string(y.value) + ", kind " + std::to_string(int(x.kind)) + " / " + std::to_string(int(y.kind)) + ")").c_str());
        }
    }
    check(compared == reference_catalogue().size(), "the fixture lacks some of the Mutable-derived modules");

    // A real engine offers its own catalogue: no demo fixtures, endpoints kept.
    const auto count = offered.size();
    const auto exported = offered;
    replace_catalogue(std::move(offered));
    check(catalogue().size() == count + 2 && !definition("urn:peepo:demo:drive") && !definition("urn:peepo:demo:nam") && definition("http://lv2plug.in/plugins/eg-amp"),
          "replaced catalogue kept fixtures or lost entries");
    check(validate_graph(empty_graph("Still valid")).empty() && module_available(empty_graph("x").modules[0]), "endpoints lost with the fixtures");

    // The simulation offers the exported modules beside its demo ones, and
    // its sample patches still work.
    std::vector<ModuleDefinition> both;
    for (const auto& d : demo_catalogue()) if (d.category != "Endpoint") both.push_back(d);
    both.insert(both.end(), exported.begin(), exported.end());
    replace_catalogue(std::move(both));
    check(definition("urn:peepo:demo:drive") && definition("http://polyeffects.com/lv2/polyclouds#granular") && catalogue().size() == count + demo_catalogue().size(), "simulation catalogue incomplete");
    check(validate_graph(demo_graph()).empty() && validate_graph(parallel_demo_graph()).empty(), "demo patches broken by the exported catalogue");
    SimulatedEngine engine;
    engine.submit(AddModule{"http://polyeffects.com/lv2/polyclouds#granular", 240, 240});
    check(engine.advance(100)[0].accepted, "simulation could not add an exported module");
    const auto& clouds = engine.snapshot().modules.back();
    check(clouds.ports.size() == 16 && clouds.parameters.size() == 12, "exported module lost its connectors or controls in the simulation");
    engine.submit(SetParameter{clouds.id, clouds.parameters[0].symbol, clouds.parameters[0].maximum});
    engine.submit(Connect{{3, clouds.id, "out", "l_in"}});
    const auto events = engine.advance(100);
    check(events.size() == 2 && events[0].accepted && events[1].accepted, "exported module could not be edited or cabled in the simulation");
    engine.submit(AddModule{"http://drobilla.net/plugins/mda/DX10", 24, 24});
    check(!engine.advance(100)[0].accepted, "a withheld instrument was added in the simulation");
}
}
int main(int argc, char** argv) {
    try {
        json_reading();
        round_trip();
        overlay();
        fixture(argc > 1 ? argv[1] : "config/catalogue");
        std::cout << "Catalogue files: JSON reading, round trip, refusals, overlay rules and the exported fixture passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
