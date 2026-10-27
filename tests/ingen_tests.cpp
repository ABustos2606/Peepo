// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// The Ingen adapter against a stand-in engine that speaks the recorded
// protocol, including the awkward parts: replies split across reads, values
// that are not echoed to their sender, copies announced without contents and
// notification numbers that collide with request numbers. What the real
// engine does is checked separately by peepo_ingen_live.
#include "engine/ingen/ingen_engine.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace peepo;
using namespace peepo::ingen;
namespace {
void check(bool result, const char* why) { if (!result) throw std::runtime_error(why); }
const std::string amp = "http://lv2plug.in/plugins/eg-amp";
// Like Poly's Clouds: silent until its own `enabled` control is set.
const std::string switched = "urn:test:switched";
// Bypasses itself through that control, like Discipline.
const std::string self_bypassed = "urn:test:self-bypassed";
// Reads a file and follows the tempo: it has an atom input for patch messages.
const std::string filed = "urn:test:filed";
// A reverb whose tail rings on through the engine's spill bypass.
const std::string spilled = "urn:test:spilled";
const std::string rdf_type(uri::rdf_type);
std::string full(std::string_view space, std::string_view local) { return std::string(space) + std::string(local); }
Value iri_value(std::string text) { return {Value::Kind::Uri, 0, false, std::move(text)}; }
Value number_value(double number) { return {Value::Kind::Number, number, false, {}}; }

struct FakeEngine;
struct FakeTransport final : Transport {
    FakeTransport(FakeEngine& engine, std::size_t chunk) : engine(engine), chunk(chunk) {}
    ~FakeTransport() override { close(); }
    void open() override;
    void close() override;
    void send(std::string) override;
    std::vector<Event> poll() override { auto out = std::move(events); events.clear(); return out; }
    void deliver(std::string text);
    FakeEngine& engine;
    std::size_t chunk;
    std::vector<Event> events;
    turtle::Reader reader;
    bool connected = false, greeted = false;
};
struct FakeEngine {
    std::map<std::string, Properties> objects{{"ingen:/main", {}}};
    std::vector<std::pair<std::string, std::string>> arcs;
    std::set<std::string> plugins{amp, switched, self_bypassed, filed, spilled};
    std::vector<FakeTransport*> clients;
    bool reachable = true, fail_saves = false;
    // Write paths relative to <ingen:/>, as the engine does when built with serd 0.30 (Debian 12).
    bool relative = false;
    int mute = 0, requests = 0, label = 0;
    // Every tempo the graph was given, in order.
    std::vector<double> tempos;
    // The control port waiting for MIDI learn, as Ingen keeps one.
    std::string learning;
    // Each bundle a client sent: the subject and property of every Set in it.
    std::vector<std::vector<std::string>> bundles;
    int open_bundles = 0;
    // Saved bundles by file URI: the objects under ingen:/main and the cables.
    std::map<std::string, std::pair<std::map<std::string, Properties>, std::vector<std::pair<std::string, std::string>>>> files;

    static std::string term(const Value& value) {
        switch (value.kind) {
        case Value::Kind::Uri: return "<" + value.text + ">";
        case Value::Kind::Number: return float_literal(value.number);
        case Value::Kind::Boolean: return value.boolean ? "true" : "false";
        // A MIDI binding, as the engine writes one.
        case Value::Kind::Object:
            return "[\n\t\ta <" + value.text + "> ;\n\t\t<" + full(uri::midi, value.text.ends_with("Controller") ? "controllerNumber" : value.text.ends_with("NoteOn") ? "noteNumber" : "channel") +
                   "> \"" + std::to_string(static_cast<int>(value.number)) + "\"^^xsd:int\n\t]";
        // The engine switches to a long string when the text has a quote in it.
        default: return value.text.find('"') != std::string::npos ? "\"\"\"" + value.text + "\"\"\"" : string_literal(value.text);
        }
    }
    static std::string key(const std::string& predicate) {
        if (predicate == rdf_type) return "a";
        if (predicate.starts_with(uri::ingen)) return "ingen:" + predicate.substr(uri::ingen.size());
        if (predicate.starts_with(uri::lv2)) return "lv2:" + predicate.substr(uri::lv2.size());
        return "<" + predicate + ">";
    }
    static std::string body(const Properties& properties) {
        if (properties.empty()) return "[]";
        std::string text = "[\n";
        for (std::size_t i = 0; i < properties.size(); ++i)
            text += "\t\t" + key(properties[i].first) + " " + term(properties[i].second) + (i + 1 < properties.size() ? " ;\n" : "\n");
        return text + "\t\t]";
    }
    std::string node(const std::string& lines) { return "\n_:b" + std::to_string(label++) + "\n" + lines + " .\n"; }
    std::string bundle(bool start) { return node(std::string("\ta ingen:Bundle") + (start ? "Start" : "End") + " ;\n\tpatch:sequenceNumber \"" + std::to_string(label) + "\"^^xsd:int"); }
    std::string response(int sequence, int status, const std::string& subject = {}) {
        return node("\ta patch:Response ;\n\tpatch:sequenceNumber \"" + std::to_string(sequence) + "\"^^xsd:int ;\n" +
                    (subject.empty() ? "" : "\tpatch:subject <" + subject + "> ;\n") + "\tpatch:body \"" + std::to_string(status) + "\"^^xsd:int");
    }
    // Notifications reuse the request's number, as the real engine's sometimes do.
    std::string put(int sequence, const std::string& subject, const Properties& properties) {
        return node("\ta patch:Put ;\n\tpatch:sequenceNumber \"" + std::to_string(sequence) + "\"^^xsd:int ;\n\tpatch:subject <" + subject + "> ;\n\tpatch:body " + body(properties));
    }
    std::string arc(int sequence, const char* type, const std::string& tail, const std::string& head) {
        return node(std::string("\ta patch:") + type + " ;\n\tpatch:sequenceNumber \"" + std::to_string(sequence) + "\"^^xsd:int ;\n" +
                    (std::string(type) == "Put" ? "\tpatch:subject <ingen:/main/> ;\n" : "") +
                    "\tpatch:body [\n\t\ta ingen:Arc ;\n\t\tingen:tail <" + tail + "> ;\n\t\tingen:head <" + head + ">\n\t]");
    }
    static void merge(Properties& object, const Properties& added) {
        for (const auto& [name, ignored] : added) std::erase_if(object, [&](const auto& entry) { return entry.first == name; });
        object.insert(object.end(), added.begin(), added.end());
    }
    std::string describe(int sequence, const std::string& path) {
        auto text = put(sequence, path, objects.at(path));
        for (const auto& [child, properties] : objects) if (child.starts_with(path + "/")) text += put(sequence, child, properties);
        return text;
    }
    void create_block(const std::string& path, Properties properties) {
        objects[path] = std::move(properties);
        auto port = [&](const char* symbol, int index, const char* kind, const char* direction) {
            auto& made = objects[path + "/" + symbol];
            made = {{full(uri::lv2, "index"), number_value(index)}, {rdf_type, iri_value(full(uri::lv2, kind))}, {rdf_type, iri_value(full(uri::lv2, direction))}};
            return &made;
        };
        auto* gain = port("gain", 0, "ControlPort", "InputPort");
        gain->insert(gain->begin(), {full(uri::ingen, "value"), number_value(0)});
        port("in", 1, "AudioPort", "InputPort");
        port("out", 2, "AudioPort", "OutputPort");
        if (const auto* prototype = find(objects[path], full(uri::lv2, "prototype")); prototype && prototype->text == filed) {
            auto* control = port("control", 3, "InputPort", "InputPort");
            control->push_back({rdf_type, iri_value(full(uri::atom, "AtomPort"))});
        }
        if (const auto* prototype = find(objects[path], full(uri::lv2, "prototype")); prototype && (prototype->text == switched || prototype->text == self_bypassed)) {
            auto* enabled = port("enabled", 3, "ControlPort", "InputPort");
            enabled->insert(enabled->begin(), {full(uri::ingen, "value"), number_value(0)});
        }
    }
    void erase(const std::string& path) {
        std::erase_if(objects, [&](const auto& entry) { return entry.first == path || entry.first.starts_with(path + "/"); });
        std::erase_if(arcs, [&](const auto& a) { return a.first.starts_with(path + "/") || a.second.starts_with(path + "/") || a.first == path || a.second == path; });
    }
    void request(FakeTransport& from, const Message& m) {
        ++requests;
        if (mute > 0) { --mute; return; }
        int status = 0;
        std::string subject, all, others;
        auto end_of = [&](const char* local) { const auto* end = find(m.body, full(uri::ingen, local)); return end ? normal_path(end->text) : std::string(); };
        switch (m.type) {
        case Message::Type::BundleStart:
            if (open_bundles++ == 0) bundles.emplace_back();
            break;
        case Message::Type::BundleEnd:
            open_bundles = std::max(0, open_bundles - 1);
            break;
        case Message::Type::Get:
            if (m.subject == "ingen:/plugins") {
                auto text = bundle(true) + response(m.sequence, 0) + bundle(true);
                for (const auto& plugin : plugins) text += put(label, plugin, {{rdf_type, iri_value(full(uri::lv2, "Plugin"))}});
                // The real listing closes its inner bundle, then the outer one in a second message.
                from.deliver(text + bundle(false));
                from.deliver(bundle(false));
                return;
            }
            if (m.subject == "ingen:/main") {
                all = put(1, "ingen:/main/", {{full(uri::ingen, "polyphony"), number_value(1)}});
                if (objects.contains(m.subject)) all += put(1, m.subject, objects.at(m.subject));
                for (const auto& [path, properties] : objects) if (path != m.subject) all += put(m.sequence, path, properties);
                for (const auto& [tail, head] : arcs) all += arc(m.sequence, "Put", tail, head);
            } else if (objects.contains(m.subject)) all = describe(m.sequence, m.subject);
            else status = 16;
            from.deliver(bundle(true) + response(m.sequence, status) + all + bundle(false));
            return;
        case Message::Type::Put:
            if (has(m.body, rdf_type, full(uri::ingen, "Arc"))) {
                const std::pair cable{end_of("tail"), end_of("head")};
                if (!objects.contains(cable.first) || !objects.contains(cable.second)) status = 16;
                else if (std::find(arcs.begin(), arcs.end(), cable) != arcs.end()) status = 11;
                else { arcs.push_back(cable); all = arc(m.sequence, "Put", cable.first, cable.second); }
                subject = cable.second;
            } else if (has(m.body, rdf_type, full(uri::ingen, "Block")) && !objects.contains(m.subject)) {
                const auto* prototype = find(m.body, full(uri::lv2, "prototype"));
                if (!prototype || !plugins.contains(prototype->text)) { status = 22; subject = prototype ? prototype->text : ""; }
                else {
                    create_block(m.subject, m.body);
                    // Two responses for one request, as recorded.
                    all = describe(m.sequence, m.subject) + response(m.sequence, 0, m.subject) + put(m.sequence, m.subject, objects.at(m.subject));
                }
            } else if (objects.contains(m.subject) || has(m.body, rdf_type, full(uri::lv2, "AudioPort")) || has(m.body, rdf_type, full(uri::atom, "AtomPort"))) {
                merge(objects[m.subject], m.body);
                all = put(m.sequence, m.subject, m.body);
            } else status = 4;
            break;
        case Message::Type::Set:
            subject = m.subject;
            if (open_bundles) bundles.back().push_back(m.subject + " " + m.property);
            if (!objects.contains(m.subject)) status = 16;
            else {
                // MIDI learn waits for a message; nothing is stored until then.
                if (m.property == full(uri::midi, "binding") && m.value.kind == Value::Kind::Uri && m.value.text == full(uri::patch, "wildcard")) { learning = m.subject; break; }
                // A graph property set without a context is accepted and never described again.
                if (m.subject != "ingen:/main" || m.context == full(uri::ingen, "internalContext")) merge(objects[m.subject], {{m.property, m.value}});
                if (m.subject == "ingen:/main" && m.property == full(uri::peepo, "tempo")) tempos.push_back(m.value.number);
                // Told to everyone except the client that asked.
                others = node("\ta patch:Set ;\n\tpatch:sequenceNumber \"" + std::to_string(m.sequence) + "\"^^xsd:int ;\n\tpatch:subject <" + m.subject +
                              "> ;\n\tpatch:property " + key(m.property) + " ;\n\tpatch:value " + term(m.value));
            }
            break;
        case Message::Type::Delete:
            if (!m.subject.empty()) {
                if (!objects.contains(m.subject)) { status = 16; subject = m.subject; }
                else { erase(m.subject); all = node("\ta patch:Delete ;\n\tpatch:sequenceNumber \"" + std::to_string(m.sequence) + "\"^^xsd:int ;\n\tpatch:subject <" + m.subject + ">"); }
            } else {
                const std::pair cable{end_of("tail"), end_of("head")};
                if (!std::erase(arcs, cable)) { status = 16; subject = cable.second; }
                else all = arc(m.sequence, "Delete", cable.first, cable.second);
            }
            break;
        case Message::Type::Copy:
            if (m.destination.starts_with("file://")) {
                // A save: only a graph can be written, and the whole of it.
                if (m.subject != "ingen:/main") status = 3;
                else if (fail_saves) status = 12;
                else {
                    auto& file = files[m.destination];
                    file.first.clear();
                    for (const auto& [path, properties] : objects) if (path.starts_with("ingen:/main")) file.first[path] = properties;
                    file.second = arcs;
                }
                all = node("\ta patch:Copy ;\n\tpatch:sequenceNumber \"" + std::to_string(m.sequence) + "\"^^xsd:int ;\n\tpatch:subject <" + m.subject +
                           "> ;\n\tpatch:destination <" + m.destination + ">");
            } else if (m.subject.starts_with("file://")) {
                // A load is answered at once, whatever the parser made of the file, and
                // the loaded graph is announced afterwards, as the real engine does.
                for (auto* client : clients) client->deliver(bundle(true) + (client == &from ? response(m.sequence, 0) : std::string()) + bundle(false));
                const auto file = files.find(m.subject);
                if (file == files.end()) return;
                // One block whose plugin is missing, and the parser builds none of the graph.
                for (const auto& [path, properties] : file->second.first)
                    if (const auto* prototype = find(properties, full(uri::lv2, "prototype")); prototype && !plugins.contains(prototype->text)) return;
                std::string text;
                for (const auto& [path, properties] : file->second.first) {
                    merge(objects[path], properties);
                    text += put(++label, path == "ingen:/main" ? "ingen:/main/" : path, properties);
                }
                for (const auto& cable : file->second.second)
                    if (objects.contains(cable.first) && objects.contains(cable.second) && std::find(arcs.begin(), arcs.end(), cable) == arcs.end()) {
                        arcs.push_back(cable); text += arc(++label, "Put", cable.first, cable.second);
                    }
                for (auto* client : clients) client->deliver(bundle(true) + text + bundle(false));
                return;
            } else if (!objects.contains(m.subject)) status = 16;
            else if (objects.contains(m.destination)) status = 11;
            else {
                std::map<std::string, Properties> copies;
                for (const auto& [path, properties] : objects)
                    if (path == m.subject || path.starts_with(m.subject + "/")) copies[m.destination + path.substr(m.subject.size())] = properties;
                objects.insert(copies.begin(), copies.end());
                // Announced, never described.
                all = node("\ta patch:Copy ;\n\tpatch:sequenceNumber \"" + std::to_string(m.sequence) + "\"^^xsd:int ;\n\tpatch:subject <" + m.subject +
                           "> ;\n\tpatch:destination <" + m.destination + ">");
            }
            break;
        case Message::Type::Patch:
            // Only what MIDI learn needs: dropping every value of a property.
            subject = m.subject;
            if (!objects.contains(m.subject)) status = 16;
            else for (const auto& [predicate, value] : m.remove)
                if (value.kind == Value::Kind::Uri && value.text == full(uri::patch, "wildcard")) std::erase_if(objects[m.subject], [&](const auto& e) { return e.first == predicate; });
            break;
        default: status = 4;
        }
        for (auto* client : clients) {
            const auto start = bundle(true);
            client->deliver(start + (client == &from ? response(m.sequence, status, subject) + all : all + others) + bundle(false));
        }
    }
    // A MIDI message reached the engine's control port while a port was
    // learning: the binding is stored and told to every client.
    void learn(const std::string& type, int number) {
        if (learning.empty()) return;
        const Value binding{Value::Kind::Object, double(number), false, full(uri::midi, type)};
        merge(objects[learning], {{full(uri::midi, "binding"), binding}});
        for (auto* client : clients)
            client->deliver(node("\ta patch:Set ;\n\tpatch:sequenceNumber \"" + std::to_string(++label) + "\"^^xsd:int ;\n\tpatch:subject <" + learning +
                                 "> ;\n\tpatch:property <" + full(uri::midi, "binding") + "> ;\n\tpatch:value " + term(binding)));
        learning.clear();
    }
    void die() {
        for (auto* client : std::vector(clients)) { client->connected = false; client->events.push_back({Transport::Event::Kind::Closed, "Engine closed the connection"}); }
        clients.clear();
    }
};
void FakeTransport::open() {
    close();
    reader = turtle::Reader(); greeted = false;
    if (!engine.reachable) { events.push_back({Event::Kind::Closed, "Engine is not reachable"}); return; }
    connected = true;
    engine.clients.push_back(this);
    events.push_back({Event::Kind::Opened, {}});
}
void FakeTransport::close() { connected = false; events.clear(); std::erase(engine.clients, this); }
void FakeTransport::send(std::string text) {
    if (!connected) return;
    // The reader deadlock in the real engine: one newline after the last period, never a NUL.
    check(text.ends_with(" .\n") && text.find('\0') == std::string::npos, "request framing would park the engine's reader");
    std::vector<turtle::Triple> triples;
    check(reader.parse(text, triples).empty(), "adapter wrote Turtle the engine could not read");
    for (const auto& message : decode(triples)) engine.request(*this, message);
}
void FakeTransport::deliver(std::string text) {
    if (engine.relative) for (std::size_t at; (at = text.find("<ingen:/")) != std::string::npos;) text.replace(at, 8, "</");
    if (!greeted) {
        text = "@prefix atom: <http://lv2plug.in/ns/ext/atom#> .\n@prefix ingen: <http://drobilla.net/ns/ingen#> .\n@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n"
               "@prefix patch: <http://lv2plug.in/ns/ext/patch#> .\n@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .\n" + text;
        greeted = true;
    }
    text += '\0';
    // One read is rarely one message.
    for (std::size_t at = 0; at < text.size(); at += chunk) events.push_back({Event::Kind::Data, text.substr(at, chunk)});
}

struct Rig {
    explicit Rig(FakeEngine& fake, std::size_t chunk = 7) {
        auto owned = std::make_unique<FakeTransport>(fake, chunk);
        transport = owned.get();
        engine = std::make_unique<IngenEngine>(Options{}, std::move(owned));
    }
    // Run until the engine has nothing more to say, and return what it said.
    std::vector<EngineEvent> settle(unsigned step = 1) {
        std::vector<EngineEvent> all;
        for (int quiet = 0; quiet < 3;) {
            auto events = engine->advance(step);
            quiet = events.empty() ? quiet + 1 : 0;
            for (auto& event : events) all.push_back(std::move(event));
        }
        return all;
    }
    EngineEvent result(Command command) {
        const auto id = engine->submit(std::move(command));
        for (auto& event : settle()) if (event.request == id) return event;
        throw std::runtime_error("no result for a submitted command");
    }
    const Graph& graph() const { return engine->snapshot(); }
    FakeTransport* transport;
    std::unique_ptr<IngenEngine> engine;
};
const Value* property(const FakeEngine& fake, const std::string& path, const std::string& predicate) {
    const auto it = fake.objects.find(path);
    return it == fake.objects.end() ? nullptr : find(it->second, predicate);
}

void turtle_and_protocol() {
    // Recorded from the engine on 13 October 2026, whitespace and repetitions included.
    const std::string recorded =
        "@prefix atom: <http://lv2plug.in/ns/ext/atom#> .\n@prefix ingen: <http://drobilla.net/ns/ingen#> .\n@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n"
        "@prefix patch: <http://lv2plug.in/ns/ext/patch#> .\n@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .\n\n"
        "_:b60\n\ta ingen:BundleStart ;\n\tpatch:sequenceNumber \"45\"^^xsd:int .\n\n"
        "_:b61\n\ta patch:Response ;\n\tpatch:sequenceNumber \"12\"^^xsd:int ;\n\tpatch:body \"0\"^^xsd:int .\n\n"
        "_:b62\n\ta patch:Put ;\n\tpatch:context ingen:internalContext ;\n\tpatch:subject <ingen:/main/> ;\n\tpatch:body [\n\t\tingen:polyphony \"1\"^^xsd:int\n\t] .\n\n"
        "_:b64\n\ta patch:Put ;\n\tpatch:sequenceNumber \"1\"^^xsd:int ;\n\tpatch:context ingen:externalContext ;\n\tpatch:subject <ingen:/main/> ;\n\tpatch:body [] .\n\n"
        "_:b66\n\ta patch:Put ;\n\tpatch:sequenceNumber \"2\"^^xsd:int ;\n\tpatch:subject <ingen:/main/m3> ;\n\tpatch:body [\n\t\tingen:canvasX \"300.0\"^^xsd:float ;\n"
        "\t\tingen:canvasY \"132.0\"^^xsd:float ;\n\t\tingen:enabled false ;\n\t\tlv2:prototype <http://lv2plug.in/plugins/eg-amp> ;\n\t\ta ingen:Block ;\n"
        "\t\t<urn:peepo:ns#displayName> \"Amp\"\n\t] .\n\n"
        "_:b68\n\ta patch:Put ;\n\tpatch:sequenceNumber \"3\"^^xsd:int ;\n\tpatch:subject <ingen:/main/control> ;\n\tpatch:body [\n\t\tatom:bufferType atom:Sequence ,\n"
        "\t\t\tatom:Sequence ;\n\t\tatom:supports patch:Message ;\n\t\t<http://lv2plug.in/ns/ext/resize-port#minimumSize> \"4096\"^^xsd:int ;\n"
        "\t\tlv2:index \"0\"^^xsd:int ,\n\t\t\t\"0\"^^xsd:int ;\n\t\tlv2:name \"Control\" ;\n\t\ta atom:AtomPort ,\n\t\t\tatom:AtomPort ,\n\t\t\tlv2:InputPort\n\t\t] .\n\n"
        "_:b82\n\ta patch:Put ;\n\tpatch:sequenceNumber \"10\"^^xsd:int ;\n\tpatch:subject <ingen:/main/> ;\n\tpatch:body [\n\t\ta ingen:Arc ;\n"
        "\t\tingen:tail <ingen:/main/in_l> ;\n\t\tingen:head <ingen:/main/m3/in>\n\t] .\n\n"
        "_:b58\n\ta patch:Set ;\n\tpatch:sequenceNumber \"43\"^^xsd:int ;\n\tpatch:subject <ingen:/main> ;\n\tpatch:property <urn:peepo:ns#patchName> ;\n"
        "\tpatch:value \"\"\"My \"patch\" \xc3\xa9\"\"\" .\n\n"
        "_:b86\n\ta ingen:BundleEnd ;\n\tpatch:sequenceNumber \"46\"^^xsd:int .\n";
    turtle::Reader reader;
    std::vector<turtle::Triple> triples;
    check(reader.parse(recorded, triples).empty(), "recorded engine output was not readable");
    const auto messages = decode(triples);
    check(messages.size() == 9 && messages[0].type == Message::Type::BundleStart && messages[8].type == Message::Type::BundleEnd, "recorded messages miscounted");
    check(messages[1].type == Message::Type::Response && messages[1].sequence == 12 && messages[1].status == 0, "response not decoded");
    check(messages[2].subject == "ingen:/main" && messages[3].body.empty(), "trailing slash or empty body mishandled");
    const auto& made = messages[4];
    check(made.type == Message::Type::Put && made.subject == "ingen:/main/m3" && has(made.body, rdf_type, full(uri::ingen, "Block")), "block not decoded");
    check(find(made.body, full(uri::ingen, "canvasX"))->number == 300 && !find(made.body, full(uri::ingen, "enabled"))->boolean, "typed values not decoded");
    check(find(made.body, full(uri::peepo, "displayName"))->text == "Amp", "project property not decoded");
    check(has(messages[5].body, rdf_type, full(uri::atom, "AtomPort")) && has(messages[5].body, rdf_type, full(uri::lv2, "InputPort")), "repeated objects lost");
    check(find(messages[6].body, full(uri::ingen, "head"))->text == "ingen:/main/m3/in", "arc not decoded");
    check(messages[7].type == Message::Type::Set && messages[7].value.text == "My \"patch\" \xc3\xa9", "long string not decoded");
    // Prefixes last for the connection; statement punctuation inside a string is text.
    triples.clear();
    check(reader.parse("_:x a patch:Set ; patch:value \"one. two ; [three] \\\"four\\\" \\u00e9\\n\" .", triples).empty(), "later message lost its prefixes");
    check(decode(triples)[0].value.text == "one. two ; [three] \"four\" \xc3\xa9\n", "punctuation or escapes in a string misread");
    triples.clear();
    check(!reader.parse("_:y a patch:Set ; patch:value \"unterminated .", triples).empty() && triples.empty(), "truncated message accepted");
    check(!reader.parse("_:z a unknown:Thing .", triples).empty(), "undeclared prefix accepted");
    // What is written is what the engine was verified to accept.
    check(set(7, "ingen:/main/m3/gain", "ingen:value", float_literal(3.5)) ==
          "[]\n\ta patch:Set ;\n\tpatch:sequenceNumber \"7\"^^xsd:int ;\n\tpatch:subject <ingen:/main/m3/gain> ;\n\tpatch:property ingen:value ;\n\tpatch:value \"3.5\"^^xsd:float .\n",
          "request text changed");
    check(string_literal("a \"b\"\n\\") == "\"a \\\"b\\\"\\n\\\\\"" && !safe_iri("urn:x> ; ingen:enabled false") && !lv2_symbol("gain> .") && lv2_symbol("in_l"), "unsafe text could reach the engine");
    check(float_literal(0.1) == "\"0.100000001\"^^xsd:float" && float_literal(-24) == "\"-24\"^^xsd:float", "float literal is not locale-independent or exact");
}

void adapter() {
    // As the catalogue export describes the LV2 example amplifier.
    register_definitions({{amp, "Amp", "Utility", "Amplifier", 0x93a4b5, {{"in", Direction::Input}, {"out", Direction::Output}},
                           {{"gain", "Gain", "dB", ParameterKind::Continuous, -90, 24, 0}}}});
    FakeEngine fake;
    Rig rig(fake);
    auto& engine = *rig.engine;
    check(engine.state() == EngineState::Disconnected && validate_graph(engine.snapshot()).empty(), "no usable graph before connecting");
    engine.submit(RenamePatch{"Too early"});
    check(!engine.advance(0)[0].accepted, "edit accepted without an engine");
    engine.connect();
    check(engine.state() == EngineState::Connecting, "connect skipped lifecycle");
    engine.submit(RenamePatch{"Too early"});
    auto events = rig.settle();
    check(engine.state() == EngineState::Ready && !events.empty() && events.back().state == EngineState::Ready && events.back().snapshot, "did not become ready");
    check(std::count_if(events.begin(), events.end(), [](const EngineEvent& e) { return e.request && !e.accepted; }) == 1 && rig.graph().name == "Untitled", "edit sent while connecting");
    for (const char* port : {"in_l", "in_r", "out_l", "out_r"}) check(fake.objects.contains(std::string("ingen:/main/") + port), "audio endpoint not created");
    // The MIDI endpoints are sequences of MIDI events, which Ingen turns into JACK MIDI ports.
    for (const char* port : {"midi_in", "midi_out"}) {
        const auto found = fake.objects.find(std::string("ingen:/main/") + port);
        check(found != fake.objects.end() && has(found->second, rdf_type, full(uri::atom, "AtomPort")) &&
              has(found->second, full(uri::atom, "supports"), "http://lv2plug.in/ns/ext/midi#MidiEvent") &&
              has(found->second, rdf_type, full(uri::lv2, port == std::string("midi_in") ? "InputPort" : "OutputPort")), "MIDI endpoint not created");
    }
    check(rig.graph().modules.size() == 2 && rig.graph().connections.empty() && validate_graph(rig.graph()).empty(), "empty engine graph misread");
    const auto caps = engine.capabilities();
    check(!caps.simulation && caps.audio_processing && caps.session_save && caps.persistent_save && caps.plugin_state, "capabilities misstated");

    // Add: nothing changes before the engine answers.
    auto before = fake.requests;
    const auto adding = engine.submit(AddModule{amp, 184, 132});
    check(rig.graph().modules.size() == 2, "module appeared before acknowledgment");
    events = rig.settle();
    check(events.size() == 1 && events[0].request == adding && events[0].accepted, "add was not acknowledged exactly once");
    check(rig.graph().modules.size() == 3 && rig.graph().dirty, "accepted module missing");
    const auto first = rig.graph().modules[2];
    check(first.id == 3 && first.uri == amp && first.name == "Amp" && first.x == 184 && first.y == 132 && !first.bypassed, "module identity or position wrong");
    check(first.ports.size() == 2 && first.ports[0].symbol == "in" && first.ports[0].direction == Direction::Input && first.ports[1].type == PortType::Audio, "ports not taken from the engine");
    check(first.parameters.size() == 1 && first.parameters[0].symbol == "gain" && first.parameters[0].minimum == -90 && first.parameters[0].maximum == 24 && first.parameters[0].unit == "dB",
          "parameter not described by the catalogue");
    check(validate_graph(rig.graph()).empty() && fake.objects.contains("ingen:/main/m3"), "engine and interface disagree after add");
    before = fake.requests;
    check(!rig.result(AddModule{"urn:peepo:demo:drive", 24, 24}).accepted && !rig.result(AddModule{"urn:missing", 24, 24}).accepted && fake.requests == before,
          "a plugin the engine lacks was requested");

    // Cables: the engine accepts feedback and duplicates differently, so both are refused here.
    check(rig.result(Connect{{1, 3, "out", "in"}}).accepted && rig.result(Connect{{3, 2, "out", "in"}}).accepted, "cable refused");
    check(fake.arcs.size() == 2 && fake.arcs[0] == std::pair<std::string, std::string>{"ingen:/main/in_l", "ingen:/main/m3/in"}, "cable not mapped to engine ports");
    before = fake.requests;
    check(!rig.result(Connect{{3, 2, "out", "in"}}).accepted && !rig.result(Connect{{3, 3, "out", "in"}}).accepted, "duplicate or feedback cable accepted");
    check(!rig.result(Connect{{1, 3, "out", "gain"}}).accepted && !rig.result(Disconnect{{1, 3, "out_r", "in"}}).accepted && fake.requests == before, "invalid cable reached the engine");
    check(rig.result(Connect{{1, 2, "out_r", "in_r"}}).accepted && rig.graph().connections.size() == 3, "endpoint-to-endpoint cable failed");

    // Values: the engine stores anything, and does not tell the sender what it set.
    check(rig.result(SetParameter{3, "gain", 6.5}).accepted && property(fake, "ingen:/main/m3/gain", full(uri::ingen, "value"))->number == 6.5, "value not set");
    check(find_module(rig.graph(), 3)->parameters[0].value == 6.5, "accepted value missing from snapshot");
    before = fake.requests;
    for (double bad : {9999.0, -91.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        check(!rig.result(SetParameter{3, "gain", bad}).accepted, "out-of-range value accepted");
    check(!rig.result(SetParameter{3, "nope", 1}).accepted && !rig.result(SetParameter{1, "gain", 1}).accepted && !rig.result(SetParameter{99, "gain", 1}).accepted && fake.requests == before,
          "invalid value reached the engine");
    check(rig.result(SetBypass{3, true}).accepted && !property(fake, "ingen:/main/m3", full(uri::ingen, "enabled"))->boolean && find_module(rig.graph(), 3)->bypassed, "bypass not applied");
    check(!rig.result(SetBypass{1, true}).accepted && !rig.result(RemoveModule{2}).accepted && !rig.result(MoveModule{1, 48, 48}).accepted, "endpoint was editable");
    check(rig.result(MoveModule{3, -500, 9000}).accepted && find_module(rig.graph(), 3)->x == module_min_x && find_module(rig.graph(), 3)->y == module_max_y, "move not clamped");
    check(property(fake, "ingen:/main/m3", full(uri::ingen, "canvasY"))->number == module_max_y, "position not stored in the engine");
    check(rig.result(MoveModule{3, 312, 144}).accepted, "move failed");

    // Duplicate: the engine copies; values and bypass come with it, cables do not.
    const auto cables = rig.graph().connections;
    check(rig.result(DuplicateModule{3, 456, 144}).accepted && rig.graph().modules.size() == 4, "duplicate failed");
    const auto copy = rig.graph().modules[3];
    check(copy.id == 4 && copy.uri == amp && copy.bypassed && copy.parameters[0].value == 6.5 && copy.x == 456 && copy.y == 144 && copy.ports.size() == 2, "duplicate lost state");
    check(rig.graph().connections == cables && !rig.result(DuplicateModule{1, 24, 24}).accepted, "duplicate copied cables or an endpoint");
    check(rig.result(SetParameter{4, "gain", -3}).accepted && find_module(rig.graph(), 3)->parameters[0].value == 6.5, "copy edits changed the original");

    // Names survive quoting, and live in the engine.
    const std::string awkward = "Friday's \"loud\" set \xc3\xa9 .";
    check(rig.result(RenamePatch{awkward}).accepted && rig.graph().name == awkward && property(fake, "ingen:/main", full(uri::peepo, "patchName"))->text == awkward, "patch name mangled");
    check(!rig.result(RenamePatch{"  "}).accepted && rig.graph().name == awkward, "blank name accepted");

    // A second client, as a control daemon would be: both stay in step.
    Rig other(fake, 4096);
    other.engine->connect(); other.settle();
    check(other.engine->state() == EngineState::Ready && other.graph().modules.size() == 4 && other.graph().name == awkward, "second client read a different graph");
    check(find_module(other.graph(), 3)->parameters[0].value == 6.5 && find_module(other.graph(), 4)->bypassed && other.graph().connections == rig.graph().connections,
          "second client lost values, bypass or cables");
    check(other.result(SetParameter{3, "gain", 12}).accepted, "second client edit failed");
    events = rig.settle();
    check(events.size() == 1 && events[0].request == 0 && events[0].accepted && find_module(rig.graph(), 3)->parameters[0].value == 12, "another client's change was not reported once");
    check(other.result(AddModule{amp, 600, 300}).accepted && other.result(Connect{{4, 5, "out", "in"}}).accepted, "second client add failed");
    rig.settle();
    check(rig.graph().modules.size() == 5 && rig.graph().modules[4].id == 5 && rig.graph().connections.size() == 4, "another client's module or cable missing");
    check(rig.result(AddModule{amp, 24, 24}).accepted && rig.graph().modules.back().id == 6, "identity reused after another client's add");
    other.settle();
    check(other.result(RemoveModule{4}).accepted, "second client removal failed");
    rig.settle();
    check(!find_module(rig.graph(), 4) && rig.graph().connections.size() == 3 && !rig.result(SetParameter{4, "gain", 0}).accepted, "removed module or its cables survived");
    // The engine does not check values: whatever another client stored is shown inside the range.
    fake.objects["ingen:/main/m3/gain"] = {{full(uri::ingen, "value"), number_value(9999)}, {rdf_type, iri_value(full(uri::lv2, "ControlPort"))}, {rdf_type, iri_value(full(uri::lv2, "InputPort"))}};
    fake.objects["ingen:/main/foreign"] = {{rdf_type, iri_value(full(uri::ingen, "Block"))}, {full(uri::lv2, "prototype"), iri_value("urn:some:unknown:plugin")}};
    Rig late(fake);
    late.engine->connect(); late.settle();
    check(find_module(late.graph(), 3)->parameters[0].value == 24, "out-of-range engine value was shown as is");
    const auto& foreign = late.graph().modules.back();
    check(foreign.id >= 7 && foreign.name == "foreign" && !module_available(foreign) && validate_graph(late.graph()).empty(), "unknown block is not a repairable placeholder");
    check(!late.result(SetBypass{foreign.id, true}).accepted && late.result(RemoveModule{foreign.id}).accepted && !fake.objects.contains("ingen:/main/foreign"), "placeholder rules wrong");
    late.engine->disconnect(); rig.settle(); other.settle();
    check(rig.result(SetParameter{3, "gain", 6.5}).accepted, "value could not be corrected");

    // A plugin with its own on/off control: adding it switches it on, and it
    // stays on, since off it writes silence. Bypass is the engine's, which
    // passes the input through. The control never appears as a parameter.
    // Its gain has a default the catalogue chose over the plugin's 0, which
    // adding it must set.
    {
        ModuleDefinition own{switched, "Switched", "Utility", "", 0x93a4b5, {{"in", Direction::Input}, {"out", Direction::Output}},
                             {{"gain", "Gain", "dB", ParameterKind::Continuous, -90, 24, -6}}};
        own.enable_symbol = "enabled";
        own.project_defaults = {"gain"};
        register_definitions({own});
        check(rig.result(AddModule{switched, 600, 24}).accepted, "switched module refused");
        const auto id = rig.graph().modules.back().id;
        const auto port = "ingen:/main/m" + std::to_string(id) + "/enabled";
        check(property(fake, port, full(uri::ingen, "value"))->number == 1 && !rig.graph().modules.back().bypassed, "module with its own switch was left off");
        check(property(fake, "ingen:/main/m" + std::to_string(id) + "/gain", full(uri::ingen, "value"))->number == -6 &&
              rig.graph().modules.back().parameters[0].value == -6, "the catalogue's chosen default was not set on the new module");
        check(rig.graph().modules.back().parameters.size() == 1, "the on/off control was shown as a parameter");
        const auto block = "ingen:/main/m" + std::to_string(id);
        check(rig.result(SetBypass{id, true}).accepted && find_module(rig.graph(), id)->bypassed, "bypass refused");
        check(property(fake, port, full(uri::ingen, "value"))->number == 1, "bypass muted the plugin through its own switch");
        check(!property(fake, block, full(uri::ingen, "enabled"))->boolean, "bypass did not use the engine's pass-through");
        check(rig.result(DuplicateModule{id, 600, 144}).accepted && rig.graph().modules.back().bypassed, "copy lost the bypass");
        check(rig.result(RemoveModule{rig.graph().modules.back().id}).accepted && rig.result(SetBypass{id, false}).accepted && !find_module(rig.graph(), id)->bypassed &&
              property(fake, block, full(uri::ingen, "enabled"))->boolean, "bypass could not be switched off");
        // Earlier versions bypassed by clearing the switch: such a module reads
        // as bypassed, and enabling it switches the plugin back on.
        for (auto& [key, value] : fake.objects[port]) if (key == full(uri::ingen, "value")) value = number_value(0);
        Rig legacy(fake);
        legacy.engine->connect(); legacy.settle();
        check(find_module(legacy.graph(), id)->bypassed, "a cleared switch did not read as bypassed");
        check(legacy.result(SetBypass{id, false}).accepted && property(fake, port, full(uri::ingen, "value"))->number == 1 && !find_module(legacy.graph(), id)->bypassed,
              "enabling did not switch the plugin back on");
        legacy.engine->disconnect(); rig.settle();
        check(rig.result(RemoveModule{id}).accepted, "switched module could not be removed");
    }
    // A plugin that bypasses itself: its own switch is the bypass, and the
    // block keeps running, so its echoes can ring on.
    {
        ModuleDefinition own{self_bypassed, "Self", "Delay", "", 0x7bbaed, {{"in", Direction::Input}, {"out", Direction::Output}},
                             {{"gain", "Gain", "dB", ParameterKind::Continuous, -90, 24, -6}}};
        own.enable_symbol = "enabled";
        own.port_bypass = true;
        register_definitions({own});
        check(rig.result(AddModule{self_bypassed, 600, 24}).accepted, "self-bypassing module refused");
        const auto id = rig.graph().modules.back().id;
        const auto block = "ingen:/main/m" + std::to_string(id), port = block + "/enabled";
        check(property(fake, port, full(uri::ingen, "value"))->number == 1, "self-bypassing module was left off");
        check(rig.result(SetBypass{id, true}).accepted && find_module(rig.graph(), id)->bypassed, "self bypass refused");
        check(property(fake, port, full(uri::ingen, "value"))->number == 0, "self bypass did not clear the plugin's switch");
        check(property(fake, block, full(uri::ingen, "enabled"))->boolean, "self bypass stopped the block");
        Rig reader(fake);
        reader.engine->connect(); reader.settle();
        check(find_module(reader.graph(), id)->bypassed, "another client did not see the self bypass");
        reader.engine->disconnect(); rig.settle();
        check(rig.result(DuplicateModule{id, 600, 144}).accepted && rig.graph().modules.back().bypassed, "copy lost the self bypass");
        check(rig.result(RemoveModule{rig.graph().modules.back().id}).accepted && rig.result(SetBypass{id, false}).accepted && !find_module(rig.graph(), id)->bypassed &&
              property(fake, port, full(uri::ingen, "value"))->number == 1 && property(fake, block, full(uri::ingen, "enabled"))->boolean, "self bypass could not be switched off");
        check(rig.result(RemoveModule{id}).accepted, "self-bypassing module could not be removed");
    }

    // A file and the tempo. The file reaches the plugin as a patch:Set on its
    // atom input and is named on the block for every client; the tempo is a
    // graph property, given to the module that follows it, inside its range.
    {
        ModuleDefinition own{filed, "Filed", "Amp", "", 0x93a4b5, {{"in", Direction::Input}, {"out", Direction::Output}, {"control", Direction::Input, 1, PortType::Atom}},
                             {{"gain", "Rate", "BPM", ParameterKind::Continuous, 40, 240, 100}}};
        own.file = {"urn:test:filed#model", "Model", "nam", {"nam"}};
        own.tempo_symbol = "gain";
        register_definitions({own});
        check(rig.result(SetTempo{96}).accepted && rig.graph().tempo == 96 && engine.telemetry().tempo_bpm == 96 &&
              property(fake, "ingen:/main", full(uri::peepo, "tempo"))->number == 96, "tempo not stored on the graph");
        const auto sent = fake.requests;
        check(!rig.result(SetTempo{500}).accepted && !rig.result(SetTempo{std::nan("")}).accepted && rig.graph().tempo == 96 && fake.requests == sent,
              "a tempo outside the range reached the engine");
        check(rig.result(AddModule{filed, 600, 24}).accepted, "file-reading module refused");
        const auto id = rig.graph().modules.back().id;
        const auto block = "ingen:/main/m" + std::to_string(id);
        check(property(fake, block + "/gain", full(uri::ingen, "value"))->number == 96, "an added module did not take the patch tempo");
        check(rig.result(SetTempo{20 + 270}).accepted && property(fake, block + "/gain", full(uri::ingen, "value"))->number == 240 &&
              find_module(rig.graph(), id)->parameters[0].value == 240, "a following module was not set, inside its range");
        check(rig.result(SetFile{id, "/files/nam/My amp.nam"}).accepted && find_module(rig.graph(), id)->file == "/files/nam/My amp.nam" &&
              property(fake, block, full(uri::peepo, "file"))->text == "/files/nam/My amp.nam", "file not named on the block");
        check(!rig.result(SetFile{id, "relative.nam"}).accepted && !rig.result(SetFile{id, "/a/../b.nam"}).accepted && !rig.result(SetFile{3, "/files/x.nam"}).accepted,
              "an unsafe path, or a module without a file, was accepted");
        Rig second(fake);
        second.engine->connect(); second.settle();
        check(find_module(second.graph(), id)->file == "/files/nam/My amp.nam" && second.graph().tempo == 290, "a second client did not see the file and tempo");
        second.engine->disconnect(); rig.settle();
        check(rig.result(DuplicateModule{id, 600, 144}).accepted && rig.graph().modules.back().file == "/files/nam/My amp.nam", "the duplicate lost the file");
        check(rig.result(RemoveModule{rig.graph().modules.back().id}).accepted && rig.result(RemoveModule{id}).accepted && rig.result(SetTempo{120}).accepted, "cleanup failed");
    }

    // Session snapshot: kept by the interface, restored through the engine.
    events.clear();
    const auto saving = engine.submit(SaveGraph{});
    for (auto& e : rig.settle()) if (e.request == saving) events.push_back(e);
    check(events.size() == 1 && events[0].accepted && events[0].saved_session && !rig.graph().dirty && events[0].saved_session->modules.size() == rig.graph().modules.size(), "session save failed");
    const auto saved = rig.graph();
    check(!rig.result(SaveGraph{SaveScope::Session, true}).accepted, "plugin state in a session snapshot pretended to succeed");
    check(!rig.result(SaveGraph{SaveScope::Persistent}).accepted && !rig.result(SaveGraph{SaveScope::Persistent, true, "file:///patches/x", "p-one"}).accepted &&
          !rig.result(SaveGraph{SaveScope::Persistent, true, "file:///with%20space/x.ingen", "p-one"}).accepted &&
          !rig.result(SaveGraph{SaveScope::Persistent, true, "file:///patches/x.ingen", "Not an id"}).accepted, "unsafe save location or identity accepted");
    check(rig.result(NewPatch{"Blank"}).accepted && rig.graph().modules.size() == 2 && rig.graph().connections.size() == 2 && rig.graph().dirty && rig.graph().name == "Blank", "new patch wrong");
    check(fake.arcs.size() == 2 && std::none_of(fake.objects.begin(), fake.objects.end(), [](const auto& o) { return o.first.starts_with("ingen:/main/m") && !o.first.starts_with("ingen:/main/midi_"); }), "engine kept old blocks");
    check(rig.result(LoadSession{}).accepted && !rig.graph().dirty, "session restore failed");
    check(rig.graph().name == saved.name && rig.graph().connections.size() == saved.connections.size() && rig.graph().modules.size() == saved.modules.size(), "restored graph differs");
    for (const auto& m : saved.modules) {
        const auto* restored = find_module(rig.graph(), m.id);
        check(restored && restored->uri == m.uri && restored->x == m.x && restored->y == m.y && restored->bypassed == m.bypassed && restored->name == m.name, "restored module differs");
        for (std::size_t i = 0; i < m.parameters.size(); ++i) check(restored->parameters[i].value == m.parameters[i].value, "restored value differs");
    }
    for (const auto& cable : saved.connections) check(std::find(rig.graph().connections.begin(), rig.graph().connections.end(), cable) != rig.graph().connections.end(), "restored cable missing");
    other.settle();
    check(other.graph().modules.size() == saved.modules.size() && other.graph().connections.size() == saved.connections.size() && other.graph().name == saved.name, "second client lost step during restore");
    // The unsaved-changes marker lives in the engine, so every client sees it.
    const auto modified = full(uri::peepo, "modified"), identity = full(uri::peepo, "patchId");
    check(rig.result(SetParameter{3, "gain", 3}).accepted && rig.graph().dirty && property(fake, "ingen:/main", modified)->boolean, "edit did not mark the engine's graph");
    before = fake.requests;
    check(rig.result(SetParameter{3, "gain", 3}).accepted && fake.requests == before + 1, "an already marked graph was marked again");
    other.settle();
    check(other.graph().dirty, "another client did not see the marker");
    // Persistent save: the engine writes the bundle, and only its answer clears the marker.
    const std::string first_bundle = "file:///patches/p-one/r000001.ingen";
    auto outcome = rig.result(SaveGraph{SaveScope::Persistent, true, first_bundle, "p-one"});
    check(outcome.accepted && !rig.graph().dirty && rig.graph().patch_id == "p-one" && fake.files.contains(first_bundle), "persistent save failed");
    const auto& written = fake.files.at(first_bundle).first.at("ingen:/main");
    check(find(written, identity)->text == "p-one" && find(written, full(uri::peepo, "schemaVersion"))->number == 1, "bundle lacks its identity or schema");
    check(!property(fake, "ingen:/main", modified)->boolean && property(fake, "ingen:/main", identity)->text == "p-one", "engine not told the save completed");
    other.settle();
    check(!other.graph().dirty && other.graph().patch_id == "p-one", "another client did not see the save");
    // A refused save leaves the identity and the marker as they were.
    rig.result(SetParameter{3, "gain", 4});
    fake.fail_saves = true;
    outcome = rig.result(SaveGraph{SaveScope::Persistent, true, "file:///patches/p-two/r000001.ingen", "p-two"});
    fake.fail_saves = false;
    rig.settle();
    check(!outcome.accepted && rig.graph().patch_id == "p-one" && rig.graph().dirty && property(fake, "ingen:/main", identity)->text == "p-one", "refused save changed identity or marker");
    // An edit sent while the engine writes the bundle is not in it: the patch stays unsaved.
    rig.engine->submit(SaveGraph{SaveScope::Persistent, true, "file:///patches/p-one/r000002.ingen", "p-one"});
    rig.engine->submit(RenamePatch{"Renamed"}); rig.settle();
    check(rig.graph().name == "Renamed" && rig.graph().dirty && property(fake, "ingen:/main", modified)->boolean &&
          find(fake.files.at("file:///patches/p-one/r000002.ingen").first.at("ingen:/main"), full(uri::peepo, "patchName"))->text != "Renamed",
          "an edit made during a save was marked saved");
    // A bundle the patch store could not publish: unsaved again, under the identity it had.
    check(rig.result(SaveGraph{SaveScope::Persistent, true, "file:///patches/p-two/r000001.ingen", "p-two"}).accepted && !rig.graph().dirty && rig.graph().patch_id == "p-two",
          "save as failed");
    check(rig.result(RestoreUnsaved{"p-one"}).accepted && rig.graph().dirty && rig.graph().patch_id == "p-one" && property(fake, "ingen:/main", modified)->boolean &&
          property(fake, "ingen:/main", identity)->text == "p-one", "an unpublished save stayed marked saved");
    check(!rig.result(RestoreUnsaved{"Not an id"}).accepted, "an invalid identity was restored");
    // Load: everything goes, the bundle comes in, and the result waits for the loaded graph.
    check(rig.result(NewPatch{"Elsewhere"}).accepted && rig.graph().patch_id.empty() && rig.graph().modules.size() == 2, "new patch kept the identity or modules");
    std::vector<SavedBlock> blocks;
    for (const auto& m : saved.modules) if (!m.fixed) blocks.push_back({"m" + std::to_string(m.id), m.uri, m.name});
    fake.tempos.clear();
    outcome = rig.result(LoadPatch{first_bundle, blocks, {}, 96});
    // The bundle's tempo is set before the bundle is read, never the default between.
    check(!fake.tempos.empty() && std::all_of(fake.tempos.begin(), fake.tempos.end(), [](double t) { return t == 96; }), "a load passed through another tempo");
    check(outcome.accepted && outcome.message.find("Loaded " + saved.name) == 0 && outcome.message.find("did not restore") == std::string::npos, "load failed or misreported");
    check(rig.graph().patch_id == "p-one" && !rig.graph().dirty && rig.graph().name == saved.name && rig.graph().modules.size() == saved.modules.size() &&
          rig.graph().connections.size() == saved.connections.size() && find_module(rig.graph(), 3)->parameters[0].value == 3, "loaded graph differs from the saved one");
    other.settle();
    check(other.graph().patch_id == "p-one" && other.graph().modules.size() == saved.modules.size(), "another client did not follow the load");
    // A bundle the engine cannot read: the patch playing comes back, as stored, and the load is reported failed.
    LoadPatch broken{"file:///patches/p-gone/r000001.ingen", blocks, "p-gone"};
    broken.fallback = std::make_shared<const LoadPatch>(LoadPatch{first_bundle, blocks, "p-one", 96});
    outcome = rig.result(broken);
    check(!outcome.accepted && outcome.message.find("restored nothing") != std::string::npos && outcome.message.find("plays again") != std::string::npos,
          "a failed load with a fallback misreported");
    check(rig.graph().patch_id == "p-one" && !rig.graph().dirty && rig.graph().modules.size() == saved.modules.size() &&
          rig.graph().connections.size() == saved.connections.size() && find_module(rig.graph(), 3)->parameters[0].value == 3, "a failed load did not bring the patch before back");
    // Its fallback unreadable too: both fail, and the result says so.
    broken.fallback = std::make_shared<const LoadPatch>(LoadPatch{"file:///patches/p-gone/r000002.ingen", blocks, "p-one"});
    outcome = rig.result(broken);
    check(!outcome.accepted && outcome.message.find("did not come back") != std::string::npos, "a failed fallback went unreported");
    check(rig.result(LoadPatch{first_bundle, blocks, "p-one", 96}).accepted && rig.graph().modules.size() == saved.modules.size(), "reload after failed fallback");
    // A restarted interface knows which stored patch is playing, and whether it has unsaved edits.
    {
        Rig fresh(fake);
        fresh.engine->connect(); fresh.settle();
        check(fresh.graph().patch_id == "p-one" && !fresh.graph().dirty, "restart lost the stored identity");
        check(rig.result(MoveModule{3, 48, 48}).accepted, "move failed");
        fresh.settle();
        check(fresh.graph().dirty, "restarted client did not see the marker change");
    }
    // A plugin the engine lacks: refused before anything is sent, since the engine would load none of it.
    auto with_stranger = blocks;
    with_stranger.push_back({"m40", "urn:test:not-installed", "Stranger"});
    before = fake.requests;
    outcome = rig.result(LoadPatch{first_bundle, with_stranger});
    check(!outcome.accepted && outcome.message.find("Stranger") != std::string::npos && fake.requests == before && rig.graph().dirty, "missing plugin not refused up front");
    // The engine says yes to anything it could not parse; what it did not build is named.
    auto phantom = blocks;
    phantom.push_back({"m41", amp, "Phantom"});
    outcome = rig.result(LoadPatch{first_bundle, phantom});
    check(outcome.accepted && outcome.message.find("did not restore: Phantom") != std::string::npos, "block the engine did not build went unreported");
    // A bundle the engine cannot see at all is a failure, and leaves no identity behind to save over.
    outcome = rig.result(LoadPatch{"file:///patches/nothing/r000001.ingen", blocks});
    check(!outcome.accepted && outcome.message.find("restored nothing") != std::string::npos && rig.graph().modules.size() == 2 && rig.graph().patch_id.empty(),
          "an unreadable bundle was reported as loaded, or kept the previous identity");
    check(rig.result(LoadPatch{first_bundle, blocks}).accepted && rig.graph().patch_id == "p-one", "load after a failed load");
    check(!rig.result(LoadPatch{"file:///patches/x"}).accepted && !rig.result(LoadPatch{"http://example.org/x.ingen"}).accepted, "unsafe load location accepted");
    check(rig.result(SaveGraph{SaveScope::Persistent, true, first_bundle, "p-one"}).accepted, "save after load failed");
    // Control assignments are graph properties: every client sees them, a restart keeps them, a save carries them.
    Assignment knob;
    knob.control = "enc1"; knob.module = 3; knob.parameter = "gain"; knob.minimum = -12; knob.maximum = 12;
    const auto control_key = full(uri::peepo, "control-enc1");
    check(rig.result(Assign{knob}).accepted && rig.graph().assignments.size() == 1 && rig.graph().assignments[0].plugin == amp &&
          property(fake, "ingen:/main", control_key)->text.starts_with("parameter|m3|"), "assignment not stored on the engine's graph");
    other.settle();
    check(other.graph().assignments.size() == 1 && other.graph().assignments[0].module == 3 && other.graph().assignments[0].maximum == 12, "another client did not see the assignment");
    auto too_wide = knob; too_wide.maximum = 99;
    check(!rig.result(Assign{too_wide}).accepted && !rig.result(Assign{Assignment{"enc2", 77, "gain"}}).accepted, "invalid assignment accepted");
    {
        Rig fresh(fake);
        fresh.engine->connect(); fresh.settle();
        check(fresh.graph().assignments.size() == 1 && assignment_target(fresh.graph(), fresh.graph().assignments[0]), "a restarted interface lost the assignment");
    }
    const std::string knob_bundle = "file:///patches/p-knob/r000001.ingen";
    check(rig.result(SaveGraph{SaveScope::Persistent, true, knob_bundle, "p-knob"}).accepted, "save with an assignment failed");
    check(rig.result(NewPatch{"Bare"}).accepted && rig.graph().assignments.empty() && property(fake, "ingen:/main", control_key)->text.empty(), "a new patch kept the assignment");
    check(rig.result(LoadPatch{knob_bundle, blocks, "p-knob"}).accepted && rig.graph().assignments.size() == 1 && assignment_target(rig.graph(), rig.graph().assignments[0]),
          "saved assignment not restored");
    check(rig.result(RemoveModule{3}).accepted && rig.graph().assignments.empty() && !rig.result(Unassign{"enc1"}).accepted, "a removed module kept its assignment");
    // A stomp switching the bypass of two modules: one property, both parts.
    {
        std::vector<InstanceId> plain;
        for (const auto& m : rig.graph().modules) if (!m.fixed) plain.push_back(m.id);
        check(plain.size() >= 2, "the loaded patch has too few modules for a shared stomp");
        Assignment first{"stomp1", plain[0]}, second{"stomp1", plain[1]};
        first.action = second.action = ControlAction::Bypass;
        const auto stomp_key = full(uri::peepo, "control-stomp1");
        check(rig.result(Assign{first, true}).accepted && rig.result(Assign{second, true}).accepted && rig.graph().assignments.size() == 2,
              "a second module did not join the stomp");
        check(assignment_parts(property(fake, "ingen:/main", stomp_key)->text).size() == 2, "the stomp's property does not hold both modules");
        other.settle();
        check(bypass_assignments(other.graph(), "stomp1").size() == 2, "another client did not see both modules on the stomp");
        check(rig.result(Unassign{"stomp1", plain[0]}).accepted && rig.graph().assignments.size() == 1 && rig.graph().assignments[0].module == plain[1],
              "unassigning one module cleared the stomp");
        check(rig.result(Assign{first, true}).accepted && rig.result(RemoveModule{plain[1]}).accepted && rig.graph().assignments.size() == 1 &&
              rig.graph().assignments[0].module == plain[0], "removing one module took the stomp's other module along");
        check(rig.result(Unassign{"stomp1"}).accepted && rig.graph().assignments.empty(), "clearing the stomp left a module on it");
    }

    // Back to the session snapshot for what follows.
    check(rig.result(LoadSession{}).accepted && rig.graph().patch_id.empty() && find_module(rig.graph(), 3)->parameters[0].value == 6.5, "session snapshot lost");
    other.settle();

    // A patch the engine cannot build is refused before anything is deleted.
    before = fake.requests;
    check(!rig.result(LoadGraph{parallel_demo_graph()}).accepted && fake.requests == before && rig.graph().modules.size() == saved.modules.size(), "unbuildable patch destroyed the current one");
    auto cyclic = saved; cyclic.connections.push_back({3, 3, "out", "in"});
    check(!rig.result(LoadGraph{cyclic}).accepted && fake.requests == before, "patch with feedback was sent");

    // An unanswered edit fails, then the graph is read again, not guessed.
    fake.mute = 1;
    const auto silent = engine.submit(SetParameter{3, "gain", 1});
    check(rig.settle(0).empty() && engine.advance(2999).empty(), "edit timed out early");
    events = rig.settle();
    check(!events.empty() && events[0].request == silent && !events[0].accepted && engine.state() == EngineState::Ready, "silent engine was not reported");
    check(find_module(rig.graph(), 3)->parameters[0].value == 6.5 && rig.result(SetParameter{3, "gain", 2}).accepted, "adapter did not recover after a timeout");

    // The engine goes away: pending edits fail, nothing stale is replayed, and
    // the graph comes back from the engine when it does.
    fake.mute = 1;
    const auto lost = engine.submit(SetParameter{3, "gain", 20});
    fake.reachable = false; fake.die();
    events = rig.settle();
    check(events.size() == 2 && events[0].request == lost && !events[0].accepted && events[1].state == EngineState::Connecting, "lost engine did not cancel pending edit");
    check(!rig.result(SetParameter{3, "gain", 21}).accepted && rig.graph().modules.size() == saved.modules.size(), "edit accepted or graph dropped while away");
    check(engine.advance(5000).empty() && engine.state() == EngineState::Connecting, "retrying was noisy or gave up");
    fake.reachable = true; fake.mute = 0;
    engine.advance(1000); events = rig.settle();
    check(engine.state() == EngineState::Ready && rig.graph().modules.size() == saved.modules.size() && find_module(rig.graph(), 3)->parameters[0].value == 2, "graph not recovered after reconnect");
    check(std::count_if(fake.objects.begin(), fake.objects.end(), [](const auto& o) { return has(o.second, rdf_type, full(uri::ingen, "Block")); }) == 3, "reconnect duplicated or lost blocks");

    // Load arrives unasked, outside any reply, and is unknown again when the engine is away.
    // The tempo is the patch's own setting; a load figure arrives only from the engine.
    check(!engine.telemetry().cpu_percent && engine.telemetry().tempo_bpm == rig.graph().tempo, "a reading was invented, or the tempo missing");
    rig.transport->deliver(fake.put(1, "ingen:/engine", {{full(uri::ingen, "meanRunLoad"), number_value(0.07)}}));
    check(rig.settle().empty() && engine.telemetry().cpu_percent && std::abs(*engine.telemetry().cpu_percent - 7) < 1e-4, "engine load misread or reported as a graph change");

    // Unreadable input replaces the connection instead of corrupting the mirror.
    rig.transport->deliver("_:broken a patch:Put ; patch:subject <ingen:/main/m3> ; patch:body [ ingen:canvasX \"1");
    events = rig.settle();
    check(!events.empty() && events[0].state == EngineState::Connecting && events[0].message.find("unreadable") != std::string::npos, "unreadable message was not reported");
    engine.advance(1000); rig.settle();
    check(engine.state() == EngineState::Ready && find_module(rig.graph(), 3)->x == saved.modules[2].x, "adapter did not resynchronize after bad input");

    // An interface restart: a new adapter finds the same modules under the same identities.
    const auto running = rig.graph();
    engine.disconnect();
    check(engine.state() == EngineState::Disconnected && !rig.result(RenamePatch{"Offline"}).accepted && !engine.telemetry().cpu_percent && !engine.telemetry().tempo_bpm,
          "explicit disconnect not honoured");
    check(engine.advance(5000).empty() && engine.state() == EngineState::Disconnected, "disconnected adapter reconnected by itself");
    Rig restarted(fake);
    restarted.engine->connect(); restarted.settle();
    check(restarted.graph().modules.size() == running.modules.size() && restarted.graph().connections.size() == running.connections.size() && restarted.graph().name == running.name,
          "restarted interface read a different graph");
    for (const auto& m : running.modules) check(find_module(restarted.graph(), m.id) && find_module(restarted.graph(), m.id)->name == m.name, "identity changed across restart");
    engine.connect(); rig.settle();
    check(engine.state() == EngineState::Ready && rig.graph().modules.size() == running.modules.size(), "reconnect after explicit disconnect failed");
}
}
// The same engine built against Debian 12's serd writes its paths relative to
// <ingen:/>. Recorded from the arm64 build on 14 October 2026.
void relative_paths() {
    turtle::Reader reader("ingen:/");
    std::vector<turtle::Triple> triples;
    check(reader.parse("@prefix ingen: <http://drobilla.net/ns/ingen#> .\n@prefix patch: <http://lv2plug.in/ns/ext/patch#> .\n@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .\n\n"
                       " _:b35\n\ta patch:Put ;\n\tpatch:sequenceNumber \"6\"^^xsd:int ;\n\tpatch:subject </main/> ;\n\tpatch:body [\n\t\ta ingen:Arc ;\n"
                       "\t\tingen:tail </main/in_l> ;\n\t\tingen:head </main/out_l>\n\t] .\n", triples).empty(), "relative recording unreadable");
    const auto messages = decode(triples);
    check(messages.size() == 1 && messages[0].subject == "ingen:/main" && find(messages[0].body, full(uri::ingen, "head"))->text == "ingen:/main/out_l",
          "relative engine paths were not resolved against <ingen:/>");
    // The other forms a writer can produce, against a base with an authority.
    turtle::Reader web("http://example.org/a/b?q#f");
    triples.clear();
    check(web.parse("<c> <#x> <//other/z> , </root> , <?p> , <> , <./d> , <urn:absolute> .", triples).empty() && triples.size() == 6, "resolution sample unreadable");
    check(triples[0].subject.value == "http://example.org/a/c" && triples[0].predicate.value == "http://example.org/a/b?q#x" && triples[0].object.value == "http://other/z" &&
          triples[1].object.value == "http://example.org/root" && triples[2].object.value == "http://example.org/a/b?p" &&
          triples[3].object.value == "http://example.org/a/b?q" && triples[4].object.value == "http://example.org/a/d" && triples[5].object.value == "urn:absolute",
          "relative references resolved wrongly");
    // Without a base (a bundle's main.ttl), they stay as written.
    turtle::Reader plain;
    triples.clear();
    check(plain.parse("<m3> <p> <m3/gain> .", triples).empty() && triples[0].subject.value == "m3" && triples[0].object.value == "m3/gain", "a baseless reader resolved");

    // A whole session against an engine that writes this way.
    FakeEngine fake;
    fake.relative = true;
    Rig rig(fake);
    rig.engine->connect(); rig.settle();
    check(rig.engine->state() == EngineState::Ready && rig.graph().modules.size() == 2, "relative engine did not become ready");
    check(rig.result(NewPatch{"Relative"}).accepted && rig.graph().connections.size() == 2 && rig.graph().name == "Relative", "relative engine's cables or name not seen");
    check(rig.result(AddModule{amp, 184, 132}).accepted && rig.graph().modules.size() == 3 && rig.graph().modules[2].parameters.size() == 1, "relative engine's block not seen");
    check(rig.result(Connect{{1, 3, "out", "in"}}).accepted && rig.graph().connections.size() == 3, "relative engine's new cable not seen");
    check(rig.result(NewPatch{"Again"}).accepted && rig.graph().modules.size() == 2 && rig.graph().connections.size() == 2, "replacing a patch on a relative engine failed");
}
// A module the catalogue marks "bypass": "spill" carries peepo:spillBypass on
// its block however it got into the engine, so a save always keeps it; the
// bypass itself stays the engine's.
void spill_bypass() {
    ModuleDefinition reverb{spilled, "Spill", "Reverb", "", 0x81c9b8, {{"in", Direction::Input}, {"out", Direction::Output}},
                            {{"gain", "Gain", "dB", ParameterKind::Continuous, -90, 24, 0}}};
    reverb.spill_bypass = true;
    register_definitions({{amp, "Amp", "Utility", "Amplifier", 0x93a4b5, {{"in", Direction::Input}, {"out", Direction::Output}},
                           {{"gain", "Gain", "dB", ParameterKind::Continuous, -90, 24, 0}}}, reverb});
    FakeEngine fake;
    Rig rig(fake);
    rig.engine->connect(); rig.settle();
    const auto mark = full(uri::peepo, "spillBypass");
    const auto marked = [&](InstanceId id) {
        const auto* value = property(fake, "ingen:/main/m" + std::to_string(id), mark);
        return value && (value->kind == Value::Kind::Boolean ? value->boolean : value->number != 0);
    };
    check(rig.result(NewPatch{"Spill"}).accepted && rig.result(AddModule{spilled, 240, 24}).accepted && rig.result(AddModule{amp, 240, 144}).accepted, "modules refused");
    const auto reverb_id = rig.graph().modules[2].id, amp_id = rig.graph().modules[3].id;
    check(marked(reverb_id) && !property(fake, "ingen:/main/m" + std::to_string(amp_id), mark), "the mark is not on the spilling block alone");
    check(rig.result(SetBypass{reverb_id, true}).accepted && find_module(rig.graph(), reverb_id)->bypassed &&
          !property(fake, "ingen:/main/m" + std::to_string(reverb_id), full(uri::ingen, "enabled"))->boolean && marked(reverb_id),
          "a spilling module is not bypassed by the engine, or lost its mark");
    // The session snapshot builds its blocks again, marked.
    check(rig.result(SaveGraph{}).accepted && rig.result(NewPatch{"Empty"}).accepted && rig.result(LoadSession{}).accepted, "snapshot round trip failed");
    check(marked(reverb_id) && find_module(rig.graph(), reverb_id)->bypassed, "the restored block lost its mark or its bypass");
    // A patch saved before the module spilled is marked as it loads.
    const std::string bundle = "file:///patches/p-spill/r000001.ingen";
    check(rig.result(SaveGraph{SaveScope::Persistent, true, bundle, "p-spill"}).accepted, "save failed");
    auto& saved = fake.files.at(bundle).first.at("ingen:/main/m" + std::to_string(reverb_id));
    check(std::erase_if(saved, [&](const auto& e) { return e.first == mark; }) == 1, "the saved patch lacks the mark");
    std::vector<SavedBlock> blocks;
    for (const auto& m : rig.graph().modules) if (!m.fixed) blocks.push_back({"m" + std::to_string(m.id), m.uri, m.name});
    check(rig.result(NewPatch{"Elsewhere"}).accepted, "new patch failed");
    const auto loaded = rig.result(LoadPatch{bundle, blocks, "p-spill"});
    check(loaded.accepted && marked(reverb_id) && !property(fake, "ingen:/main/m" + std::to_string(amp_id), mark) && !rig.graph().dirty,
          "an old patch was not marked as it loaded, or the load left it modified");
}
void tuner_monitor() {
    using namespace peepo;
    using namespace peepo::ingen;
    FakeEngine fake;
    const std::string path = "ingen:/main/m3", plugin = "http://gareus.org/oss/lv2/tuna#mod";
    fake.plugins.insert(plugin);
    fake.objects[path] = {{rdf_type, iri_value(full(uri::ingen, "Block"))}, {full(uri::lv2, "prototype"), iri_value(plugin)}};
    fake.objects[path + "/freq_out"] = {{rdf_type, iri_value(full(uri::lv2, "OutputPort"))},
        {rdf_type, iri_value(full(uri::lv2, "ControlPort"))}, {full(uri::ingen, "value"), number_value(440)}};
    Rig rig(fake);
    auto& engine = *rig.engine;
    engine.connect(); rig.settle();
    check(!engine.tuner_frequency(3), "unwatched tuner exposed a reading");
    engine.watch_tuner(3);
    check(rig.settle().empty() && !engine.tuner_frequency(3), "monitor setup rebuilt the patch or used an initial port value as live pitch");
    const auto* enabled = property(fake, path + "/freq_out", full(uri::ingen, "broadcast"));
    check(enabled && enabled->boolean, "tuner output was not subscribed");
    rig.transport->deliver(fake.put(1, path + "/freq_out", {{full(uri::ingen, "value"), number_value(442)}}));
    check(rig.settle().empty() && engine.tuner_frequency(3) == 442 && !rig.graph().dirty, "pitch update rebuilt/modified the patch");
    rig.transport->deliver(fake.put(1, path + "/freq_out", {{full(uri::ingen, "value"), number_value(0)}}));
    check(rig.settle().empty() && engine.tuner_frequency(3) == 0, "silence retained a note");
    // Get descriptions contain initialization values, not current DSP output.
    rig.transport->deliver(fake.put(1, path + "/freq_out", fake.objects.at(path + "/freq_out")));
    check(rig.settle().empty() && engine.tuner_frequency(3) == 0, "port description overwrote live pitch");
    // Explicit monitoring sends unchanged readings too (engine patch 0006).
    rig.transport->deliver(fake.put(1, path + "/freq_out", {{full(uri::ingen, "value"), number_value(440)}}));
    rig.settle();
    check(engine.tuner_frequency(3) == 440, "live pitch did not refresh");
    fake.mute = 1;
    engine.advance(800); rig.settle();
    check(!engine.tuner_frequency(3), "stalled engine retained a stale pitch");
    fake.mute = 0;
    engine.watch_tuner(std::nullopt); rig.settle();
    check(!engine.tuner_frequency(3), "leaving tuner retained its reading");
    enabled = property(fake, path + "/freq_out", full(uri::ingen, "broadcast"));
    check(enabled && !enabled->boolean, "leaving tuner kept monitoring enabled");
    engine.watch_tuner(3); rig.settle(); engine.disconnect();
    check(!engine.tuner_frequency(3), "offline tuner retained a reading");
}
void euclidean_monitor() {
    using namespace peepo;
    using namespace peepo::ingen;
    FakeEngine fake;
    const std::string path = "ingen:/main/m3", plugin = "http://polyeffects.com/lv2/euclidean";
    fake.plugins.insert(plugin);
    fake.objects[path] = {{rdf_type, iri_value(full(uri::ingen, "Block"))}, {full(uri::lv2, "prototype"), iri_value(plugin)}};
    for (int t = 1; t <= 4; ++t)
        fake.objects[path + "/current_step_out_" + std::to_string(t)] = {{rdf_type, iri_value(full(uri::lv2, "OutputPort"))},
            {rdf_type, iri_value(full(uri::lv2, "ControlPort"))}, {full(uri::ingen, "value"), number_value(0)}};
    Rig rig(fake);
    auto& engine = *rig.engine;
    engine.connect(); rig.settle();
    check(!engine.euclidean_steps(3)[0], "unwatched euclidean exposed a step");
    engine.watch_euclidean(3);
    check(rig.settle().empty() && !engine.euclidean_steps(3)[0], "euclidean monitor setup rebuilt the patch or used initial port value");
    for (int t = 1; t <= 4; ++t) {
        const auto* enabled = property(fake, path + "/current_step_out_" + std::to_string(t), full(uri::ingen, "broadcast"));
        check(enabled && enabled->boolean, "euclidean step output was not subscribed");
    }
    rig.transport->deliver(fake.put(1, path + "/current_step_out_1", {{full(uri::ingen, "value"), number_value(2)}}));
    rig.transport->deliver(fake.put(1, path + "/current_step_out_3", {{full(uri::ingen, "value"), number_value(5)}}));
    check(rig.settle().empty() && engine.euclidean_steps(3)[0] == 2 && !engine.euclidean_steps(3)[1] && engine.euclidean_steps(3)[2] == 5 && !rig.graph().dirty,
          "euclidean step update rebuilt/modified the patch");
    // Get descriptions contain initialization values, not current DSP output.
    rig.transport->deliver(fake.put(1, path + "/current_step_out_1", fake.objects.at(path + "/current_step_out_1")));
    check(rig.settle().empty() && engine.euclidean_steps(3)[0] == 2, "port description overwrote live step");
    fake.mute = 1;
    engine.advance(800); rig.settle();
    check(!engine.euclidean_steps(3)[0] && !engine.euclidean_steps(3)[2], "stalled engine retained a stale step");
    fake.mute = 0;
    engine.watch_euclidean(std::nullopt); rig.settle();
    for (int t = 1; t <= 4; ++t) {
        const auto* enabled = property(fake, path + "/current_step_out_" + std::to_string(t), full(uri::ingen, "broadcast"));
        check(enabled && !enabled->boolean, "leaving euclidean kept monitoring enabled");
    }
    engine.watch_euclidean(3); rig.settle();
    rig.transport->deliver(fake.put(1, path + "/current_step_out_2", {{full(uri::ingen, "value"), number_value(1)}}));
    rig.settle();
    check(engine.euclidean_steps(3)[1] == 1, "re-watched euclidean did not report step");
    engine.disconnect();
    check(!engine.euclidean_steps(3)[1], "offline euclidean retained a step");
}
void looper_monitor() {
    using namespace peepo; using namespace peepo::ingen;
    FakeEngine fake;
    const std::string path = "ingen:/main/m3", plugin = "https://github.com/ABustos2606/loo-pi#loopi";
    fake.plugins.insert(plugin);
    register_definitions({{plugin, "Loo-pi", "Delay", "", 0x7dd3fc, {{"in_l", Direction::Input}, {"out_l", Direction::Output}},
                           {{"loops", "Loops", "", ParameterKind::Integer, 1, 8, 1}}}});
    fake.objects[path] = {{rdf_type, iri_value(full(uri::ingen, "Block"))}, {full(uri::lv2, "prototype"), iri_value(plugin)}};
    for (std::size_t i=0;i<looper_outputs;++i)
        fake.objects[path + "/" + looper_symbol(i)] = {{rdf_type, iri_value(full(uri::lv2,"OutputPort"))},
            {rdf_type, iri_value(full(uri::lv2,"ControlPort"))}, {full(uri::ingen,"value"), number_value(0)}};
    fake.objects[path+"/loops"] = {{full(uri::ingen, "value"), number_value(2)}, {rdf_type, iri_value(full(uri::lv2, "ControlPort"))}, {rdf_type, iri_value(full(uri::lv2, "InputPort"))}};
    fake.objects[path+"/commands"] = {{rdf_type,iri_value(full(uri::lv2,"InputPort"))},{rdf_type,iri_value(full(uri::atom,"AtomPort"))}};
    Rig rig(fake); auto& engine=*rig.engine; engine.connect(); rig.settle();
    engine.watch_looper(3); check(rig.settle().empty(),"looper watch rebuilt graph");
    // Ingen sends every subscribed output 25 times a second: only the two
    // loops that exist, and the three global outputs, are watched.
    const auto subscribed = [&](std::size_t i) {
        const auto* flag=property(fake,path+"/"+looper_symbol(i),full(uri::ingen,"broadcast")); return flag && flag->boolean;
    };
    for(std::size_t i=0;i<looper_outputs;++i) check(subscribed(i) == (i < 12 || i >= 48),"looper outputs of absent loops subscribed, or existing ones not");
    check(!engine.looper_status(3)[0],"initial description masquerades as live output");
    rig.transport->deliver(fake.put(1,path+"/l1_state",{{full(uri::ingen,"value"),number_value(4)}}));
    rig.transport->deliver(fake.put(1,path+"/l2_pos",{{full(uri::ingen,"value"),number_value(1.25)}}));
    check(rig.settle().empty() && engine.looper_status(3)[0]==4 && engine.looper_status(3)[11]==1.25 && !rig.graph().dirty,"looper telemetry rebuilt or modified graph");
    rig.transport->deliver(fake.put(1,path+"/l1_state",fake.objects.at(path+"/l1_state")));
    rig.settle(); check(engine.looper_status(3)[0]==4,"Get overwrote current looper state");
    engine.advance(800);rig.settle();check(engine.looper_status(3)[0]==4,"stable looper telemetry expired despite change-only broadcasts");
    check(engine.looper_command(3,5,3,true),"looper command rejected"); rig.settle();
    const auto* value=property(fake,path+"/commands",full(uri::ingen,"value"));
    check(value && value->number==779,"looper atom Int press/target encoding");
    check(engine.looper_command(3,5,3,false),"looper release rejected"); rig.settle();
    value=property(fake,path+"/commands",full(uri::ingen,"value"));
    check(value && value->number==778 && !rig.graph().dirty,"looper release encoding or saved graph changed");
    check(!engine.looper_command(3,127,0,true) && !engine.looper_command(3,5,10,true) && !engine.looper_command(3,4,0,true),"invalid looper command accepted");
    check(engine.looper_command(3,7,0,true),"Milestone 3 command (Multiply) rejected"); rig.settle();
    // A third loop is watched as it arrives; the values already known stay.
    check(rig.result(SetParameter{3, "loops", 3}).accepted, "loop count not set"); engine.watch_looper(3); rig.settle();
    check(subscribed(12) && subscribed(17) && !subscribed(18) && engine.looper_status(3)[0]==4,"added loop not watched, or known values lost");
    engine.watch_looper(std::nullopt);rig.settle();
    for(std::size_t i=0;i<looper_outputs;++i) check(!subscribed(i),"looper subscription leaked on leaving page");
    engine.disconnect();check(!engine.looper_status(3)[0] && !engine.looper_command(3,5,0,true),"disconnected looper still active");
}
void midi_cables() {
    using namespace peepo;
    using namespace peepo::ingen;
    // An atom port is a MIDI connector when the engine says it supports MIDI
    // events; any other, such as a file input, is a message port.
    const std::string path = "ingen:/main/m3", plugin = "urn:test:chord", midi_event = "http://lv2plug.in/ns/ext/midi#MidiEvent";
    register_definitions({{plugin, "Chord", "MIDI", "Notes to chords", 0x93a4b5,
                           {{"midi_in", Direction::Input, 1, PortType::Midi}, {"midi_out", Direction::Output, 1, PortType::Midi}, {"control", Direction::Input, 1, PortType::Atom}}, {}}});
    FakeEngine fake;
    fake.plugins.insert(plugin);
    fake.objects[path] = {{rdf_type, iri_value(full(uri::ingen, "Block"))}, {full(uri::lv2, "prototype"), iri_value(plugin)}};
    auto atom_port = [&](const char* symbol, int index, const char* direction, const char* supports) {
        fake.objects[path + "/" + symbol] = {{full(uri::lv2, "index"), number_value(index)}, {rdf_type, iri_value(full(uri::atom, "AtomPort"))},
                                             {rdf_type, iri_value(full(uri::lv2, direction))}, {full(uri::atom, "supports"), iri_value(supports)}};
    };
    atom_port("midi_in", 0, "InputPort", midi_event.c_str());
    atom_port("midi_out", 1, "OutputPort", midi_event.c_str());
    atom_port("control", 2, "InputPort", full(uri::patch, "Message").c_str());
    Rig rig(fake);
    auto& engine = *rig.engine;
    engine.connect(); rig.settle();
    const auto* chord = find_module(rig.graph(), 3);
    check(chord && chord->ports.size() == 3 && chord->ports[0].type == PortType::Midi && chord->ports[1].type == PortType::Midi && chord->ports[2].type == PortType::Atom,
          "MIDI and message ports not told apart");
    // IN's merged MIDI through the module to OUT, on the engine's own MIDI ports.
    check(rig.result(Connect{{1, 3, "midi", "midi_in"}}).accepted && rig.result(Connect{{3, 2, "midi_out", "midi"}}).accepted, "MIDI cable refused");
    check(std::find(fake.arcs.begin(), fake.arcs.end(), std::pair<std::string, std::string>{"ingen:/main/midi_in", path + "/midi_in"}) != fake.arcs.end() &&
          std::find(fake.arcs.begin(), fake.arcs.end(), std::pair<std::string, std::string>{path + "/midi_out", "ingen:/main/midi_out"}) != fake.arcs.end(),
          "MIDI cable did not reach the engine's MIDI ports");
    check(rig.graph().connections.size() == 2 && rig.graph().connections[0].from == 1 && rig.graph().connections[0].output == "midi", "MIDI cable not read back");
    const auto before = fake.requests;
    check(!rig.result(Connect{{1, 3, "midi", "control"}}).accepted && !rig.result(Connect{{1, 3, "out", "midi_in"}}).accepted && fake.requests == before,
          "a MIDI cable to a message port or an audio cable to MIDI reached the engine");
}
void midi_learn() {
    // Ingen's own MIDI learn: a port waits, the engine announces what it
    // learned, and the binding is a port property saved with the patch.
    FakeEngine fake;
    const std::string port = "ingen:/main/m3/gain", binding = full(uri::midi, "binding"), modified = full(uri::peepo, "modified");
    fake.create_block("ingen:/main/m3", {{rdf_type, iri_value(full(uri::ingen, "Block"))}, {full(uri::lv2, "prototype"), iri_value(amp)}});
    Rig rig(fake), other(fake);
    rig.engine->connect(); other.engine->connect(); rig.settle(); other.settle();
    auto gain = [](const Rig& r) { return find_module(r.graph(), 3)->parameters.at(0); };
    check(rig.result(LearnMidi{3, "gain"}).accepted && fake.learning == port && !rig.graph().dirty, "learn not started, or marked the patch before learning");
    // Controller 7 on channel 3: Ingen numbers it 2 * 256 + 7.
    fake.learn("Controller", 2 * 256 + 7);
    rig.settle(); other.settle();
    check(gain(rig).midi == MidiBinding{MidiBinding::Kind::Controller, 3, 7} && gain(other).midi == gain(rig).midi, "learned binding not read");
    check(rig.graph().dirty && property(fake, "ingen:/main", modified) && property(fake, "ingen:/main", modified)->boolean, "learning did not mark the patch");
    check(midi_binding_text(*gain(rig).midi) == "CC 7 / 3", "binding text");
    // Learning again replaces it: Ingen would keep both, so the old one goes first.
    check(rig.result(LearnMidi{3, "gain"}).accepted && !property(fake, port, binding) && fake.learning == port, "old binding kept while learning again");
    fake.learn("NoteOn", 60);
    rig.settle();
    check(gain(rig).midi == MidiBinding{MidiBinding::Kind::Note, 1, 60}, "note binding not read");
    check(rig.result(LearnMidi{3, "gain", true}).accepted && !property(fake, port, binding) && !gain(rig).midi, "forget left the binding");
    // A wait ended here cannot be ended in Ingen: what it learns later goes again.
    check(rig.result(LearnMidi{3, "gain"}).accepted && rig.result(LearnMidi{3, "gain", true}).accepted, "learn and cancel refused");
    fake.learn("Controller", 1);
    rig.settle(); rig.settle();
    check(!property(fake, port, binding) && !gain(rig).midi, "a cancelled learn kept its binding");
    check(!rig.result(LearnMidi{3, general_output_symbol}).accepted && !rig.result(LearnMidi{1, "out"}).accepted, "learn on something that is not a port");
    // A rebuilt graph brings its bindings with it.
    auto copy = rig.graph();
    find_module(copy, 3)->parameters.at(0).midi = MidiBinding{MidiBinding::Kind::Bender, 16, 0};
    check(rig.result(LoadGraph{copy}).accepted, "graph with a binding refused");
    const auto* stored = property(fake, port, binding);
    check(stored && stored->kind == Value::Kind::Object && stored->number == 15 && stored->text == full(uri::midi, "Bender"), "binding not given to the engine");
    rig.settle();
    check(gain(rig).midi == MidiBinding{MidiBinding::Kind::Bender, 16, 0}, "binding of a rebuilt graph not read back");
    // A tempo from incoming MIDI clock follows the playing; it does not edit the patch.
    check(rig.result(SaveGraph{SaveScope::Persistent, true, "file:///tmp/learn.ingen", "learn"}).accepted && !rig.graph().dirty, "save");
    check(rig.result(SetTempo{97, true}).accepted && rig.graph().tempo == 97 && !rig.graph().dirty, "following MIDI clock marked the patch");
    check(rig.result(SetTempo{98}).accepted && rig.graph().dirty, "a tempo edit did not mark the patch");
    // The control changes a patch sends as it loads: a graph property, read back, cleared by a new patch.
    const auto on_load = full(uri::peepo, "midiOnLoad");
    check(rig.result(SetLoadMessages{{{1, 7, 100}, {2, 10, 64}}}).accepted && property(fake, "ingen:/main", on_load)->text == "1:7:100 2:10:64", "control changes not given to the engine");
    other.settle();
    check(other.graph().load_messages == std::vector<ControlChange>{{1, 7, 100}, {2, 10, 64}}, "control changes not read by another client");
    check(!rig.result(SetLoadMessages{{{0, 7, 100}}}).accepted, "a control change on channel 0 accepted");
    check(rig.result(NewPatch{"Plain"}).accepted && rig.graph().load_messages.empty() && property(fake, "ingen:/main", on_load)->text.empty(), "a new patch kept the control changes");
}
bool same_scenes(const Scenes& a, const Scenes& b) {
    auto entries = [](const Scene& x, const Scene& y) {
        return x.name == y.name && x.entries.size() == y.entries.size() &&
               std::equal(x.entries.begin(), x.entries.end(), y.entries.begin(), [](const SceneEntry& e, const SceneEntry& f) { return e.target.same(f.target) && e.value == f.value; });
    };
    return a.current == b.current && a.scene_mode == b.scene_mode && a.parameters.size() == b.parameters.size() && a.scenes.size() == b.scenes.size() &&
           std::equal(a.parameters.begin(), a.parameters.end(), b.parameters.begin(), [](const SceneTarget& s, const SceneTarget& t) { return s.same(t); }) &&
           std::equal(a.scenes.begin(), a.scenes.end(), b.scenes.begin(), entries);
}
void scenes() {
    // Scenes are graph properties; selecting one is a single bundle.
    register_definitions({{amp, "Amp", "Utility", "Amplifier", 0x93a4b5, {{"in", Direction::Input}, {"out", Direction::Output}},
                           {{"gain", "Gain", "dB", ParameterKind::Continuous, -90, 24, 0}}}});
    FakeEngine fake;
    Rig rig(fake), other(fake);
    rig.engine->connect(); other.engine->connect(); rig.settle(); other.settle();
    check(rig.result(NewPatch{"Scenes"}).accepted && rig.result(AddModule{amp, 240, 24}).accepted && rig.result(AddModule{amp, 240, 144}).accepted, "modules refused");
    const auto a = rig.graph().modules[2].id, b = rig.graph().modules[3].id;
    const auto pa = "ingen:/main/m" + std::to_string(a), pb = "ingen:/main/m" + std::to_string(b);
    auto scene_key = [](const std::string& local) { return full(uri::peepo, "scene-" + local); };
    auto gain = [](const Rig& r, InstanceId id) { return find_module(r.graph(), id)->parameters.at(0).value; };
    check(!rig.result(SelectScene{0}).accepted && !rig.result(MarkSceneParameter{b, "gain"}).accepted, "a patch without scenes took a scene command");
    check(rig.result(AddScene{}).accepted && rig.graph().scenes.scenes.size() == 1 && property(fake, "ingen:/main", scene_key("count"))->number == 1 &&
          property(fake, "ingen:/main", scene_key("1-name"))->text == "Scene 1", "the first scene is not on the engine's graph");
    check(rig.result(MarkSceneParameter{b, "gain"}).accepted && scene_parameter_marked(rig.graph(), b, "gain") &&
          property(fake, "ingen:/main", scene_key("params"))->text == "m" + std::to_string(b) + "|" + amp + "|gain", "parameter not marked per scene");
    check(rig.result(AddScene{}).accepted && rig.graph().scenes.current == 1, "second scene");
    // Edits on the screen go into the current scene; a performance change does not.
    check(rig.result(SetBypass{a, true, true}).accepted && rig.result(SetParameter{b, "gain", -12, true}).accepted, "screen edits refused");
    check(scene_changes(rig.graph(), 1).empty() && (scene_changes(rig.graph(), 0) == std::vector<SceneChange>{{a, "", 0}, {b, "gain", 0}}),
          "screen edits not written into the current scene");
    check(rig.result(SetParameter{b, "gain", 6}).accepted && (scene_changes(rig.graph(), 1) == std::vector<SceneChange>{{b, "gain", -12}}),
          "a performance change was written into the scene");
    const std::string saved = "file:///patches/p-scenes/r000001.ingen";
    check(rig.result(SaveGraph{SaveScope::Persistent, true, saved, "p-scenes"}).accepted && !rig.graph().dirty, "save failed");
    // Selecting: the changes and the current scene in one bundle, and the patch not marked.
    fake.bundles.clear();
    check(rig.result(SelectScene{0}).accepted, "scene not selected");
    check((fake.bundles == std::vector<std::vector<std::string>>{{pa + " " + full(uri::ingen, "enabled"), pb + "/gain " + full(uri::ingen, "value"),
                                                                  "ingen:/main " + scene_key("current")}}),
          "the scene's changes were not sent as one bundle");
    check(!find_module(rig.graph(), a)->bypassed && gain(rig, b) == 0 && rig.graph().scenes.current == 0, "the scene was not applied");
    check(!rig.graph().dirty && !property(fake, "ingen:/main", full(uri::peepo, "modified"))->boolean, "selecting a scene marked the patch");
    other.settle();
    check(other.graph().scenes.current == 0 && !find_module(other.graph(), a)->bypassed && gain(other, b) == 0 && same_scenes(other.graph().scenes, rig.graph().scenes),
          "a second client did not see the scene change");
    // Selecting the current scene again brings back its saved state; with nothing to change, nothing is sent.
    check(rig.result(SetBypass{a, true}).accepted && rig.result(SelectScene{0}).accepted && !find_module(rig.graph(), a)->bypassed, "the scene was not restored");
    auto before = fake.requests;
    check(rig.result(SelectScene{0}).accepted && fake.requests == before && !rig.result(SelectScene{5}).accepted && fake.requests == before, "a needless or missing scene was sent");
    // Picked one after another before the engine answers: the last one plays.
    rig.engine->submit(SelectScene{1}); rig.engine->submit(SelectScene{0}); rig.settle();
    check(rig.graph().scenes.current == 0 && gain(rig, b) == 0, "a scene picked straight after another did not play");
    // Two screen edits in flight together both go into the current scene.
    rig.engine->submit(SetBypass{a, true, true}); rig.engine->submit(SetParameter{b, "gain", -6, true}); rig.settle();
    check(find_module(rig.graph(), a)->bypassed && gain(rig, b) == -6 && scene_changes(rig.graph(), 0).empty(), "an edit in flight was lost from the scene");
    check(rig.result(SetBypass{a, false, true}).accepted && rig.result(SetParameter{b, "gain", 0, true}).accepted && scene_changes(rig.graph(), 0).empty(), "scene edits not undone");
    // Rename, reorder, copy, delete.
    check(rig.result(RenameScene{0, "Verse"}).accepted && rig.result(RenameScene{1, "Solo"}).accepted && property(fake, "ingen:/main", scene_key("1-name"))->text == "Verse",
          "rename not stored");
    check(!rig.result(RenameScene{0, "Far too long a name"}).accepted && !rig.result(RenameScene{4, "Gone"}).accepted, "a bad rename accepted");
    check(rig.result(MoveScene{1, 0}).accepted && rig.graph().scenes.scenes[0].name == "Solo" && rig.graph().scenes.current == 1 &&
          property(fake, "ingen:/main", scene_key("current"))->number == 2, "move lost the current scene");
    check(rig.result(AddScene{}).accepted && rig.result(CopyScene{0, 2}).accepted && rig.graph().scenes.scenes[2].entries == rig.graph().scenes.scenes[0].entries &&
          rig.graph().scenes.scenes[2].name == "Scene 1", "copy");
    check(!rig.result(DeleteScene{2}).accepted && rig.result(SelectScene{1}).accepted && rig.result(DeleteScene{2}).accepted && rig.graph().scenes.scenes.size() == 2 &&
          property(fake, "ingen:/main", scene_key("count"))->number == 2 && property(fake, "ingen:/main", scene_key("3-name"))->text.empty() &&
          property(fake, "ingen:/main", scene_key("3"))->text.empty(), "delete left the scene on the engine's graph");
    // A new module joins every scene, a copy follows its original, a removed one leaves.
    auto in_every = [&](InstanceId id) {
        const auto& all = rig.graph().scenes.scenes;
        return std::all_of(all.begin(), all.end(), [&](const Scene& s) { return std::any_of(s.entries.begin(), s.entries.end(), [&](const SceneEntry& e) { return e.target.module == id; }); });
    };
    check(rig.result(AddModule{amp, 400, 24}).accepted && in_every(rig.graph().modules.back().id), "a new module is not in every scene");
    const auto added = rig.graph().modules.back().id;
    check(rig.result(DuplicateModule{b, 400, 144}).accepted && in_every(rig.graph().modules.back().id) && scene_parameter_marked(rig.graph(), rig.graph().modules.back().id, "gain"),
          "a copy did not follow its original");
    check(rig.result(RemoveModule{added}).accepted && !in_every(added) && validate_graph(rig.graph()).empty(), "a removed module stayed in the scenes");
    check(rig.result(MarkSceneParameter{b, "gain", false}).accepted && !scene_parameter_marked(rig.graph(), b, "gain") && rig.result(MarkSceneParameter{b, "gain"}).accepted,
          "unmark");
    // Scene mode is the patch's, on the engine's graph like the scenes.
    check(rig.result(SetSceneMode{true}).accepted && rig.graph().scenes.scene_mode && property(fake, "ingen:/main", scene_key("mode"))->number == 1,
          "scene mode is not on the engine's graph");
    other.settle();
    check(same_scenes(other.graph().scenes, rig.graph().scenes), "a second client's scenes differ");
    const auto kept = rig.graph().scenes;
    {
        Rig fresh(fake);
        fresh.engine->connect(); fresh.settle();
        check(same_scenes(fresh.graph().scenes, kept) && fresh.graph().scenes.scenes[0].name == "Solo", "a restarted interface lost the scenes");
    }
    // The session snapshot holds them; a new patch has none.
    check(rig.result(SaveGraph{}).accepted && rig.result(NewPatch{"Plain"}).accepted && rig.graph().scenes.scenes.empty() &&
          property(fake, "ingen:/main", scene_key("count"))->number == 0 && property(fake, "ingen:/main", scene_key("1"))->text.empty(), "a new patch kept the scenes");
    check(rig.result(LoadSession{}).accepted && same_scenes(rig.graph().scenes, kept), "the session snapshot lost the scenes");
    // Saved and loaded with the patch: the current scene is the one saved, its values the saved ones.
    check(rig.result(SelectScene{0}).accepted && rig.result(SaveGraph{SaveScope::Persistent, true, saved, "p-scenes"}).accepted, "save failed");
    const auto playing = rig.graph();
    check(find(fake.files.at(saved).first.at("ingen:/main"), scene_key("count"))->number == 2, "the bundle lacks its scenes");
    std::vector<SavedBlock> blocks;
    for (const auto& m : playing.modules) if (!m.fixed) blocks.push_back({"m" + std::to_string(m.id), m.uri, m.name});
    check(rig.result(NewPatch{"Elsewhere"}).accepted && rig.result(LoadPatch{saved, blocks, "p-scenes"}).accepted, "load failed");
    check(same_scenes(rig.graph().scenes, playing.scenes) && rig.graph().scenes.current == 0 && scene_changes(rig.graph(), 0).empty() && !rig.graph().dirty,
          "the load did not restore the current scene");
}
int main() {
    try {
        turtle_and_protocol();
        adapter();
        relative_paths();
        spill_bypass();
        midi_cables();
        midi_learn();
        scenes();
        tuner_monitor();
        euclidean_monitor();
        looper_monitor();
        std::cout << "Ingen protocol reading, command mapping, acknowledgment, second client, scenes, timeout, reconnect and restart passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
