// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// The Ingen adapter against a running engine. Needs the LV2 example amplifier
// and replaces whatever patch the engine holds:
//   peepo_ingen_live unix:///tmp/peepo-ingen.sock catalogue.json
// It leaves IN (left) -> amp at +6.02 dB -> OUT (left) for a level measurement.
// With --survive-restart it instead waits for the engine to be stopped and
// started again, and reports whether the adapter found its way back.
#include "catalogue/catalogue.hpp"
#include "engine/ingen/ingen_engine.hpp"
#include "storage/patch_store.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

using namespace peepo;
namespace {
using Clock = std::chrono::steady_clock;
const std::string amp = "http://lv2plug.in/plugins/eg-amp";
int failures = 0;
bool check(bool ok, const std::string& name, const std::string& detail = {}) {
    std::cout << (ok ? "PASS  " : "FAIL  ") << name << (detail.empty() ? "" : " -- " + detail) << std::endl;
    if (!ok) ++failures;
    return ok;
}
struct Client {
    explicit Client(const std::string& address) : engine(ingen::Options{address}) {}
    std::vector<EngineEvent> pump(unsigned milliseconds) {
        std::vector<EngineEvent> all;
        const auto until = Clock::now() + std::chrono::milliseconds(milliseconds);
        while (Clock::now() < until) {
            for (auto& event : step()) all.push_back(std::move(event));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return all;
    }
    std::vector<EngineEvent> step() {
        const auto now = Clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
        if (elapsed > 0) last = now;
        return engine.advance(static_cast<unsigned>(elapsed));
    }
    bool ready(unsigned milliseconds = 5000) {
        const auto until = Clock::now() + std::chrono::milliseconds(milliseconds);
        while (engine.state() != EngineState::Ready && Clock::now() < until) { step(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        return engine.state() == EngineState::Ready;
    }
    EngineEvent result(Command command, double* took = nullptr) {
        const auto start = Clock::now();
        const auto id = engine.submit(std::move(command));
        while (Clock::now() - start < std::chrono::seconds(6)) {
            for (auto& event : step()) if (event.request == id) {
                if (took) *took = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
                return event;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        return {id, false, "no result within six seconds", engine.state(), {}, {}};
    }
    bool ok(Command command, const std::string& name) {
        const auto event = result(std::move(command));
        return check(event.accepted, name, event.accepted ? "" : event.message);
    }
    const Graph& graph() const { return engine.snapshot(); }
    IngenEngine engine;
    Clock::time_point last = Clock::now();
};
bool has_cable(const Graph& graph, const Connection& cable) { return std::find(graph.connections.begin(), graph.connections.end(), cable) != graph.connections.end(); }
double gain_of(const Graph& graph, InstanceId id) { const auto* m = find_module(graph, id); return m && !m->parameters.empty() ? m->parameters[0].value : std::nan(""); }
}
int main(int argc, char** argv) {
    const std::string address = argc > 1 ? argv[1] : "unix:///tmp/peepo-ingen.sock";
    // The catalogue exported from this engine's plugins, not a hand-written entry.
    std::vector<ModuleDefinition> offered;
    if (const auto issue = load_catalogue(argc > 2 ? argv[2] : "catalogue.json", "", offered); !issue.empty()) { std::cerr << issue << std::endl; return 2; }
    replace_catalogue(std::move(offered));
    auto a = std::make_unique<Client>(address);
    a->engine.connect();
    if (!check(a->ready(), "connect, read the graph and create the audio endpoints", address)) return 1;
    if (argc > 3 && std::string(argv[3]) == "--survive-restart") {
        a->ok(NewPatch{"Before restart"}, "edit before the engine goes away");
        std::cout << "WAITING for the engine to stop" << std::endl;
        std::vector<EngineEvent> seen;
        const auto until = Clock::now() + std::chrono::seconds(20);
        while (a->engine.state() == EngineState::Ready && Clock::now() < until) for (auto& e : a->pump(20)) seen.push_back(std::move(e));
        check(a->engine.state() == EngineState::Connecting && !seen.empty() && !seen.back().message.empty(), "a stopped engine is noticed and reported",
              seen.empty() ? "" : seen.back().message);
        check(!a->result(RenamePatch{"While away"}).accepted && a->graph().name == "Before restart", "edits are refused and the last graph kept while the engine is away");
        check(a->ready(20000), "the adapter reconnects by itself when the engine returns");
        check(a->graph().name == "Untitled" && a->graph().connections.empty(), "the graph shown is the new engine's, not the old one remembered");
        a->ok(RenamePatch{"After restart"}, "edits work again");
        std::cout << "SUMMARY " << (failures ? std::to_string(failures) + " failed" : std::string("engine restart survived")) << std::endl;
        return failures ? 1 : 0;
    }
    // For the MIDI clock check: set the tempo as the Tempo page or a tap does;
    // save a patch at one tempo and load it later; or only stay connected, as
    // an interface does, until killed.
    const std::string mode = argc > 3 ? argv[3] : "";
    if (mode == "--set-tempo" && argc > 4) return a->ok(SetTempo{std::stod(argv[4])}, "set the tempo to " + std::string(argv[4]) + " BPM") ? 0 : 1;
    if (mode == "--wait") { for (;;) a->pump(1000); }
    if (mode == "--save-tempo-patch" || mode == "--load-tempo-patch") {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "peepo-live-patches" / "tempo";
        if (mode == "--save-tempo-patch" && argc > 5) {
            std::error_code cleared;
            std::filesystem::remove_all(root, cleared);
            PatchStore store(root);
            if (!check(store.open().empty(), "patch store opens")) return 1;
            a->ok(SetTempo{std::stod(argv[4])}, "the patch to load has its own tempo");
            const auto id = store.new_id();
            std::string issue;
            auto staging = store.stage(id, issue);
            check(a->result(SaveGraph{SaveScope::Persistent, true, staging->location, id}).accepted && store.publish(*staging).empty(), "save it");
            a->ok(SetTempo{std::stod(argv[5])}, "then play another tempo");
            return failures ? 1 : 0;
        }
        PatchStore store(root);
        if (!check(store.open().empty() && !store.list().empty(), "the saved patch is there")) return 1;
        const auto stored = store.list().front();
        const auto event = a->result(LoadPatch{stored.location, saved_blocks(stored.bundle), stored.id, stored.bundle.tempo});
        return check(event.accepted, "load it", event.message) ? 0 : 1;
    }
    // For the MIDI settings check: what plays now; stored patches for program
    // change; and MIDI learn, waiting for a probe to move a controller.
    if (mode == "--state") {
        std::cout << "STATE name=" << a->graph().name << " tempo=" << a->graph().tempo << " dirty=" << a->graph().dirty << std::endl;
        return 0;
    }
    if (mode == "--midi-patches" && argc > 4) {
        std::error_code cleared;
        std::filesystem::remove_all(argv[4], cleared);
        PatchStore store(argv[4]);
        if (!check(store.open().empty(), "patch store opens")) return 1;
        for (const auto& [name, bpm] : {std::pair{"Alpha", 90.0}, std::pair{"Bravo", 100.0}, std::pair{"Charlie", 110.0}}) {
            a->ok(NewPatch{name}, std::string("new patch ") + name);
            a->ok(SetTempo{bpm}, "at " + std::to_string(static_cast<int>(bpm)) + " BPM");
            // Bravo sends two control changes as it loads.
            if (std::string(name) == "Bravo") a->ok(SetLoadMessages{{{1, 7, 100}, {2, 10, 64}}}, "with two control changes on load");
            const auto id = store.new_id();
            std::string issue;
            auto staging = store.stage(id, issue);
            check(staging && a->result(SaveGraph{SaveScope::Persistent, true, staging->location, id}).accepted && store.publish(*staging).empty(), std::string("store ") + name);
        }
        a->ok(NewPatch{"Scratch"}, "then play an unsaved patch");
        return failures ? 1 : 0;
    }
    if (mode == "--midi-learn") {
        a->ok(NewPatch{"Learn"}, "new patch");
        // The first module with a continuous control that this engine has.
        const ModuleDefinition* known = nullptr;
        for (const auto& d : catalogue())
            if (d.availability == Availability::Available && d.category != "Endpoint" &&
                std::any_of(d.parameters.begin(), d.parameters.end(), [](const Parameter& p) { return p.kind == ParameterKind::Continuous && p.symbol != general_output_symbol; }) &&
                a->result(AddModule{d.uri, 300, 200}).accepted) { known = &d; break; }
        if (!check(known, "add a module with a continuous control", known ? known->name : "")) return 1;
        const auto& m = a->graph().modules.back();
        const auto symbol = std::find_if(m.parameters.begin(), m.parameters.end(), [](const Parameter& p) { return p.kind == ParameterKind::Continuous && p.symbol != general_output_symbol; })->symbol;
        a->ok(LearnMidi{m.id, symbol}, "learn " + symbol);
        std::cout << "LEARNING " << symbol << std::endl;
        auto bound = [&] { const auto* now = find_module(a->graph(), m.id); for (const auto& p : now->parameters) if (p.symbol == symbol) return p.midi; return std::optional<MidiBinding>{}; };
        const auto until = Clock::now() + std::chrono::seconds(10);
        while (!bound() && Clock::now() < until) a->pump(20);
        if (!check(bound().has_value(), "a controller moved on a device is learned", bound() ? midi_binding_text(*bound()) : "nothing learned")) return 1;
        check(a->graph().dirty, "learning marks the patch");
        std::cout << "LEARNED " << midi_binding_text(*bound()) << std::endl;
        // The probe moves it again; the engine sets the control.
        const auto value = [&] { for (const auto& p : find_module(a->graph(), m.id)->parameters) if (p.symbol == symbol) return normalized_value(p); return -1.0; };
        const auto start = value();
        const auto moved = Clock::now() + std::chrono::seconds(5);
        while (std::abs(value() - start) < 0.2 && Clock::now() < moved) a->pump(20);
        check(std::abs(value() - start) >= 0.2, "the learned controller sets the control", std::to_string(start) + " -> " + std::to_string(value()));
        // Saved and loaded again, the binding is still there.
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "peepo-live-patches" / "learn";
        std::error_code cleared;
        std::filesystem::remove_all(root, cleared);
        PatchStore store(root);
        store.open();
        const auto id = store.new_id();
        std::string issue;
        auto staging = store.stage(id, issue);
        check(staging && a->result(SaveGraph{SaveScope::Persistent, true, staging->location, id}).accepted && store.publish(*staging).empty(), "save the patch");
        a->ok(NewPatch{"Empty"}, "play another patch");
        const auto stored = store.find(id);
        const auto loaded = a->result(LoadPatch{stored->location, saved_blocks(stored->bundle), stored->id, stored->bundle.tempo});
        check(loaded.accepted, "load it again", loaded.message);
        const auto* again = a->graph().modules.size() > 2 ? &a->graph().modules.back() : nullptr;
        const auto restored = again ? std::find_if(again->parameters.begin(), again->parameters.end(), [&](const Parameter& p) { return p.symbol == symbol; }) : std::vector<Parameter>::const_iterator{};
        check(again && restored != again->parameters.end() && restored->midi == bound(), "the binding is saved with the patch", again && restored != again->parameters.end() && restored->midi ? midi_binding_text(*restored->midi) : "none");
        a->ok(LearnMidi{again->id, symbol, true}, "forget it");
        check(!find_module(a->graph(), again->id)->parameters.empty() && std::none_of(find_module(a->graph(), again->id)->parameters.begin(), find_module(a->graph(), again->id)->parameters.end(), [](const Parameter& p) { return p.midi.has_value(); }), "forgotten");
        std::cout << "SUMMARY " << (failures ? std::to_string(failures) + " failed" : std::string("MIDI learn passed")) << std::endl;
        return failures ? 1 : 0;
    }
    if (argc > 3 && std::string(argv[3]) == "--midi") {
        // MIDI IN -> x42 MIDI Transpose (+12) -> MIDI OUT, saved and loaded
        // again, then left for a JACK MIDI probe to play through.
        const std::string transpose = "http://gareus.org/oss/lv2/midifilter#miditranspose";
        auto* known = const_cast<ModuleDefinition*>(definition(transpose));
        if (!check(known, "the catalogue lists x42 MIDI Transpose")) return 1;
        check(std::count_if(known->ports.begin(), known->ports.end(), [](const Port& p) { return p.type == PortType::Midi; }) == 2,
              "the export calls its two atom ports MIDI connectors");
        // Not offered until it is heard with a controller; this check is that hearing.
        known->availability = Availability::Available;
        check(validate_graph(a->graph()).empty() && a->engine.state() == EngineState::Ready, "the engine took the MIDI endpoints");
        a->ok(NewPatch{"MIDI check"}, "new patch");
        auto event = a->result(AddModule{transpose, 184, 132});
        const auto shift = a->graph().modules.back();
        check(event.accepted && shift.uri == transpose, "add MIDI Transpose", event.message);
        const auto midi_ports = std::count_if(shift.ports.begin(), shift.ports.end(), [](const Port& p) { return p.type == PortType::Midi; });
        check(midi_ports == 2, "the engine's description of it has two MIDI connectors", std::to_string(midi_ports));
        a->ok(SetParameter{shift.id, "transpose", 12}, "transpose up an octave");
        a->ok(Connect{{1, shift.id, "midi", "midiin"}}, "cable MIDI IN to it");
        a->ok(Connect{{shift.id, 2, "midiout", "midi"}}, "cable it to MIDI OUT");
        check(!a->result(Connect{{1, shift.id, "out", "midiin"}}).accepted && !a->result(Connect{{shift.id, 2, "midiout", "in"}}).accepted, "audio and MIDI are not joined");
        check(has_cable(a->graph(), {1, shift.id, "midi", "midiin"}) && has_cable(a->graph(), {shift.id, 2, "midiout", "midi"}), "MIDI cables read back from the engine");
        const char* folder = std::getenv("PEEPO_PATCHES");
        const std::filesystem::path root = (folder ? std::filesystem::path(folder) : std::filesystem::temp_directory_path() / "peepo-live-patches") / "midi";
        std::error_code cleared;
        std::filesystem::remove_all(root, cleared);
#ifdef _WIN32
        PatchStore store(root, wsl_path(root));
#else
        PatchStore store(root);
#endif
        check(store.open().empty(), "patch store opens", root.string());
        const auto id = store.new_id();
        std::string issue;
        auto staging = store.stage(id, issue);
        event = a->result(SaveGraph{SaveScope::Persistent, true, staging->location, id});
        check(event.accepted && store.publish(*staging).empty(), "the engine saves the patch with its MIDI cables", event.message);
        const auto stored = store.find(id);
        check(stored && stored->bundle.arcs.size() == a->graph().connections.size(), "the stored patch lists its MIDI cables",
              stored ? std::to_string(stored->bundle.arcs.size()) + " cables" : "missing");
        a->ok(NewPatch{"Empty"}, "replace it with an empty patch");
        check(!has_cable(a->graph(), {1, 2, "midi", "midi"}) && a->graph().modules.size() == 2, "the empty patch has no MIDI cable");
        event = a->result(LoadPatch{stored->location, saved_blocks(stored->bundle), id});
        const auto* back = find_module(a->graph(), shift.id);
        const auto value = back ? std::find_if(back->parameters.begin(), back->parameters.end(), [](const Parameter& p) { return p.symbol == "transpose"; }) : std::vector<Parameter>::const_iterator{};
        check(event.accepted && back && value != back->parameters.end() && value->value == 12 && has_cable(a->graph(), {1, shift.id, "midi", "midiin"}) &&
              has_cable(a->graph(), {shift.id, 2, "midiout", "midi"}), "the patch loads with its MIDI cables and value", event.message);
        // A second client, as a restarted interface would be, reads the same.
        Client b(address);
        b.engine.connect();
        check(b.ready() && has_cable(b.graph(), {1, shift.id, "midi", "midiin"}) && has_cable(b.graph(), {shift.id, 2, "midiout", "midi"}), "a second client reads the MIDI cables");
        std::cout << "SUMMARY " << (failures ? std::to_string(failures) + " failed" : std::string("MIDI cable checks passed")) << std::endl;
        return failures ? 1 : 0;
    }

    a->ok(NewPatch{"Live check"}, "new patch replaces what the engine held");
    check(a->graph().modules.size() == 2 && a->graph().connections.size() == 2 && a->graph().name == "Live check", "new patch is IN wired to OUT on both sides");
    auto event = a->result(AddModule{amp, 184, 132});
    check(event.accepted && a->graph().modules.size() == 3, "add a plugin", event.message);
    if (a->graph().modules.size() < 3) return 1;
    const auto first = a->graph().modules[2];
    check(first.uri == amp && first.x == 184 && first.y == 132 && first.ports.size() == 2 && first.parameters.size() == 1 && first.parameters[0].value == 0,
          "the new module carries the engine's ports and value", "id " + std::to_string(first.id) + ", " + std::to_string(first.ports.size()) + " ports");
    check(!a->result(AddModule{"urn:peepo:demo:drive", 24, 24}).accepted, "a plugin the engine lacks is refused");
    a->ok(Disconnect{{1, 2, "out", "in"}}, "remove the direct left cable");
    a->ok(Connect{{1, first.id, "out", "in"}}, "cable IN to the module");
    a->ok(Connect{{first.id, 2, "out", "in"}}, "cable the module to OUT");
    check(!a->result(Connect{{first.id, first.id, "out", "in"}}).accepted, "feedback is refused before it reaches the engine");
    check(a->graph().connections.size() == 3 && has_cable(a->graph(), {1, first.id, "out", "in"}) && has_cable(a->graph(), {1, 2, "out_r", "in_r"}), "cables read back from the engine");
    a->ok(SetParameter{first.id, "gain", 6.0206}, "set a value");
    check(std::abs(gain_of(a->graph(), first.id) - 6.0206) < 1e-6 && !a->result(SetParameter{first.id, "gain", 9999}).accepted, "value applied; out-of-range value refused");
    a->ok(SetBypass{first.id, true}, "bypass");
    a->ok(MoveModule{first.id, 312, 144}, "move");
    event = a->result(DuplicateModule{first.id, 456, 144});
    const auto copy = a->graph().modules.back();
    check(event.accepted && a->graph().modules.size() == 4 && copy.id != first.id && copy.bypassed && std::abs(copy.parameters[0].value - 6.0206) < 1e-4 && copy.x == 456 && copy.ports.size() == 2,
          "duplicate keeps value and bypass at the new position", event.message);
    const std::string awkward = "Live \"check\" \xc3\xa9 .";
    a->ok(RenamePatch{awkward}, "rename the patch");

    // A second client reads the same graph and each hears the other.
    Client b(address);
    b.engine.connect();
    check(b.ready() && b.graph().modules.size() == 4 && b.graph().name == awkward && b.graph().connections.size() == 3, "a second client reads the same graph");
    const auto* seen = find_module(b.graph(), first.id);
    check(seen && seen->bypassed && seen->x == 312 && seen->y == 144 && std::abs(gain_of(b.graph(), first.id) - 6.0206) < 1e-4, "values, bypass and positions were in the engine, not only in the first client");
    b.ok(SetParameter{copy.id, "gain", -12}, "second client sets a value");
    a->pump(100);
    check(std::abs(gain_of(a->graph(), copy.id) + 12) < 1e-6, "the first client hears the second client's change");
    b.ok(RemoveModule{copy.id}, "second client removes a module");
    a->pump(100);
    check(a->graph().modules.size() == 3 && !find_module(a->graph(), copy.id), "the first client sees the removal");

    // Session snapshot, then rebuild the whole patch through the engine.
    a->ok(SetBypass{first.id, false}, "bypass off");
    a->ok(SaveGraph{}, "session snapshot");
    const auto saved = a->graph();
    a->ok(NewPatch{"Scratch"}, "new patch");
    check(a->graph().modules.size() == 2, "engine graph emptied");
    event = a->result(LoadSession{});
    const auto* restored = find_module(a->graph(), first.id);
    check(event.accepted && restored && a->graph().name == awkward && a->graph().connections.size() == saved.connections.size() && restored->x == 312 && !restored->bypassed &&
          std::abs(restored->parameters[0].value - 6.0206) < 1e-6, "session snapshot rebuilt in the engine", event.message);
    b.pump(150);
    check(b.graph().modules.size() == 3 && std::abs(gain_of(b.graph(), first.id) - 6.0206) < 1e-4 && b.graph().connections.size() == saved.connections.size(),
          "the second client followed the rebuild, including values it was never sent directly");

    // Patches on disk: the engine writes the bundle, the store publishes it, and
    // a load rebuilds the graph from it. From Windows the engine is in WSL and
    // sees the same folder under /mnt/<drive>.
    {
        const char* folder = std::getenv("PEEPO_PATCHES");
        const std::filesystem::path root = folder ? std::filesystem::path(folder) : std::filesystem::temp_directory_path() / "peepo-live-patches";
        std::error_code cleared;
        std::filesystem::remove_all(root, cleared);
        check(!cleared, "the patch folder from an earlier run can be cleared", root.string() + (cleared ? ": " + cleared.message() : ""));
#ifdef _WIN32
        PatchStore store(root, wsl_path(root));
#else
        PatchStore store(root);
#endif
        check(store.open().empty(), "patch store opens", root.string());
        const std::string granular = "http://polyeffects.com/lv2/polyclouds#granular";
        event = a->result(AddModule{granular, 456, 264});
        const auto mist = a->graph().modules.back();
        check(event.accepted && mist.uri == granular && !mist.parameters.empty(), "add a Mutable-derived module to the patch", event.message);
        const auto& knob = mist.parameters.front();
        const double turned = knob.minimum + (knob.maximum - knob.minimum) * 0.75;
        a->ok(SetParameter{mist.id, knob.symbol, turned}, "turn one of its controls");
        Assignment pinned_knob;
        pinned_knob.control = "enc1"; pinned_knob.module = mist.id; pinned_knob.parameter = knob.symbol; pinned_knob.minimum = knob.minimum; pinned_knob.maximum = knob.maximum;
        pinned_knob.inverted = true;
        a->ok(Assign{pinned_knob}, "pin encoder 1 to it");
        b.pump(150);
        check(a->graph().dirty && b.graph().dirty, "the unsaved-changes marker is in the engine: both clients see it");
        const auto id = store.new_id();
        std::string issue;
        auto staging = store.stage(id, issue);
        double took = 0;
        event = a->result(SaveGraph{SaveScope::Persistent, true, staging->location, id}, &took);
        check(event.accepted && !a->graph().dirty && a->graph().patch_id == id, "the engine saves the patch to disk", std::to_string(took) + " ms; " + event.message);
        check(store.publish(*staging).empty(), "what it wrote reads back with the right identity and is published");
        const auto stored = store.find(id);
        check(stored && stored->revision == 1 && stored->name == awkward && stored->bundle.blocks.size() == 2, "the stored patch lists its name and modules",
              stored ? std::to_string(stored->bundle.blocks.size()) + " blocks" : "missing");
        const auto before_load = a->graph();
        a->ok(NewPatch{"Scratch 2"}, "replace it with an empty patch");
        event = a->result(LoadPatch{stored->location, saved_blocks(stored->bundle)}, &took);
        check(event.accepted && event.message.find("did not restore") == std::string::npos, "load the stored patch", std::to_string(took) + " ms; " + event.message);
        const auto* back = find_module(a->graph(), mist.id);
        const auto* turned_back = back ? &*std::find_if(back->parameters.begin(), back->parameters.end(), [&](const Parameter& p) { return p.symbol == knob.symbol; }) : nullptr;
        check(a->graph().patch_id == id && !a->graph().dirty && a->graph().name == awkward && a->graph().modules.size() == before_load.modules.size() &&
              a->graph().connections.size() == before_load.connections.size() && std::abs(gain_of(a->graph(), first.id) - 6.0206) < 1e-4 &&
              back && back->x == 456 && back->y == 264 && turned_back && std::abs(turned_back->value - turned) < 1e-3,
              "identity, name, modules, cables, values and positions came back from disk");
        check(a->graph().assignments.size() == 1 && a->graph().assignments[0].inverted && assignment_target(a->graph(), a->graph().assignments[0]) &&
              a->graph().assignments[0].module == mist.id, "the encoder assignment came back from disk");
        b.pump(200);
        check(b.graph().patch_id == id && b.graph().modules.size() == before_load.modules.size() && !b.graph().dirty, "the second client followed the load");
        a->ok(MoveModule{mist.id, 600, 264}, "edit after loading");
        staging = store.stage(id, issue);
        check(a->result(SaveGraph{SaveScope::Persistent, true, staging->location, id}).accepted && store.publish(*staging).empty() && store.find(id)->revision == 2,
              "a second save becomes revision 2");
        auto stranger = saved_blocks(stored->bundle);
        stranger.push_back({"m90", "urn:peepo:not-installed", "Stranger"});
        check(!a->result(LoadPatch{stored->location, stranger}).accepted && a->graph().patch_id == id, "a patch needing a missing plugin is refused before anything changes");
        event = a->result(LoadPatch{"file:///tmp/peepo-live-nowhere/r000001.ingen", saved_blocks(stored->bundle)});
        check(!event.accepted && event.message.find("restored nothing") != std::string::npos && a->graph().patch_id.empty(), "a bundle the engine cannot read is a failure, not a silent empty patch", event.message);
        event = a->result(LoadPatch{store.find(id)->location, saved_blocks(store.find(id)->bundle)});
        check(event.accepted && find_module(a->graph(), mist.id) && find_module(a->graph(), mist.id)->x == 600, "the newest revision loads", event.message);
        // The same bundle with every project property and position removed:
        // still a usable graph, laid out by default, under its folder's identity.
        {
            const auto source = store.find(id)->path;
            const auto stripped = root / "p-stripped" / "r000001.ingen";
            std::filesystem::create_directories(stripped.parent_path());
            std::filesystem::copy(source, stripped, std::filesystem::copy_options::recursive);
            std::ifstream in(source / "main.ttl");
            std::vector<std::string> kept;
            for (std::string line; std::getline(in, line);) {
                const bool ours = line.find("peepo:") != std::string::npos || line.find("ingen:canvas") != std::string::npos;
                if (!ours) { kept.push_back(line); continue; }
                // Dropping a statement's last property moves its full stop up a line.
                if (line.ends_with(" .") && !kept.empty() && kept.back().ends_with(" ;")) kept.back().replace(kept.back().size() - 2, 2, " .");
            }
            std::ofstream out(stripped / "main.ttl", std::ios::trunc);
            for (const auto& line : kept) out << line << "\n";
        }
        const auto bare = store.find("p-stripped");
        check(bare && bare->revision == 1 && bare->issue.empty() && bare->name == "p-stripped" && bare->bundle.id.empty(), "a bundle without project properties is offered", bare ? bare->issue : "missing");
        event = a->result(LoadPatch{bare->location, saved_blocks(bare->bundle), bare->id});
        const auto* plain = find_module(a->graph(), first.id);
        check(event.accepted && a->graph().patch_id == "p-stripped" && a->graph().name == "Untitled" && a->graph().modules.size() == before_load.modules.size() &&
              plain && plain->name == "Simple Amplifier" && plain->y == 276 && std::abs(gain_of(a->graph(), first.id) - 6.0206) < 1e-4,
              "it loads with default names and layout, its values, and the folder's identity", event.message);
        event = a->result(LoadPatch{store.find(id)->location, saved_blocks(store.find(id)->bundle), id});
        check(event.accepted && a->graph().patch_id == id, "back to the stored patch", event.message);
        a->ok(RemoveModule{mist.id}, "remove the granular module again");
        b.pump(150);
    }

    std::vector<double> times;
    for (int i = 0; i < 200; ++i) { double took = 0; a->result(SetParameter{first.id, "gain", 6.0206 - 0.001 * (i % 2)}, &took); times.push_back(took); }
    a->ok(SetParameter{first.id, "gain", 6.0206}, "restore the value");
    std::sort(times.begin(), times.end());
    std::cout << "INFO  edit round trip through the adapter -- median " << times[100] << " ms, p95 " << times[189] << " ms, max " << times.back() << " ms (200 edits)" << std::endl;

    // The interface dies without a word; a new one finds everything as it was.
    const auto running = a->graph();
    a.reset();
    Client restarted(address);
    restarted.engine.connect();
    check(restarted.ready() && restarted.graph().modules.size() == running.modules.size() && restarted.graph().name == running.name &&
          restarted.graph().connections.size() == running.connections.size() && find_module(restarted.graph(), first.id) &&
          std::abs(gain_of(restarted.graph(), first.id) - 6.0206) < 1e-4, "a restarted interface recovers the graph under the same identities");
    check(restarted.graph().patch_id == running.patch_id && restarted.graph().dirty == running.dirty,
          "it also knows which stored patch is playing, and whether it has unsaved edits", running.patch_id + (running.dirty ? ", unsaved edits" : ", saved"));
    const auto load = restarted.engine.telemetry().cpu_percent;
    restarted.pump(300);
    std::cout << "INFO  engine load reading -- " << (restarted.engine.telemetry().cpu_percent ? std::to_string(*restarted.engine.telemetry().cpu_percent) + " %" : std::string("none yet"))
              << (load ? "" : " (none at connect)") << std::endl;
    restarted.ok(SetParameter{first.id, "gain", 6.0206}, "the restarted interface can edit");
    std::cout << "SUMMARY " << (failures ? std::to_string(failures) + " failed" : std::string("all adapter checks passed")) << std::endl;
    return failures ? 1 : 0;
}
