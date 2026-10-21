// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "storage/bundle.hpp"
#include "engine/ingen/protocol.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <set>

namespace peepo {
namespace {
using turtle::Term;
constexpr std::string_view xsd = "http://www.w3.org/2001/XMLSchema#";
const std::string ingen_ns(ingen::uri::ingen), lv2_ns(ingen::uri::lv2), peepo_ns(ingen::uri::peepo);
const std::string state_key = "http://lv2plug.in/ns/ext/state#state";
// The graph's own audio ports stand for IN's outputs and OUT's inputs, as in the adapter.
struct Endpoint { const char* symbol; InstanceId module; const char* port; bool midi = false; };
constexpr Endpoint endpoints[] = {{"in_l", 1, "out"}, {"in_r", 1, "out_r"}, {"out_l", 2, "in"}, {"out_r", 2, "in_r"},
                                  {"midi_in", 1, "midi", true}, {"midi_out", 2, "midi", true}};

std::optional<double> number(const Term& term) {
    if (term.kind != Term::Kind::Literal || !term.datatype.starts_with(xsd) || term.datatype == std::string(xsd) + "boolean" || term.datatype == std::string(xsd) + "string") return std::nullopt;
    double value = 0;
    const auto* begin = term.value.data() + (term.value.starts_with('+') ? 1 : 0);
    const auto* end = term.value.data() + term.value.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value)) return std::nullopt;
    return value;
}
std::optional<bool> boolean(const Term& term) {
    if (term.kind != Term::Kind::Literal || term.datatype != std::string(xsd) + "boolean") return std::nullopt;
    return term.value == "true" || term.value == "1";
}
// Relative references are kept as written; `./m3` and `m3` are one block.
std::string relative(std::string value) {
    while (value.starts_with("./")) value.erase(0, 2);
    while (value.size() > 1 && value.back() == '/') value.pop_back();
    return value;
}
std::string literal_number(double value) {
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
    return "\"" + std::string(buffer, result.ptr) + "\"^^xsd:double";
}
std::string iri(const std::string& text) { return "<" + text + ">"; }
}

std::string read_bundle(std::string_view text, Bundle& out) {
    out = {};
    turtle::Reader reader;
    std::vector<turtle::Triple> triples;
    if (auto issue = reader.parse(text, triples); !issue.empty()) return "main.ttl is not readable Turtle: " + issue;
    std::map<std::string, std::vector<std::pair<std::string, Term>>> subjects;
    for (auto& triple : triples) {
        const auto key = triple.subject.kind == Term::Kind::Blank ? "_:" + triple.subject.value : relative(triple.subject.value);
        subjects[key].emplace_back(triple.predicate.value, triple.object);
    }
    auto is = [](const std::vector<std::pair<std::string, Term>>& properties, const std::string& type) {
        return std::any_of(properties.begin(), properties.end(), [&](const auto& p) {
            return p.first == ingen::uri::rdf_type && p.second.kind == Term::Kind::Iri && p.second.value == type;
        });
    };
    const std::vector<std::pair<std::string, Term>>* graph = nullptr;
    for (const auto& [subject, properties] : subjects)
        if (subject.empty() && is(properties, ingen_ns + "Graph")) graph = &properties;
    if (!graph) return "main.ttl does not describe a graph";
    for (const auto& [predicate, object] : *graph) {
        if (predicate == peepo_ns + "patchName" && object.kind == Term::Kind::Literal) out.name = object.value;
        else if (predicate == peepo_ns + "patchId" && object.kind == Term::Kind::Literal) out.id = object.value;
        else if (predicate == peepo_ns + "schemaVersion") out.schema = static_cast<int>(number(object).value_or(0));
        else if (predicate == peepo_ns + "modified") out.modified = boolean(object).value_or(false);
        else if (predicate == peepo_ns + "tempo") out.tempo = number(object);
        else if (predicate == peepo_ns + "midiOnLoad" && object.kind == Term::Kind::Literal) {
            if (auto messages = decode_control_changes(object.value)) out.load_messages = std::move(*messages);
        }
        else if (predicate.starts_with(peepo_ns + "control-") && object.kind == Term::Kind::Literal && !object.value.empty())
            out.controls[predicate.substr(peepo_ns.size() + 8)] = object.value;
        else if (predicate == ingen_ns + "arc" && object.kind == Term::Kind::Blank) {
            std::string tail, head;
            const auto arc = subjects.find("_:" + object.value);
            if (arc != subjects.end())
                for (const auto& [key, end] : arc->second) {
                    if (key == ingen_ns + "tail" && end.kind == Term::Kind::Iri) tail = relative(end.value);
                    if (key == ingen_ns + "head" && end.kind == Term::Kind::Iri) head = relative(end.value);
                }
            if (tail.empty() || head.empty()) return "main.ttl has a cable without both ends";
            out.arcs.emplace_back(tail, head);
        }
    }
    for (const auto& [subject, properties] : subjects) {
        if (subject.empty() || subject.starts_with("_:") || subject.find('/') != std::string::npos || !is(properties, ingen_ns + "Block")) continue;
        BundleBlock block;
        block.symbol = subject;
        std::optional<double> x, y;
        for (const auto& [predicate, object] : properties) {
            if (predicate == lv2_ns + "prototype" && object.kind == Term::Kind::Iri) block.plugin = object.value;
            else if (predicate == peepo_ns + "displayName" && object.kind == Term::Kind::Literal) block.name = object.value;
            else if (predicate == peepo_ns + "file" && object.kind == Term::Kind::Literal) block.file = object.value;
            else if (predicate == ingen_ns + "canvasX") x = number(object);
            else if (predicate == ingen_ns + "canvasY") y = number(object);
            else if (predicate == ingen_ns + "enabled") block.enabled = boolean(object).value_or(true);
            else if (predicate == state_key && object.kind == Term::Kind::Iri) block.state = relative(object.value);
            // The general Output is the block's own level, read like a control.
            else if (predicate == peepo_ns + "outputLevel") { if (const auto value = number(object)) block.values[general_output_symbol] = *value; }
        }
        if (block.plugin.empty()) return "main.ttl has a block without a plugin: " + subject;
        if (!ingen::lv2_symbol(block.symbol) || !ingen::safe_iri(block.plugin)) return "main.ttl has a block the engine could not address: " + subject;
        if (x && y) { block.positioned = true; block.x = *x; block.y = *y; }
        const auto prefix = subject + "/";
        for (auto it = subjects.lower_bound(prefix); it != subjects.end() && it->first.starts_with(prefix); ++it) {
            const auto port = it->first.substr(prefix.size());
            if (port.find('/') != std::string::npos) continue;
            for (const auto& [predicate, object] : it->second)
                if (predicate == ingen_ns + "value") if (const auto value = number(object)) block.values[port] = *value;
        }
        out.blocks.push_back(std::move(block));
    }
    // Saved order is the engine's: blocks by name, m3 before m10.
    std::sort(out.blocks.begin(), out.blocks.end(), [](const BundleBlock& a, const BundleBlock& b) {
        return a.symbol.size() != b.symbol.size() ? a.symbol.size() < b.symbol.size() : a.symbol < b.symbol;
    });
    return {};
}

std::vector<SavedBlock> saved_blocks(const Bundle& bundle) {
    std::vector<SavedBlock> out;
    for (const auto& block : bundle.blocks) {
        const auto* known = definition(block.plugin);
        out.push_back({block.symbol, block.plugin, !block.name.empty() ? block.name : known ? known->name : block.symbol});
    }
    return out;
}

Graph bundle_graph(const Bundle& bundle) {
    Graph graph;
    graph.name = validate_patch_name(bundle.name).empty() ? bundle.name : "Untitled";
    if (bundle.tempo && *bundle.tempo >= minimum_tempo && *bundle.tempo <= maximum_tempo) graph.tempo = *bundle.tempo;
    graph.load_messages = bundle.load_messages;
    graph.patch_id = valid_patch_id(bundle.id) ? bundle.id : std::string();
    graph.dirty = bundle.modified;
    const auto& in = demo_catalogue()[0];
    const auto& out = demo_catalogue()[1];
    graph.modules.push_back({1, in.uri, in.name, 32, 132, true, false, {}, in.ports});
    graph.modules.push_back({2, out.uri, out.name, 664, 132, true, false, {}, out.ports});
    auto own_id = [](const std::string& symbol) -> InstanceId {
        if (symbol.size() < 2 || symbol.size() > 16 || symbol[0] != 'm' || !std::all_of(symbol.begin() + 1, symbol.end(), [](char c) { return c >= '0' && c <= '9'; })) return 0;
        const auto id = std::stoull(symbol.substr(1));
        return id >= 3 ? id : 0;
    };
    std::set<InstanceId> used{1, 2};
    std::map<std::string, InstanceId> ids;
    for (const auto& block : bundle.blocks) if (const auto id = own_id(block.symbol); id && used.insert(id).second) ids[block.symbol] = id;
    InstanceId next = 3;
    int unplaced = 0;
    for (const auto& block : bundle.blocks) {
        if (!ids.contains(block.symbol)) { while (used.contains(next)) ++next; ids[block.symbol] = next; used.insert(next); }
        const auto* known = definition(block.plugin);
        Module m{ids[block.symbol], block.plugin, block.name, 0, 0, false, !block.enabled, {}, {}};
        if (m.name.empty()) m.name = known ? known->name : block.symbol;
        m.file = block.file;
        if (block.positioned) {
            m.x = static_cast<int>(std::clamp(std::round(block.x), double(module_min_x), double(module_max_x)));
            m.y = static_cast<int>(std::clamp(std::round(block.y), double(module_min_y), double(module_max_y)));
        } else { m.x = std::min(module_max_x, 184 + 144 * unplaced++); m.y = 276; }
        if (known) {
            m.ports = known->ports;
            m.parameters = known->parameters;
            for (auto& p : m.parameters)
                if (const auto it = block.values.find(p.symbol); it != block.values.end()) {
                    auto value = p.kind == ParameterKind::Continuous ? it->second : std::round(it->second);
                    p.value = std::clamp(value, p.minimum, p.maximum);
                }
            // Earlier versions bypassed such a plugin by clearing its own switch.
            if (!known->enable_symbol.empty())
                if (const auto it = block.values.find(known->enable_symbol); it != block.values.end() && it->second < 0.5) m.bypassed = true;
        } else {
            // A plugin nobody can describe: keep the jacks its cables use, so they survive for repair.
            for (const auto& [tail, head] : bundle.arcs)
                for (const auto& [end, direction] : {std::pair{tail, Direction::Output}, std::pair{head, Direction::Input}})
                    if (end.starts_with(block.symbol + "/")) {
                        const auto symbol = end.substr(block.symbol.size() + 1);
                        if (std::none_of(m.ports.begin(), m.ports.end(), [&](const Port& p) { return p.symbol == symbol; }))
                            m.ports.push_back({symbol, direction, 1, PortType::Audio, {}});
                    }
        }
        graph.modules.push_back(std::move(m));
    }
    auto end = [&](const std::string& path, bool source) -> std::optional<std::pair<InstanceId, std::string>> {
        const auto slash = path.find('/');
        if (slash == std::string::npos) {
            for (const auto& endpoint : endpoints)
                if (path == endpoint.symbol && (endpoint.module == 1) == source) return std::pair{endpoint.module, std::string(endpoint.port)};
            return std::nullopt;
        }
        const auto id = ids.find(path.substr(0, slash));
        if (id == ids.end()) return std::nullopt;
        return std::pair{id->second, path.substr(slash + 1)};
    };
    for (const auto& [tail, head] : bundle.arcs) {
        const auto from = end(tail, true), to = end(head, false);
        if (!from || !to) continue;
        Connection cable{from->first, to->first, from->second, to->second};
        if (std::find(graph.connections.begin(), graph.connections.end(), cable) == graph.connections.end()) graph.connections.push_back(std::move(cable));
    }
    for (const auto& [control, text] : bundle.controls)
        for (const auto& part : assignment_parts(text)) {
            Assignment a;
            std::string block;
            if (!decode_assignment(control, part, a, block)) continue;
            // A block the bundle lacks leaves the assignment orphaned, for repair.
            if (const auto id = ids.find(block); id != ids.end()) a.module = id->second;
            graph.assignments.push_back(std::move(a));
        }
    return graph;
}

std::string write_main_ttl(const Graph& graph) {
    std::string text = ingen::prefixes() + "\n";
    for (const auto& endpoint : endpoints)
        text += iri(endpoint.symbol) + "\n\tlv2:symbol " + ingen::string_literal(endpoint.symbol) + " ;\n\ta " +
                (endpoint.midi ? "atom:AtomPort" : "lv2:AudioPort") + " , lv2:" + (endpoint.module == 1 ? "InputPort" : "OutputPort") +
                (endpoint.midi ? " ;\n\tatom:bufferType atom:Sequence ;\n\tatom:supports midi:MidiEvent" : "") + " .\n\n";
    std::vector<std::string> blocks;
    for (const auto& m : graph.modules) {
        if (m.fixed) continue;
        const auto symbol = "m" + std::to_string(m.id);
        blocks.push_back(iri(symbol));
        const auto* known = definition(m.uri);
        const bool own_switch = known && !known->enable_symbol.empty(), by_port = own_switch && known->port_bypass;
        std::string level;
        for (const auto& p : m.parameters) if (p.symbol == general_output_symbol) level = " ;\n\tpeepo:outputLevel " + literal_number(p.value);
        text += iri(symbol) + "\n\ta ingen:Block ;\n\tlv2:prototype " + iri(m.uri) + " ;\n\tingen:canvasX " + literal_number(m.x) + " ;\n\tingen:canvasY " +
                literal_number(m.y) + " ;\n\tpeepo:displayName " + ingen::string_literal(m.name) + (m.bypassed && !by_port ? " ;\n\tingen:enabled false" : "") + level +
                (m.file.empty() ? std::string() : " ;\n\tpeepo:file " + ingen::string_literal(m.file)) + " .\n\n";
        // A plugin that bypasses itself is saved with its switch off instead.
        if (own_switch) text += iri(symbol + "/" + known->enable_symbol) + "\n\tingen:value " + literal_number(by_port && m.bypassed ? 0 : 1) + " .\n\n";
        for (const auto& p : m.parameters)
            if (p.symbol != general_output_symbol) text += iri(symbol + "/" + p.symbol) + "\n\tingen:value " + literal_number(p.value) + " .\n\n";
    }
    auto address = [&](InstanceId id, const std::string& port, bool source) {
        for (const auto& endpoint : endpoints)
            if (endpoint.module == id && port == endpoint.port && (id == 1) == source) return std::string(endpoint.symbol);
        return "m" + std::to_string(id) + "/" + port;
    };
    text += "<>\n\ta ingen:Graph , lv2:Plugin";
    if (!blocks.empty()) {
        text += " ;\n\tingen:block ";
        for (std::size_t i = 0; i < blocks.size(); ++i) text += (i ? " , " : "") + blocks[i];
    }
    for (const auto& c : graph.connections)
        text += " ;\n\tingen:arc [\n\t\tingen:tail " + iri(address(c.from, c.output, true)) + " ;\n\t\tingen:head " + iri(address(c.to, c.input, false)) + "\n\t]";
    // One property per control, holding every module a stomp switches.
    std::map<std::string, std::string> controls;
    for (const auto& a : graph.assignments) {
        auto& value = controls[a.control];
        value += (value.empty() ? "" : " ") + encode_assignment(a, "m" + std::to_string(a.module));
    }
    for (const auto& [control, value] : controls) text += " ;\n\tpeepo:control-" + control + " " + ingen::string_literal(value);
    text += " ;\n\tpeepo:patchName " + ingen::string_literal(graph.name);
    text += " ;\n\tpeepo:tempo " + literal_number(graph.tempo);
    if (!graph.load_messages.empty()) text += " ;\n\tpeepo:midiOnLoad " + ingen::string_literal(encode_control_changes(graph.load_messages));
    if (!graph.patch_id.empty()) text += " ;\n\tpeepo:patchId " + ingen::string_literal(graph.patch_id);
    text += " ;\n\tpeepo:schemaVersion " + std::to_string(bundle_schema_version) + " ;\n\tpeepo:modified " + (graph.dirty ? "true" : "false") + " .\n";
    return text;
}
std::string write_manifest_ttl() {
    return ingen::prefixes() + "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n\n<main.ttl>\n\tlv2:prototype ingen:GraphPrototype ;\n\ta ingen:Graph , lv2:Plugin ;\n\trdfs:seeAlso <main.ttl> .\n";
}
}
