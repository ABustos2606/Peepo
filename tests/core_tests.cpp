// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "core/euclidean.hpp"
#include "core/poly_voices.hpp"
#include "engine/simulated_engine.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace peepo;
void check(bool result, const char* why) { if (!result) throw std::runtime_error(why); }
int main() {
    try {
        auto g = demo_graph();
        check(!validate_connection(g, {5, 3}).empty(), "feedback cycle accepted");
        check(!validate_connection(g, {3, 4}).empty(), "duplicate cable accepted");
        check(!validate_connection(g, {2, 3}).empty(), "wrong port direction accepted");
        check(validate_connection(g, {3, 5}).empty(), "valid fan-out rejected");
        check(!validate_connection(g, {99, 3}).empty(), "missing module accepted");
        {
            auto named = g; const auto plain = named.modules[2].name;
            check(numbered_name(named, "Nobody") == "Nobody" && numbered_name(named, plain) == plain + " 2", "first copy not numbered 2");
            named.modules.push_back(named.modules[2]); named.modules.back().name = plain + " 3";
            check(numbered_name(named, plain) == plain + " 2", "lowest free number not taken");
            const Port stereo_in{"in", Direction::Input, 2}, side{"side", Direction::Input, 1}, out{"out", Direction::Output, 2};
            check(card_height({stereo_in, out}) == module_height && card_height({stereo_in, side, out}) == 3 * connector_pitch + 8,
                "card not grown for a third connector");
        }
        const auto& p = g.modules[3].parameters[1];
        check(std::abs(value_from_normalized(p, normalized_value(p)) - p.value) < 0.0001, "logarithmic round trip");
        check(value_from_normalized(p, -1) == p.minimum && value_from_normalized(p, 2) == p.maximum, "range clamp");
        // A patch's control changes as one property value.
        const std::vector<ControlChange> changes{{1, 7, 100}, {3, 0, 0}, {16, 127, 127}};
        check(encode_control_changes(changes) == "1:7:100 3:0:0 16:127:127" && decode_control_changes(encode_control_changes(changes)) == changes &&
              decode_control_changes("") == std::vector<ControlChange>{}, "control changes do not read back");
        for (const char* bad : {"0:7:1", "17:7:1", "1:128:1", "1:7:-1", "1:7", "1-7-1", "1:7:1x", "1:1:1 1:1:1 1:1:1 1:1:1 1:1:1 1:1:1 1:1:1"})
            check(!decode_control_changes(bad), "a bad control change list was read");
        SimulatedEngine engine;
        const auto req = engine.submit(SetParameter{3, "drive", 70});
        check(engine.snapshot().modules[2].parameters[0].value == 32, "edit applied before ack");
        check(engine.advance(99).empty(), "delay ignored");
        auto events = engine.advance(1);
        check(events.size() == 1 && events[0].request == req && events[0].accepted, "request correlation");
        check(engine.snapshot().modules[2].parameters[0].value == 70, "accepted value missing");
        engine.reject_next(); engine.submit(SetParameter{3, "drive", 80});
        check(!engine.advance(100)[0].accepted, "rejection missing");
        check(engine.snapshot().modules[2].parameters[0].value == 70, "rejected edit changed state");
        engine.submit(SetParameter{3, "drive", std::numeric_limits<double>::quiet_NaN()});
        check(!engine.advance(100)[0].accepted, "NaN accepted");
        engine.submit(SetParameter{4, "freeze", 0.4});
        check(!engine.advance(100)[0].accepted, "fractional toggle accepted");
        engine.submit(RemoveModule{1}); check(!engine.advance(100)[0].accepted, "endpoint deleted");
        engine.submit(AddModule{"urn:missing", 24, 24}); check(!engine.advance(100)[0].accepted, "missing plugin accepted");
        engine.submit(SetParameter{3, "drive", 80}); engine.set_state(EngineState::Disconnected);
        events = engine.advance(100); check(events.size() == 2 && !events[0].accepted, "disconnect did not cancel pending");
        engine.set_state(EngineState::Ready); engine.advance(100);
        check(engine.snapshot().modules[2].parameters[0].value == 70, "stale edit survived reconnect");
        engine.submit(RemoveModule{4}); check(engine.advance(100)[0].accepted, "remove failed");
        check(engine.snapshot().connections.size() == 4, "dangling cables remain");
        auto invalid = demo_graph(); invalid.connections.push_back({5, 3});
        engine.submit(LoadGraph{invalid}); check(!engine.advance(100)[0].accepted, "invalid loaded graph accepted");
        engine.submit(LoadGraph{demo_graph()}); check(engine.advance(100)[0].accepted, "valid restore rejected");
        auto assigned = engine.snapshot(); assigned.assignments.push_back({"knob-1", 3, "drive", 0, 100});
        engine.submit(LoadGraph{assigned}); engine.advance(100);
        engine.submit(SetParameter{3, "drive", 81}); engine.advance(100);
        engine.submit(SetBypass{3, true}); engine.advance(100);
        const auto original = *find_module(engine.snapshot(), 3);
        const auto cables = engine.snapshot().connections;
        engine.reject_next(); engine.submit(DuplicateModule{3, 1008, 504});
        check(!engine.advance(100)[0].accepted && engine.snapshot().modules.size() == 5, "rejected duplicate changed graph");
        engine.submit(DuplicateModule{1, 1008, 504});
        check(!engine.advance(100)[0].accepted, "endpoint duplicated");
        engine.submit(DuplicateModule{3, 1008, 504});
        check(engine.snapshot().modules.size() == 5, "duplicate appeared before acknowledgment");
        check(engine.advance(100)[0].accepted, "duplicate rejected");
        const auto& copy = engine.snapshot().modules.back();
        check(copy.id != original.id && copy.uri == original.uri && copy.bypassed && copy.parameters[0].value == 81,
            "duplicate lost identity, parameter or bypass state");
        check(copy.x == 1008 && copy.y == 504 && engine.snapshot().connections == cables, "duplicate copied cables or lost world position");
        check(copy.name == original.name + " 2", "duplicate not numbered after the original");
        check(engine.snapshot().assignments.size() == 1 && engine.snapshot().assignments[0].module == 3, "duplicate copied control assignments");
        engine.submit(MoveModule{copy.id, 2016, 1008}); engine.advance(100);
        check(engine.snapshot().modules.back().x == 2016 && engine.snapshot().modules.back().y == 1008, "larger canvas movement was clamped to viewport");
        engine.submit(SetParameter{copy.id, "drive", 22}); engine.advance(100);
        check(find_module(engine.snapshot(), 3)->parameters[0].value == 81, "copy edits changed original");
        engine.submit(MoveModule{copy.id, -96, 5000}); engine.advance(100);
        check(engine.snapshot().modules.back().x == module_min_x && engine.snapshot().modules.back().y == module_max_y,
            "canvas boundary constraints failed");
        const auto caps = engine.capabilities();
        check(caps.simulation && caps.session_save && caps.persistent_save && !caps.plugin_state && !caps.audio_processing,
            "simulator advertised unsupported capability");
        const auto prior_name = engine.snapshot().name;
        engine.submit(NewPatch{"  "}); check(!engine.advance(100)[0].accepted && engine.snapshot().name == prior_name, "blank patch replaced graph");
        engine.submit(NewPatch{"Practice"}); check(engine.advance(100)[0].accepted, "new patch rejected");
        check(engine.snapshot().modules.size() == 2 && engine.snapshot().connections.size() == 2 && engine.snapshot().dirty, "new patch endpoints or dirty marker incorrect");
        engine.submit(RenamePatch{"Evening practice"}); engine.advance(100);
        engine.submit(SaveGraph{}); events = engine.advance(100);
        check(events[0].accepted && events[0].saved_session && !engine.snapshot().dirty && events[0].saved_session->name == "Evening practice", "engine session save failed");
        engine.submit(RenamePatch{"Changed"}); engine.advance(100);
        engine.reject_next(); engine.submit(SaveGraph{}); check(!engine.advance(100)[0].accepted && engine.snapshot().dirty, "rejected save cleared dirty state");
        engine.submit(LoadSession{}); engine.advance(100);
        check(engine.snapshot().name == "Evening practice" && !engine.snapshot().dirty, "rejected save overwrote session snapshot");
        engine.submit(SaveGraph{SaveScope::Persistent}); events = engine.advance(100);
        check(!events[0].accepted && events[0].message.find("Invalid save location") != std::string::npos, "persistent save without a location was accepted");
        engine.submit(SaveGraph{SaveScope::Session, true}); check(!engine.advance(100)[0].accepted, "plugin state save pretended to succeed");
        engine.disconnect(); engine.advance(0); engine.connect();
        check(engine.state() == EngineState::Connecting, "connect skipped lifecycle");
        engine.advance(99); check(engine.state() == EngineState::Connecting, "connect delay ignored");
        engine.advance(1); check(engine.state() == EngineState::Ready, "connect did not become ready");
        engine.connect(); engine.disconnect(); engine.advance(100);
        check(engine.state() == EngineState::Disconnected, "cancelled connection became ready");
        engine.connect(); engine.submit(RenamePatch{"Too early"}); events = engine.advance(100);
        check(!events[1].accepted && engine.snapshot().name == "Evening practice", "edit submitted while connecting was applied later");
        engine.set_state(EngineState::Ready); engine.advance(0);
        check(engine.simulate_missing_module(), "missing-module fixture failed");
        engine.advance(0);
        check(engine.snapshot().connections.size() == 8 && find_module(engine.snapshot(), 4)->parameters.size() == 6, "missing module lost cables or saved values");
        check(validate_graph(engine.snapshot()).empty() && !expand_connection(engine.snapshot(), {3, 4}).error.empty(), "missing cable was routed or graph lost recoverability");
        engine.submit(SetParameter{4, "mix", 80}); check(!engine.advance(100)[0].accepted, "missing-module parameter accepted");
        engine.submit(DuplicateModule{4, 24, 24}); check(!engine.advance(100)[0].accepted, "missing module duplicated");
        engine.submit(AddModule{"urn:peepo:demo:nam", 24, 24}); check(!engine.advance(100)[0].accepted, "untested module added");
        engine.submit(RemoveModule{4}); engine.advance(100); check(engine.snapshot().connections.size() == 4, "missing-module deletion left dangling cables");

        // All four channel-count combinations, using actual per-channel symbols.
        for (unsigned source_channels : {1U, 2U}) for (unsigned target_channels : {1U, 2U}) {
            auto routing_graph = empty_graph("Routing"); routing_graph.connections.clear();
            auto& out = routing_graph.modules[0].ports[0]; auto& in = routing_graph.modules[1].ports[0];
            out.channels = source_channels; out.channel_symbols = source_channels == 1 ? std::vector<std::string>{"source"} : std::vector<std::string>{"source_left", "source_right"};
            in.channels = target_channels; in.channel_symbols = target_channels == 1 ? std::vector<std::string>{"sink"} : std::vector<std::string>{"sink_left", "sink_right"};
            const auto plan = expand_connection(routing_graph, {1, 2});
            check(plan.error.empty() && plan.connections.size() == std::max(source_channels, target_channels), "channel expansion count incorrect");
            check(plan.connections[0].output == out.channel_symbols[0] && plan.connections[0].input == in.channel_symbols[0], "left/mono channel mapping incorrect");
            if (plan.connections.size() == 2) check(plan.connections[1].output == out.channel_symbols[source_channels - 1] && plan.connections[1].input == in.channel_symbols[target_channels - 1], "right channel mapping incorrect");
            out.type = PortType::Midi; check(!expand_connection(routing_graph, {1, 2}).error.empty(), "non-audio cable accepted");
            out.type = PortType::Audio; out.channels = 3; check(!validate_graph(routing_graph).empty(), "unsupported channel count accepted");
        }
        auto bad = demo_graph(); bad.modules[0].fixed = false;
        check(!validate_graph(bad).empty(), "movable endpoint accepted");
        bad = demo_graph(); bad.modules[2].parameters[0].value = std::numeric_limits<double>::infinity();
        check(!validate_graph(bad).empty(), "invalid snapshot parameter accepted");
        bad = demo_graph(); bad.modules[2].id = 1;
        check(!validate_graph(bad).empty(), "duplicate instance identity accepted");
        bad = demo_graph(); bad.modules[0].ports[0].channel_symbols = {"one", "too_many"};
        check(!expand_connection(bad, {1, 3}).error.empty(), "incomplete channel symbols were expanded");
        auto parallel = parallel_demo_graph();
        check(validate_graph(parallel).empty() && parallel.connections.size() == 10, "parallel fixture invalid");
        std::size_t physical_count = 0;
        for (const auto& edge : parallel.connections) {
            const auto plan = expand_connection(parallel, edge);
            check(plan.error.empty(), "parallel branch expansion rejected");
            physical_count += plan.connections.size();
        }
        check(physical_count == 10, "parallel stereo fixture lost physical routes");
        check(!validate_connection(parallel, {4, 3}).empty(), "parallel feedback cycle accepted");
        engine.submit(LoadGraph{parallel}); check(engine.advance(100)[0].accepted, "engine refused parallel fixture");
        engine.submit(Disconnect{{3, 4}}); check(engine.advance(100)[0].accepted, "parallel branch deletion failed");
        check(engine.snapshot().connections.size() == 9, "branch deletion changed multiple edges");
        engine.submit(Connect{{3, 4}}); check(engine.advance(100)[0].accepted, "parallel branch reconnect failed");
        // Reference entries carry upstream LV2 metadata, rather than invented fixtures.
        const auto* mist = definition("http://polyeffects.com/lv2/polyclouds#granular");
        const auto* fold = definition("http://polyeffects.com/lv2/polywarps#fold");
        const auto* grids = definition("http://polyeffects.com/lv2/polygrids");
        check(mist && fold && grids, "reference catalogue entries missing");
        check(mist->reference && mist->availability == Availability::Available && mist->parameters.size() == 12, "reference Clouds metadata incomplete");
        check(grids->availability == Availability::Available, "control-only module is not offered");
        check(format_value(mist->parameters[0]) == "0.24", "narrow-range value lost its decimals");
        auto real = demo_graph(); real.connections.clear();
        real.modules.push_back({6, mist->uri, mist->name, 24, 264, false, false, mist->parameters, mist->ports});
        real.modules.push_back({7, fold->uri, fold->name, 168, 264, false, false, fold->parameters, fold->ports});
        check(validate_graph(real).empty(), "reference modules rejected by graph validation");
        // Every connector is patched on its own: left to one effect, right to another.
        check(validate_connection(real, {3, 6, "out", "l_in"}).empty() && validate_connection(real, {3, 7, "out_r", "carrier"}).empty(),
            "left and right outputs could not go to different modules");
        check(validate_connection(real, {6, 7, "r_out", "modulator"}).empty(), "one channel of a stereo module could not feed a mono input");
        const auto route = expand_connection(real, {3, 6, "out_r", "l_in"});
        check(route.connections.size() == 1 && route.connections[0].output == "out_r" && route.connections[0].input == "l_in", "a cable is not exactly one engine connection");
        check(!validate_connection(real, {6, 7}).empty(), "cable matched a port the module does not have");
        check(!validate_connection(real, {7, 6, "out", "position"}).empty(), "CV input accepted an audio cable");
        check(!validate_connection(real, {3, 6, "out", "in"}).empty(), "cable matched a merged stereo port that should no longer exist");
        engine.submit(AddModule{grids->uri, 24, 24}); check(engine.advance(100)[0].accepted, "control-only module could not be added");
        // Control cables join control connectors, and never an audio one.
        const auto* tides = definition("http://polyeffects.com/lv2/polytides");
        check(tides != nullptr, "reference Tides metadata missing");
        real.modules.push_back({8, tides->uri, tides->name, 312, 264, false, false, tides->parameters, tides->ports});
        check(validate_graph(real).empty() && validate_connection(real, {8, 6, "out1", "position"}).empty(), "control output could not modulate a control input");
        check(!validate_connection(real, {8, 6, "out1", "l_in"}).empty(), "control output accepted by an audio input");
        check(!validate_connection(real, {6, 8, "l_out", "trig_input"}).empty(), "audio output accepted by a control input");
        real.connections.push_back({8, 6, "out1", "position"});
        check(validate_graph(real).empty() && expand_connection(real, {8, 6, "out1", "position"}).connections.size() == 1, "control cable is not one engine connection");
        // MIDI cables join MIDI connectors: IN's merged devices, modules, OUT.
        // A message input, such as a file port, is not a connector at all.
        register_definitions({{"urn:test:chord", "Chord", "MIDI", "Notes to chords", 0x93a4b5,
                               {{"midi_in", Direction::Input, 1, PortType::Midi}, {"midi_out", Direction::Output, 1, PortType::Midi},
                                {"files", Direction::Input, 1, PortType::Atom}, {"notify", Direction::Output, 1, PortType::Atom}}, {}}});
        const auto* chord = definition("urn:test:chord");
        real.modules.push_back({9, chord->uri, chord->name, 456, 264, false, false, {}, chord->ports});
        real.modules.push_back({10, chord->uri, chord->name, 600, 264, false, false, {}, chord->ports});
        check(validate_graph(real).empty() && validate_connection(real, {1, 9, "midi", "midi_in"}).empty() && validate_connection(real, {9, 2, "midi_out", "midi"}).empty(),
              "MIDI could not go from IN through a module to OUT");
        check(validate_connection(real, {1, 2, "midi", "midi"}).empty(), "MIDI IN could not go straight to MIDI OUT");
        check(!validate_connection(real, {1, 9, "out", "midi_in"}).empty() && !validate_connection(real, {9, 6, "midi_out", "l_in"}).empty() &&
              !validate_connection(real, {9, 8, "midi_out", "trig_input"}).empty() && !validate_connection(real, {1, 2, "midi", "in"}).empty(),
              "MIDI joined to audio or control");
        check(!validate_connection(real, {1, 9, "midi", "files"}).empty() && !validate_connection(real, {9, 10, "notify", "files"}).empty(), "a message port was cabled");
        real.connections.push_back({9, 10, "midi_out", "midi_in"});
        check(!validate_connection(real, {9, 10, "midi_out", "midi_in"}).empty(), "duplicate MIDI cable accepted");
        check(!validate_connection(real, {10, 9, "midi_out", "midi_in"}).empty(), "MIDI feedback cycle accepted");
        check(expand_connection(real, {9, 10, "midi_out", "midi_in"}).connections.size() == 1, "MIDI cable is not one engine connection");
        engine.submit(AddModule{fold->uri, 24, 24}); check(engine.advance(100)[0].accepted, "reference module could not be added");
        // A time in beats names its note and steps from one note value to the next.
        check(note_name(0.75) == "1/8 dot" && note_name(1.0 / 3) == "1/8 trip" && note_name(0.6).empty(), "note names");
        check(step_note(0.5, 1, 0.001, 16) == 2.0 / 3 && step_note(0.5, -1, 0.001, 16) == 0.375, "a step from a note is not the next one");
        check(step_note(0.6, 1, 0.001, 16) == 2.0 / 3 && step_note(0.6, -1, 0.001, 16) == 0.5, "between notes, a step does not land on the nearer one");
        check(step_note(16, 1, 0.001, 16) == 16 && step_note(0.5, -40, 0.25, 16) == 0.25 && step_note(1, 3, 0.001, 2) == 2, "a step leaves the range");
        Parameter beats{"time", "Time", beats_unit, ParameterKind::Continuous, 0.001, 16, 0.75};
        check(format_value(beats) == "1/8 dot", "a note is not shown by name");
        beats.value = 0.6; check(format_value(beats) == "0.60 beats", "a time between notes is not shown in beats");
        // A stomp switches the bypass of several modules: each module once, and
        // nothing else on the same control.
        Graph stomps = demo_graph();
        Assignment drive{"stomp1", 3}; drive.action = ControlAction::Bypass;
        Assignment clouds{"stomp1", 4}; clouds.action = ControlAction::Bypass;
        stomps.assignments = {drive};
        auto held = assignments_after(stomps, clouds, true);
        check(held.size() == 2 && held[0].module == 3 && held[1].module == 4, "adding a bypass dropped the others");
        check(assignments_after(stomps, clouds, false).size() == 1, "assigning without adding kept the others");
        stomps.assignments = held;
        check(validate_graph(stomps).empty() && bypass_assignments(stomps, "stomp1").size() == 2, "a stomp switching two modules is invalid");
        check(assignments_after(stomps, drive, true).size() == 2, "the same module was added twice");
        stomps.assignments.push_back(drive);
        check(!validate_graph(stomps).empty(), "the same module twice on one stomp accepted");
        stomps.assignments.pop_back();
        Assignment freeze{"stomp1", 4, "freeze", 0, 1};
        stomps.assignments.push_back(freeze);
        check(!validate_graph(stomps).empty(), "a parameter shared a stomp with bypasses");
        check(assignment_parts(" a  b ") == std::vector<std::string>{"a", "b"} && assignment_parts("").empty(), "assignment parts");
        engine.submit(LoadGraph{demo_graph()}); engine.advance(100);
        engine.submit(Assign{drive, true}); engine.submit(Assign{clouds, true}); engine.advance(100);
        check(engine.snapshot().assignments.size() == 2, "the simulator did not add a second bypass to a stomp");
        engine.submit(Unassign{"stomp1", 3}); engine.advance(100);
        check(engine.snapshot().assignments.size() == 1 && engine.snapshot().assignments[0].module == 4, "unassigning one module cleared the stomp");
        engine.submit(Assign{drive, true}); engine.submit(RemoveModule{4}); engine.advance(100);
        check(engine.snapshot().assignments.size() == 1 && engine.snapshot().assignments[0].module == 3, "removing a module took the stomp's other module along");
        // Scenes: bypasses and marked parameters, written from the screen,
        // worked out as changes, kept in step with the modules, stored as text.
        {
            auto sc = demo_graph();
            const std::string drive_uri = sc.modules[2].uri, mist_uri = sc.modules[3].uri;
            auto value_of = [&](InstanceId id, const char* symbol) -> double& {
                for (auto& p : find_module(sc, id)->parameters) if (p.symbol == symbol) return p.value;
                throw std::runtime_error("no such parameter");
            };
            check(validate_graph(sc).empty() && scene_changes(sc, 0).empty(), "a patch without scenes changed");
            check(!mark_scene_parameter(sc, 3, "drive").empty() && !record_scene_bypass(sc, 3, true), "a patch without scenes took a scene edit");
            check(add_scene(sc).empty() && sc.scenes.scenes.size() == 1 && sc.scenes.scenes[0].name == "Scene 1" && sc.scenes.current == 0, "first scene");
            check(sc.scenes.scenes[0].entries.size() == 3, "the first scene does not hold every module's bypass");
            check(mark_scene_parameter(sc, 3, "drive").empty() && mark_scene_parameter(sc, 3, "drive").empty() && sc.scenes.parameters.size() == 1,
                  "marking twice marked twice");
            check(!mark_scene_parameter(sc, 1, "out").empty() && !mark_scene_parameter(sc, 3, "nothing").empty(), "an endpoint or missing parameter marked");
            check(scene_parameter_marked(sc, 3, "drive") && !scene_parameter_marked(sc, 3, "tone"), "marked parameter not found");
            check(add_scene(sc).empty() && sc.scenes.scenes[1].name == "Scene 2" && sc.scenes.current == 1, "second scene");
            // Edits on the screen go into the current scene, unmarked parameters do not.
            find_module(sc, 4)->bypassed = true;
            value_of(3, "drive") = 80;
            check(record_scene_bypass(sc, 4, true) && record_scene_parameter(sc, 3, "drive", 80) && !record_scene_parameter(sc, 3, "tone", 500),
                  "screen edits not written into the scene");
            check(!record_scene_bypass(sc, 4, true), "an unchanged bypass counted as a change");
            check(scene_changes(sc, 1).empty(), "the live state differs from the scene it was written into");
            check((scene_changes(sc, 0) == std::vector<SceneChange>{{4, "", 0}, {3, "drive", 32}}), "changes to the first scene");
            check(scene_changes(sc, 7).empty(), "a missing scene gave changes");
            // A performance change is not recorded, so the scene brings back its state.
            value_of(3, "drive") = 50;
            check((scene_changes(sc, 1) == std::vector<SceneChange>{{3, "drive", 80}}), "selecting the scene again does not restore it");
            value_of(3, "drive") = 80;
            check(validate_graph(sc).empty(), "valid scenes refused");
            // Stored as graph properties.
            auto block_of = [](InstanceId id) { return "m" + std::to_string(id); };
            auto id_of = [&](const std::string& block) -> InstanceId {
                for (const auto& m : sc.modules) if (block == "m" + std::to_string(m.id)) return m.id;
                return 0;
            };
            const auto stored = encode_scenes(sc.scenes, block_of);
            check(stored.count == 2 && stored.current == 2 && stored.parameters == "m3|" + drive_uri + "|drive", "scene properties");
            check(stored.scenes[1].first == "Scene 2" && stored.scenes[1].second == "b|m3|" + drive_uri + "|0 b|m4|" + mist_uri + "|1 b|m5|" +
                  sc.modules[4].uri + "|0 p|m3|" + drive_uri + "|drive|80", "a scene's entries");
            auto read = sc;
            read.scenes = decode_scenes(stored, id_of);
            const auto again = encode_scenes(read.scenes, block_of);
            check(again.count == stored.count && again.current == stored.current && again.parameters == stored.parameters && again.scenes == stored.scenes &&
                  scene_changes(read, 0) == scene_changes(sc, 0), "scenes do not read back");
            // Orphans: a block that is gone, or that now holds another plugin, is kept and never applied.
            auto orphaned = stored;
            orphaned.scenes[0].second += " b|m9|urn:gone|1 b|m3|urn:other|1 p|m3|" + drive_uri + "|tone|500 bad b|m4|" + mist_uri + "|1 b|m4|x|0.5";
            read.scenes = decode_scenes(orphaned, id_of);
            check(read.scenes.scenes[0].entries.size() == 6 && validate_graph(read).empty(), "orphans not kept, or a bad entry read");
            check(scene_changes(read, 0) == scene_changes(sc, 0), "an orphan was applied");
            check(encode_scenes(read.scenes, block_of).scenes[0].second.ends_with(" b|m9|urn:gone|1 b|m3|urn:other|1"), "an orphan was not written back");
            read.modules.push_back(Module{9, drive_uri, "Drive 2", 100, 300, false, false, sc.modules[2].parameters, sc.modules[2].ports});
            check(scene_changes(read, 0) == scene_changes(sc, 0), "an orphan's plugin was not checked");
            // Bad text is cut, not refused.
            auto bad = stored;
            bad.count = 12; bad.current = 9; bad.scenes[0].first = "Far too long a name";
            read.scenes = decode_scenes(bad, id_of);
            check(read.scenes.scenes.size() == 2 && read.scenes.current == 1 && read.scenes.scenes[0].name == "Scene 1", "bad properties not cut");
            check(decode_scenes({0, 1, stored.parameters, stored.scenes}, id_of).scenes.empty() && decode_scenes({}, id_of).parameters.empty(), "no scenes read as some");
            // Modules added, duplicated and removed.
            sc.modules.push_back(Module{6, drive_uri, "Drive 2", 100, 300, false, true, sc.modules[2].parameters, sc.modules[2].ports});
            scenes_module_duplicated(sc, 3, 6);
            check(sc.scenes.parameters.size() == 2 && scene_parameter_marked(sc, 6, "drive"), "a duplicate's marked parameter not marked");
            check((scene_changes(sc, 0) == std::vector<SceneChange>{{4, "", 0}, {3, "drive", 32}, {6, "", 0}, {6, "drive", 32}}),
                  "a duplicate does not follow the original's scenes");
            sc.modules.push_back(Module{7, mist_uri, "Mist 2", 100, 400, false, true, sc.modules[3].parameters, sc.modules[3].ports});
            scenes_module_added(sc, 7);
            check(std::all_of(sc.scenes.scenes.begin(), sc.scenes.scenes.end(), [](const Scene& s) { return s.entries.back().target.module == 7 && s.entries.back().value == 1; }),
                  "an added module's bypass is not in every scene");
            sc.scenes.scenes[0].entries.push_back({{0, "m9", "urn:gone", {}}, 1});
            const auto copy = *find_module(sc, 6);
            std::erase_if(sc.modules, [](const Module& m) { return m.id == 6; });
            scenes_module_removed(sc, copy);
            check(sc.scenes.parameters.size() == 1 && sc.scenes.scenes[1].entries.size() == 5 && sc.scenes.scenes[0].entries.back().target.block == "m9",
                  "a removed module stayed in the scenes, or an orphan went with it");
            check(validate_graph(sc).empty(), "scenes after removing a module refused");
            // At most 16 marked parameters.
            auto full = sc;
            for (int i = 0; full.scenes.parameters.size() < max_scene_parameters; ++i) full.scenes.parameters.push_back({0, "m9", "urn:gone", "p" + std::to_string(i)});
            check(!mark_scene_parameter(full, 3, "tone").empty() && validate_graph(full).empty(), "a 17th parameter marked");
            full.scenes.parameters.push_back({0, "m9", "urn:gone", "p99"});
            check(!validate_graph(full).empty(), "17 marked parameters accepted");
            // Unmarking leaves the live value and takes the parameter out of every scene.
            check(unmark_scene_parameter(sc, 3, "drive").empty() && !unmark_scene_parameter(sc, 3, "drive").empty() && sc.scenes.parameters.empty() &&
                  value_of(3, "drive") == 80 && sc.scenes.scenes[1].entries.size() == 4, "unmarking");
            // Rename, copy, move, delete; the current scene follows a move.
            check(!rename_scene(sc, 0, "").empty() && !rename_scene(sc, 0, "Thirteen char").empty() && !rename_scene(sc, 5, "Verse").empty(), "a bad rename accepted");
            check(rename_scene(sc, 0, "Verse").empty() && add_scene(sc).empty() && sc.scenes.scenes[2].name == "Scene 1", "rename, or the lowest free name");
            check(copy_scene(sc, 1, 0).empty() && sc.scenes.scenes[0].entries == sc.scenes.scenes[1].entries && sc.scenes.scenes[0].name == "Verse", "copy");
            check(sc.scenes.current == 2 && move_scene(sc, 2, 0).empty() && sc.scenes.current == 0 && sc.scenes.scenes[0].name == "Scene 1", "moving the current scene");
            check(move_scene(sc, 2, 0).empty() && sc.scenes.current == 1 && move_scene(sc, 0, 2).empty() && sc.scenes.current == 0, "the current scene did not follow");
            check(!move_scene(sc, 0, 3).empty() && !copy_scene(sc, 3, 0).empty(), "a missing scene moved or copied");
            check(!delete_scene(sc, 0).empty(), "the current scene deleted while others exist");
            check(delete_scene(sc, 1).empty() && sc.scenes.scenes.size() == 2 && sc.scenes.current == 0, "deleting another scene");
            sc.scenes.current = 1;
            check(delete_scene(sc, 0).empty() && sc.scenes.current == 0 && mark_scene_parameter(sc, 3, "tone").empty(), "deleting before the current scene");
            check(delete_scene(sc, 0).empty() && sc.scenes.scenes.empty() && sc.scenes.parameters.empty() && validate_graph(sc).empty(), "deleting the last scene");
            for (std::size_t i = 0; i < max_scenes; ++i) check(add_scene(sc).empty(), "eight scenes refused");
            check(!add_scene(sc).empty() && sc.scenes.scenes.back().name == "Scene 8", "a ninth scene");
            // Validation.
            auto invalid = sc;
            invalid.scenes.current = 8;
            check(!validate_graph(invalid).empty(), "a current scene out of range accepted");
            invalid = sc; invalid.scenes.scenes[0].entries[0].value = 0.5;
            check(!validate_graph(invalid).empty(), "a half bypass accepted");
            invalid = sc; invalid.scenes.scenes[0].entries.push_back(invalid.scenes.scenes[0].entries[0]);
            check(!validate_graph(invalid).empty(), "a repeated entry accepted");
            invalid = sc; mark_scene_parameter(invalid, 3, "drive"); invalid.scenes.scenes[0].entries.back().value = 101;
            check(!validate_graph(invalid).empty(), "a value out of range accepted");
            invalid = sc; invalid.scenes.scenes[0].entries.push_back({{3, {}, drive_uri, "tone"}, 500});
            check(!validate_graph(invalid).empty(), "an unmarked parameter's entry accepted");
            invalid = sc; invalid.scenes.scenes[0].entries.push_back({{0, "m9", "urn:bad plugin", {}}, 1});
            check(!validate_graph(invalid).empty(), "a separator in a field accepted");
        }
        // Euclidean rhythm generator matches poly_euclidean.c's Bjorklund rotation.
        check(euclidean_uri("http://polyeffects.com/lv2/euclidean") && !euclidean_uri("http://gareus.org/oss/lv2/tuna#mod"), "euclidean URI check");
        check(euclidean_pattern(0, 3, 0).empty() && euclidean_pattern(8, 0, 0) == std::vector<bool>(8, false), "empty or silent euclidean pattern");
        check(euclidean_pattern(4, 4, 0) == std::vector<bool>(4, true) && euclidean_pattern(4, 9, 0) == std::vector<bool>(4, true), "full euclidean pattern");
        check(euclidean_pattern(8, 3, 0) == std::vector<bool>{true, false, true, false, false, true, false, false}, "tresillo 3:8 pattern");
        check(euclidean_pattern(8, 3, 1) == std::vector<bool>{false, true, false, true, false, false, true, false}, "tresillo 3:8 shifted by 1");
        check(euclidean_pattern(8, 3, 2) == std::vector<bool>{false, false, true, false, true, false, false, true}, "tresillo 3:8 shifted by 2");
        check(euclidean_pattern(40, 1, 0).size() == euclidean_max_steps, "euclidean step count clamped to 32");
        // MIDI to CV poly's Voices follows the highest voice with a cable.
        Graph poly;
        poly.modules.push_back(Module{7, midi_cv_poly_uri, "MIDI to CV poly", 0, 0});
        const auto& midi_cv = poly.modules.back();
        check(cabled_voices(poly, midi_cv) == 0, "no cables, no voices");
        poly.connections = {{7, 8, "pitch_1", "note_cv"}, {7, 8, "gate_1", "trigger_cv"}, {7, 9, "gate_2", "trigger_cv"}};
        check(cabled_voices(poly, midi_cv) == 2, "two voices cabled");
        poly.connections.push_back({7, 10, "pitch_3", "note_cv"});
        check(cabled_voices(poly, midi_cv) == 3, "a pitch alone counts");
        poly.connections.push_back({7, 10, "mod", "timbre_cv"});
        poly.connections.push_back({6, 10, "gate_4", "trigger_cv"});
        check(cabled_voices(poly, midi_cv) == 3, "mod and other modules' cables do not count");
        poly.connections = {{7, 8, "gate_4", "trigger_cv"}, {7, 8, "gate_1", "level_cv"}};
        check(cabled_voices(poly, midi_cv) == 4, "the highest voice counts, in any order");
        std::cout << "Domain validation, typed ranges, delayed acknowledgment, rejection, reconnect and restore passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
