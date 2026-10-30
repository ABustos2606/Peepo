// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// Bundles and the patch store: reading what the engine writes, revisions,
// interrupted saves, fallback to an older revision, identity, the startup
// choice, and the simulator saving and loading through the same store.
// Files from a stick: sorted, checked, copied, duplicates skipped or numbered.
#include "engine/simulated_engine.hpp"
#include "storage/file_import.hpp"
#include "storage/file_store.hpp"
#include "storage/patch_store.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace peepo;
namespace fs = std::filesystem;
namespace {
void check(bool result, const char* why) { if (!result) throw std::runtime_error(why); }
// Written by the patched Ingen under WSL on 14 October 2026 (tools/ingen-spike/10-bundle-probe.py),
// with the LV2 example amplifier and the stateful example `params`.
const char* recorded = R"(@prefix atom: <http://lv2plug.in/ns/ext/atom#> .
@prefix doap: <http://usefulinc.com/ns/doap#> .
@prefix ingen: <http://drobilla.net/ns/ingen#> .
@prefix lv2: <http://lv2plug.in/ns/lv2core#> .
@prefix midi: <http://lv2plug.in/ns/ext/midi#> .
@prefix owl: <http://www.w3.org/2002/07/owl#> .
@prefix patch: <http://lv2plug.in/ns/ext/patch#> .
@prefix rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .
@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .
@prefix peepo: <urn:peepo:ns#> .

<control>
	ingen:canvasX 32.0 ;
	ingen:canvasY 32.0 ;
	atom:bufferType atom:Sequence ;
	atom:supports patch:Message ;
	<http://lv2plug.in/ns/ext/resize-port#minimumSize> 4096 ;
	lv2:designation lv2:control ;
	lv2:index 0 ;
	lv2:name "Control" ;
	lv2:portProperty lv2:connectionOptional ;
	lv2:symbol "control" ;
	a atom:AtomPort ,
		lv2:InputPort .

<in_l>
	lv2:index 2 ;
	lv2:name "in_l" ;
	lv2:symbol "in_l" ;
	a lv2:AudioPort ,
		lv2:InputPort .

<m3>
	ingen:canvasX 300.0 ;
	ingen:canvasY 200.0 ;
	lv2:port <m3/gain> ,
		<m3/in> ,
		<m3/out> ;
	lv2:prototype <http://lv2plug.in/plugins/eg-amp> ;
	a ingen:Block ;
	peepo:displayName "Boost" .

<m3/gain>
	ingen:value 6.0 ;
	lv2:index 0 ;
	a lv2:ControlPort ,
		lv2:InputPort ;
	peepo:assignedControl "knob1" .

<m3/in>
	lv2:index 1 ;
	a lv2:AudioPort ,
		lv2:InputPort .

<m3/out>
	lv2:index 2 ;
	a lv2:AudioPort ,
		lv2:OutputPort .

<m4>
	<http://lv2plug.in/ns/ext/state#state> <m4/state.ttl> ;
	lv2:port <m4/in> ,
		<m4/out> ;
	lv2:prototype <http://lv2plug.in/plugins/eg-params> ;
	a ingen:Block .

<>
	ingen:arc [
		ingen:head <m3/in> ;
		ingen:tail <in_l>
	] , [
		ingen:head <out_l> ;
		ingen:tail <m3/out>
	] ;
	ingen:block <m3> ,
		<m4> ;
	ingen:polyphony 1 ;
	<http://lv2plug.in/ns/extensions/ui#ui> ingen:GraphUIGtk2 ;
	lv2:extensionData <http://lv2plug.in/ns/ext/state#interface> ;
	doap:name "one" ;
	a ingen:Graph ,
		lv2:Plugin ;
	peepo:patchId "p-0001" ;
	peepo:patchName "Saved one" ;
	peepo:schemaVersion 1 .
)";
void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}
// What the engine leaves after a save: a complete bundle in staging.
void engine_writes(const PatchStore::Staging& staging, Graph graph) {
    graph.patch_id = staging.id;
    write(staging.path / "main.ttl", write_main_ttl(graph));
    write(staging.path / "manifest.ttl", write_manifest_ttl());
}
std::vector<EngineEvent> run(SimulatedEngine& engine, Command command) {
    engine.submit(std::move(command));
    return engine.advance(100);
}

void bundles() {
    Bundle bundle;
    check(read_bundle(recorded, bundle).empty(), "the engine's own bundle was not readable");
    check(bundle.id == "p-0001" && bundle.name == "Saved one" && bundle.schema == 1 && !bundle.modified, "graph properties misread");
    check(bundle.blocks.size() == 2 && bundle.blocks[0].symbol == "m3" && bundle.blocks[1].symbol == "m4", "blocks misread");
    const auto& amp = bundle.blocks[0];
    check(amp.plugin == "http://lv2plug.in/plugins/eg-amp" && amp.name == "Boost" && amp.positioned && amp.x == 300 && amp.y == 200, "block properties misread");
    check(amp.values.size() == 1 && amp.values.at("gain") == 6 && amp.enabled, "control value misread");
    check(bundle.blocks[1].state == "m4/state.ttl" && !bundle.blocks[1].positioned, "plugin state reference misread");
    check(bundle.arcs.size() == 2 && bundle.arcs[0] == std::pair<std::string, std::string>{"in_l", "m3/in"}, "cables misread");
    const auto blocks = saved_blocks(bundle);
    check(blocks.size() == 2 && blocks[0].name == "Boost" && blocks[1].name == "m4", "blocks for a load misnamed");
    // Neither plugin is in this build's catalogue: placeholders keep their cables for repair.
    const auto graph = bundle_graph(bundle);
    check(graph.patch_id == "p-0001" && graph.name == "Saved one" && graph.modules.size() == 4 && graph.connections.size() == 2, "bundle graph wrong");
    check(find_module(graph, 3) && !module_available(*find_module(graph, 3)) && find_module(graph, 3)->ports.size() == 2, "unknown block lost its jacks");
    check(validate_graph(graph).empty(), "bundle graph does not validate");

    Bundle broken;
    check(!read_bundle("<> a <http://example.org/Nothing> .", broken).empty(), "a file without a graph was accepted");
    check(!read_bundle("this is not turtle", broken).empty(), "unreadable text was accepted");
    check(!read_bundle("@prefix ingen: <http://drobilla.net/ns/ingen#> .\n<m3> a ingen:Block .\n<> a ingen:Graph .", broken).empty(), "a block without a plugin was accepted");

    // What the simulator writes reads back as the same graph.
    auto demo = parallel_demo_graph();
    demo.patch_id = "p-demo"; demo.dirty = false;
    demo.modules[2].bypassed = true;
    demo.modules[3].parameters[0].value = 25.5;
    demo.name = "Quotes \"and\" accents \xc3\xa9";
    Bundle written;
    check(read_bundle(write_main_ttl(demo), written).empty(), "the simulator's bundle was not readable");
    const auto back = bundle_graph(written);
    check(back.name == demo.name && back.patch_id == demo.patch_id && back.modules.size() == demo.modules.size() && back.connections.size() == demo.connections.size(), "round trip lost structure");
    for (const auto& m : demo.modules) {
        const auto* r = find_module(back, m.id);
        // The endpoints sit where every graph has them; only modules keep a position.
        check(r && r->uri == m.uri && r->name == m.name && (m.fixed || (r->x == m.x && r->y == m.y)) && r->bypassed == m.bypassed, "round trip lost a module");
        for (std::size_t i = 0; i < m.parameters.size(); ++i) check(r->parameters[i].value == m.parameters[i].value, "round trip lost a value");
    }
    for (const auto& c : demo.connections) check(std::find(back.connections.begin(), back.connections.end(), c) != back.connections.end(), "round trip lost a cable");
    // MIDI IN to MIDI OUT is saved on the graph's own MIDI ports, typed for the engine.
    auto thru = demo;
    thru.connections.push_back({1, 2, "midi", "midi"});
    const auto thru_text = write_main_ttl(thru);
    Bundle with_midi;
    check(thru_text.find("<midi_in>\n\tlv2:symbol \"midi_in\" ;\n\ta atom:AtomPort , lv2:InputPort ;\n\tatom:bufferType atom:Sequence ;\n\tatom:supports midi:MidiEvent") != std::string::npos &&
          thru_text.find("ingen:tail <midi_in> ;\n\t\tingen:head <midi_out>") != std::string::npos, "MIDI endpoints or cable not written");
    check(read_bundle(thru_text, with_midi).empty() && bundle_graph(with_midi).connections.back() == Connection{1, 2, "midi", "midi"}, "MIDI cable lost in a round trip");

    // The patch tempo and a module's file come back; a bundle from before
    // tempo was saved plays at the default one.
    auto timed = demo;
    timed.tempo = 87.5;
    timed.modules[3].file = "/files/nam/A \"quoted\" amp.nam";
    Bundle with_tempo;
    check(read_bundle(write_main_ttl(timed), with_tempo).empty() && with_tempo.tempo == 87.5, "tempo not written to the bundle");
    const auto timed_back = bundle_graph(with_tempo);
    timed.load_messages = {{1, 7, 100}, {16, 0, 127}};
    Bundle with_messages;
    check(read_bundle(write_main_ttl(timed), with_messages).empty() && with_messages.load_messages == timed.load_messages &&
          bundle_graph(with_messages).load_messages == timed.load_messages && with_tempo.load_messages.empty(), "control changes on load lost in a round trip");
    check(timed_back.tempo == 87.5 && find_module(timed_back, timed.modules[3].id)->file == timed.modules[3].file && find_module(timed_back, timed.modules[2].id)->file.empty(),
          "tempo or file lost in a round trip");
    auto text = write_main_ttl(timed);
    text.erase(text.find(" ;\n\tpeepo:tempo "), std::string(" ;\n\tpeepo:tempo \"87.5\"^^xsd:double").size());
    Bundle older;
    check(read_bundle(text, older).empty() && !older.tempo && bundle_graph(older).tempo == default_tempo, "a bundle without tempo did not play at the default");
    // A stomp switching two modules is one property holding both, and comes back as two.
    auto shared = demo;
    Assignment one{"stomp1", demo.modules[2].id}, two{"stomp1", demo.modules[3].id};
    one.action = two.action = ControlAction::Bypass;
    one.plugin = demo.modules[2].uri; two.plugin = demo.modules[3].uri;
    shared.assignments = {one, two};
    const auto shared_text = write_main_ttl(shared);
    check(shared_text.find("peepo:control-stomp1") == shared_text.rfind("peepo:control-stomp1"), "a shared stomp was written as two properties");
    Bundle with_stomp;
    check(read_bundle(shared_text, with_stomp).empty(), "a bundle with a shared stomp was not readable");
    const auto stomp_back = bundle_graph(with_stomp);
    check(bypass_assignments(stomp_back, "stomp1").size() == 2 && stomp_back.assignments[0].module == one.module && stomp_back.assignments[1].module == two.module &&
          validate_graph(stomp_back).empty(), "a shared stomp did not come back with both modules");
    // Scenes are graph properties: they come back with their current scene, an orphan kept.
    {
        auto staged = demo;
        const auto marked = staged.modules[3].id;
        const auto symbol = staged.modules[3].parameters[0].symbol;
        check(add_scene(staged).empty() && mark_scene_parameter(staged, marked, symbol).empty() && add_scene(staged).empty() &&
              rename_scene(staged, 1, "Solo \"loud\"").empty(), "scenes not made");
        check(record_scene_bypass(staged, staged.modules[2].id, false) && record_scene_parameter(staged, marked, symbol, 40), "scene edits not recorded");
        staged.scenes.scenes[0].entries.push_back({{0, "m77", "urn:gone", {}}, 1});
        staged.scenes.current = 0;
        Bundle with_scenes;
        check(read_bundle(write_main_ttl(staged), with_scenes).empty() && with_scenes.scenes.count == 2 && with_scenes.scenes.current == 1, "scenes not written to the bundle");
        const auto scenes_back = bundle_graph(with_scenes);
        auto block_of = [](InstanceId id) { return "m" + std::to_string(id); };
        const auto before = encode_scenes(staged.scenes, block_of), after = encode_scenes(scenes_back.scenes, block_of);
        check(before.count == after.count && before.current == after.current && before.parameters == after.parameters && before.scenes == after.scenes &&
              scenes_back.scenes.scenes[1].name == "Solo \"loud\"" && validate_graph(scenes_back).empty(), "scenes lost in a round trip");
        check(scene_changes(scenes_back, 0).empty() && scene_changes(scenes_back, 1).size() == 2, "the saved current scene is not what the saved values are");
        Bundle without;
        check(write_main_ttl(demo).find("peepo:scene-") == std::string::npos && read_bundle(write_main_ttl(demo), without).empty() && bundle_graph(without).scenes.scenes.empty(),
              "a patch without scenes came back with some");
    }
    // A plugin that bypasses itself is saved with its own switch off and its block on.
    {
        ModuleDefinition own{"urn:test:self-bypassed", "Self", "Delay", "", 0x7bbaed, {{"in", Direction::Input}, {"out", Direction::Output}}, {}};
        own.enable_symbol = "enabled";
        own.port_bypass = true;
        register_definitions({own});
        auto selfish = demo;
        selfish.modules.push_back({90, own.uri, "Self", 600, 24, false, true, {}, own.ports, {}});
        const auto selfish_text = write_main_ttl(selfish);
        const auto at = selfish_text.find("<m90>");
        check(at != std::string::npos && selfish_text.find("ingen:enabled false", at) > selfish_text.find("<m90/enabled>", at) &&
              selfish_text.find("<m90/enabled>\n\tingen:value \"0", at) != std::string::npos, "a self-bypassed module was saved with the engine's bypass");
        Bundle with_self;
        check(read_bundle(selfish_text, with_self).empty() && find_module(bundle_graph(with_self), 90)->bypassed, "a self-bypassed module came back on");
    }
    // A module that spills is saved with the engine's bypass and its mark, as the engine saves it.
    {
        ModuleDefinition reverb{"urn:test:spilled", "Spill", "Reverb", "", 0x81c9b8, {{"in", Direction::Input}, {"out", Direction::Output}}, {}};
        reverb.spill_bypass = true;
        register_definitions({reverb});
        auto spilling = demo;
        spilling.modules.push_back({91, reverb.uri, "Spill", 600, 24, false, true, {}, reverb.ports, {}});
        const auto text = write_main_ttl(spilling);
        const auto at = text.find("<m91>"), end = text.find(" .\n", at);
        check(at != std::string::npos && text.find("ingen:enabled false", at) < end && text.find("peepo:spillBypass true", at) < end &&
              text.find("peepo:spillBypass") == text.rfind("peepo:spillBypass"), "a spilling module was saved without its mark, or another module with one");
        Bundle with_spill;
        check(read_bundle(text, with_spill).empty() && find_module(bundle_graph(with_spill), 91)->bypassed, "a spilling module came back on");
    }
    // Files are listed from the store's folder, by name, with the extension asked for.
    const auto files_root = fs::temp_directory_path() / "peepo-file-store-tests";
    fs::remove_all(files_root);
    FileStore files(files_root, "/engine/files");
    check(files.open().empty() && fs::is_directory(files_root / "nam") && fs::is_directory(files_root / "ir" / "cab") && fs::is_directory(files_root / "ir" / "reverb"),
          "file store folders not made");
    for (const char* name : {"b amp.NAM", "a amp.nam", ".hidden.nam", "notes.txt", "cab.wav"}) write(files_root / (std::string(name).ends_with(".wav") ? "ir/cab" : "nam") / name, "x");
    write(files_root / "ir" / "reverb" / "Hall.wav", "x");
    const auto listed = files.list("nam", {"nam"});
    check(listed.size() == 2 && listed[0].name == "a amp.nam" && listed[1].name == "b amp.NAM" && listed[0].engine_path == "/engine/files/nam/a amp.nam",
          "models not listed by name, with their engine path");
    const auto cabs = files.list("ir/cab", {"wav", "flac"});
    check(cabs.size() == 1 && cabs[0].engine_path == "/engine/files/ir/cab/cab.wav" && files.list("ir/reverb", {"wav"}).size() == 1 && files.list("ir/reverb", {"wav"})[0].name == "Hall.wav" &&
          files.list("ir", {"wav"}).empty() && files.list("../nam", {"nam"}).empty() && files.list("ir/../nam", {"nam"}).empty() && files.list("ir/cab/x", {"wav"}).empty() &&
          files.list("/ir", {"wav"}).empty() && files.list("ir/cab", {"nam"}).empty(), "impulse responses listed wrongly, or outside the store");
    check(file_name("/engine/files/nam/a amp.nam") == "a amp.nam" && file_name("C:\\x\\y.wav") == "y.wav" && file_name("plain") == "plain", "file names misread");
    fs::remove_all(files_root);
}

void store(const fs::path& root) {
    check(engine_path("/var/lib/peepo/patches") && engine_path("C:/Users/me/patches") && engine_path("/mnt/c/Users/me/AppData/Local/Peepo/patches"), "plain paths refused");
    check(!engine_path("relative/patches") && !engine_path("/with space/x") && !engine_path("/a%20b") && !engine_path("/a/../b") && !engine_path("/a>b"), "unsafe path accepted");
    check(wsl_path("C:\\Users\\me\\x") == "/mnt/c/Users/me/x" && wsl_path("/home/me") == "/home/me", "WSL path translation wrong");
    check(!PatchStore(root, "/with space").open().empty(), "a store the engine cannot be told about opened");

    PatchStore patches(root);
    check(patches.open().empty() && patches.usable() && patches.list().empty(), "empty store did not open");
    const auto id = patches.new_id();
    check(valid_patch_id(id) && id != patches.new_id(), "identities are not unique or valid");
    std::string issue;
    auto first = patches.stage(id, issue);
    check(first && first->revision == 1 && first->location.starts_with("file:///") && first->location.ends_with("/" + id + "/.staging-r000001.ingen"), "first staging wrong");
    auto graph = demo_graph(); graph.name = "Clean boost";
    check(!patches.publish(*first).empty() && patches.list().empty(), "an empty staging directory was published");
    engine_writes(*first, graph);
    check(patches.publish(*first).empty(), "complete revision not published");
    auto listed = patches.list();
    check(listed.size() == 1 && listed[0].id == id && listed[0].name == "Clean boost" && listed[0].revision == 1 && listed[0].issue.empty(), "published patch not listed");
    check(listed[0].location.ends_with("/" + id + "/r000001.ingen") && saved_blocks(listed[0].bundle).size() == 3, "listed revision wrong");

    // A save whose bundle carries another identity is not published.
    auto wrong = patches.stage(id, issue);
    engine_writes(*wrong, graph);
    write(wrong->path / "main.ttl", write_main_ttl([&] { auto g = graph; g.patch_id = "p-other"; return g; }()));
    check(!patches.publish(*wrong).empty(), "bundle with another identity published");
    patches.discard(*wrong);
    check(patches.find(id)->revision == 1, "a refused save changed the stored patch");

    // Revisions: newer ones replace, two are kept.
    for (const char* name : {"Clean boost 2", "Clean boost 3"}) {
        auto next = patches.stage(id, issue);
        graph.name = name; engine_writes(*next, graph);
        check(patches.publish(*next).empty(), "later revision not published");
    }
    check(patches.find(id)->revision == 3 && patches.find(id)->name == "Clean boost 3", "newest revision not offered");
    check(!fs::exists(root / id / "r000001.ingen") && fs::exists(root / id / "r000002.ingen"), "revisions not pruned to two");

    // An interrupted save: staging is left behind, the stored patch is untouched, and reopening clears it.
    auto interrupted = patches.stage(id, issue);
    write(interrupted->path / "main.ttl", "@prefix ingen: <http://drob");
    check(patches.find(id)->revision == 3 && patches.list().size() == 1, "an interrupted save was offered");
    PatchStore reopened(root);
    check(reopened.open().empty() && !fs::exists(interrupted->path) && reopened.find(id)->revision == 3, "interrupted save not cleared on open");

    // The newest revision damaged after it was published: the previous one is offered, and said so.
    write(root / id / "r000003.ingen" / "main.ttl", "damaged");
    auto fallback = reopened.find(id);
    check(fallback && fallback->revision == 2 && fallback->name == "Clean boost 2" && !fallback->issue.empty(), "damaged revision did not fall back");
    write(root / id / "r000002.ingen" / "main.ttl", "damaged too");
    fallback = reopened.find(id);
    check(fallback && fallback->revision == 0 && fallback->issue.find("No readable revision") == 0, "unreadable patch offered as loadable");
    // Saving again repairs it: the next revision number is still new.
    auto repair = reopened.stage(id, issue);
    check(repair->revision == 4, "revision number reused");
    graph.name = "Repaired"; engine_writes(*repair, graph);
    check(reopened.publish(*repair).empty() && reopened.find(id)->revision == 4 && reopened.find(id)->issue.empty(), "repair save failed");

    // Startup choice, and deletion.
    check(reopened.startup().empty() && reopened.set_startup(id).empty() && reopened.startup() == id && reopened.list()[0].startup, "startup choice not recorded");
    const auto other = reopened.new_id();
    auto second = reopened.stage(other, issue);
    graph.name = "Another"; engine_writes(*second, graph);
    check(reopened.publish(*second).empty() && reopened.list().size() == 2 && reopened.list()[0].name == "Another", "list not ordered by name");
    // Loading one falls back on the patch playing, as stored; never on itself, or on nothing loadable.
    const auto another = reopened.find(other), repaired = reopened.find(id);
    const auto load = load_command(*another, repaired);
    check(load.location == another->location && load.patch_id == other && load.fallback && load.fallback->location == repaired->location &&
          load.fallback->patch_id == id && load.fallback->blocks.size() == 3 && !load.fallback->fallback, "load falls back on the wrong patch");
    check(!load_command(*another, another).fallback && !load_command(*another, std::nullopt).fallback && !load_command(*another, fallback).fallback,
          "load falls back on itself or on nothing loadable");
    check(reopened.remove(id).empty() && reopened.list().size() == 1 && reopened.startup().empty() && !fs::exists(root / id), "delete left the patch or its startup choice");
    check(!reopened.remove(id).empty(), "deleting twice succeeded");
    // A first save that fails leaves no empty patch behind.
    const auto never = reopened.new_id();
    auto failed = reopened.stage(never, issue);
    reopened.discard(*failed);
    check(!fs::exists(root / never) && reopened.list().size() == 1, "failed first save left a directory");
}

// Bundles from elsewhere: without project properties, from a newer schema, and
// with plugin state that refers to files.
// A WAV file as the parts say, with a chunk before the format as some
// editors write; `cut` drops that many bytes off the end.
std::string wav(unsigned format, unsigned channels, unsigned rate, unsigned bits, std::uint32_t frames, std::size_t cut = 0) {
    auto le = [](std::string& out, std::uint32_t v, int bytes) { for (int i = 0; i < bytes; ++i) out += static_cast<char>((v >> (8 * i)) & 0xff); };
    const std::uint32_t block = channels * bits / 8, data = frames * block;
    std::string out = "RIFF";
    le(out, 36 + 12 + data, 4);
    out += "WAVELIST";
    le(out, 4, 4);
    out += "INFOfmt ";
    le(out, 16, 4);
    le(out, format, 2);
    le(out, channels, 2);
    le(out, rate, 4);
    le(out, rate * block, 4);
    le(out, block, 2);
    le(out, bits, 2);
    out += "data";
    le(out, data, 4);
    out += std::string(data, '\0');
    out.resize(out.size() - cut);
    return out;
}
// A 16-bit FLAC's marker and STREAMINFO, then some bytes standing for its frames.
std::string flac(unsigned rate, unsigned channels, std::uint64_t samples) {
    std::string info(34, '\0');
    info[10] = static_cast<char>(rate >> 12);
    info[11] = static_cast<char>(rate >> 4);
    info[12] = static_cast<char>(((rate & 0xf) << 4) | ((channels - 1) << 1));
    info[13] = static_cast<char>((15 << 4) | ((samples >> 32) & 0xf));
    for (int i = 0; i < 4; ++i) info[14 + i] = static_cast<char>(samples >> (24 - 8 * i));
    return std::string("fLaC\x80\0\0\x22", 8) + info + std::string(64, '\x55');
}
const std::string model = R"({"version": "0.5.4", "architecture": "WaveNet", "config": {"layers": [{"input_size": 1, "gated": false, "name": "a \"b\""}]}, "weights": [0.1, -2e-3, 3]})";

void imports(const fs::path& root) {
    const auto stick = root / "stick";
    write(stick / "Amp One.nam", model);
    write(stick / "deep" / "er" / "Amp Two.nam", R"({"weights":[],"architecture":"LSTM"})");
    write(stick / "No weights.nam", R"({"architecture": "WaveNet", "config": {}})");
    write(stick / "No architecture.nam", R"({"weights": [1, 2]})");
    write(stick / "Cut.nam", model.substr(0, model.size() - 20));
    write(stick / "Text.nam", "not a model at all");
    write(stick / "Trailing.nam", model + " x");
    write(stick / "Huge.nam", R"({"architecture": "WaveNet", "weights": [)" + std::string(max_model_bytes, ' ') + "]}");
    write(stick / "Cabs" / "4x12.wav", wav(1, 2, 48000, 24, 9600));
    write(stick / "IR" / "My Reverbs" / "Hall.flac", flac(44100, 2, 44100 * 3));
    write(stick / "Room Mics" / "Booth.wav", wav(0xFFFE, 1, 48000, 32, 100));
    write(stick / "loose" / "Short.wav", wav(3, 1, 48000, 32, 12000));
    write(stick / "loose" / "Long.wav", wav(1, 1, 1000, 8, 31000));
    write(stick / "loose" / "Five.wav", wav(1, 5, 48000, 16, 100));
    write(stick / "loose" / "Cut.wav", wav(1, 1, 48000, 16, 1000, 10));
    write(stick / "loose" / "Empty.wav", wav(1, 1, 48000, 16, 0));
    write(stick / "loose" / "ADPCM.wav", wav(2, 1, 48000, 4, 100));
    write(stick / "loose" / "Garbage.wav", "RIFF....WAVEjunk");
    write(stick / "loose" / "Unknown length.flac", flac(48000, 1, 0));
    write(stick / "loose" / "Not flac.flac", "ID3 tags first");
    write(stick / "cabinet" / "room" / "Nearest wins.wav", wav(1, 1, 48000, 16, 100));
    write(stick / "._Amp One.nam", "macOS resource fork");
    write(stick / ".Trashes" / "Old.nam", model);
    write(stick / "System Volume Information" / "x.wav", wav(1, 1, 48000, 16, 100));
    write(stick / "notes.txt", "ignored");
    auto deep = stick;
    for (int i = 0; i < max_folder_depth + 2; ++i) deep /= "d";
    write(deep / "Too deep.nam", model);

    const auto found = find_files(stick);
    const auto named = [](const std::vector<FoundFile>& list, const std::string& name) -> const FoundFile* {
        const auto at = std::find_if(list.begin(), list.end(), [&](const FoundFile& f) { return f.name == name; });
        return at == list.end() ? nullptr : &*at;
    };
    for (const char* name : {"Amp One.nam", "Amp Two.nam", "4x12.wav", "Hall.flac", "Short.wav", "Nearest wins.wav"}) check(named(found.files, name), name);
    check(found.files.size() == 6 && found.truncated, "the stick's usable files not all found, or the deep folder not reported");
    for (const char* name : {"No weights.nam", "No architecture.nam", "Cut.nam", "Text.nam", "Trailing.nam", "Huge.nam", "Booth.wav", "Long.wav", "Five.wav", "Cut.wav",
                             "Empty.wav", "ADPCM.wav", "Garbage.wav", "Unknown length.flac", "Not flac.flac"})
        check(named(found.rejected, name) && !named(found.rejected, name)->check.problem.empty(), name);
    check(found.rejected.size() == 15, "hidden, system or other files were mentioned");
    check(found.files[0].name == "4x12.wav" && found.files[1].name == "Amp One.nam", "found files not by name");
    const auto& amp_two = *named(found.files, "Amp Two.nam");
    check(amp_two.kind == FileKind::Model && amp_two.folder == "deep/er" && named(found.files, "Amp One.nam")->folder.empty(), "a model's folder misread");
    const auto& cab = *named(found.files, "4x12.wav");
    check(cab.kind == FileKind::Cabinet && cab.check.channels == 2 && cab.check.seconds == 0.2 && sounds_like_cabinet(cab.check.seconds), "a cabinet folder not recognised");
    const auto& hall = *named(found.files, "Hall.flac");
    check(hall.kind == FileKind::Reverb && hall.check.seconds == 3 && hall.check.channels == 2 && !sounds_like_cabinet(hall.check.seconds), "a reverb folder or FLAC misread");
    check(named(found.files, "Short.wav")->kind == FileKind::Unsorted && named(found.files, "Short.wav")->check.seconds == 0.25, "a loose impulse response was sorted");
    check(named(found.files, "Nearest wins.wav")->kind == FileKind::Reverb, "the nearest folder did not decide");
    check(named(found.rejected, "Huge.nam")->check.problem.starts_with("Larger than 10 MB") && named(found.rejected, "Long.wav")->check.problem.starts_with("31.0 s long") &&
          named(found.rejected, "Five.wav")->check.problem.starts_with("5 channels") && named(found.rejected, "Cut.wav")->check.problem == "The file is cut short",
          "rejections not explained");

    // Into the store: copied with no temporary left, then skipped, then numbered.
    FileStore store(root / "files");
    check(store.open().empty(), "file store did not open");
    const auto copied = import_file(store, *named(found.files, "Amp One.nam"), FileKind::Model);
    check(copied.result == ImportResult::Copied && copied.name == "Amp One.nam" && fs::file_size(root / "files" / "nam" / "Amp One.nam") == model.size(), "model not copied");
    check(import_file(store, cab, FileKind::Cabinet).result == ImportResult::Copied && fs::exists(root / "files" / "ir" / "cab" / "4x12.wav"), "cabinet not copied");
    const auto& loose = *named(found.files, "Short.wav");
    check(import_file(store, loose, FileKind::Unsorted).result == ImportResult::Failed, "an unsorted file copied without an answer");
    check(import_file(store, loose, FileKind::Model).result == ImportResult::Failed, "an impulse response went to the models");
    check(import_file(store, loose, FileKind::Reverb).result == ImportResult::Copied && fs::exists(root / "files" / "ir" / "reverb" / "Short.wav"), "the player's answer not used");
    check(import_file(store, *named(found.rejected, "Text.nam"), FileKind::Model).result == ImportResult::Failed, "a refused file was copied");
    const auto again = import_file(store, *named(found.files, "Amp One.nam"), FileKind::Model);
    check(again.result == ImportResult::Skipped && again.name == "Amp One.nam", "the same file imported twice");
    // Another model with the name: numbered, and skipped the next time.
    const std::string other_model = R"({"architecture": "LSTM", "weights": [1]})";
    write(root / "stick2" / "Amp One.nam", other_model);
    const auto other = find_files(root / "stick2");
    check(other.files.size() == 1 && !other.truncated, "second stick misread");
    const auto numbered = import_file(store, other.files[0], FileKind::Model);
    check(numbered.result == ImportResult::Copied && numbered.name == "Amp One (2).nam" && fs::file_size(root / "files" / "nam" / "Amp One (2).nam") == other_model.size(),
          "a different file with the name was not numbered");
    check(import_file(store, other.files[0], FileKind::Model).result == ImportResult::Skipped, "a numbered file imported twice");
    // The stick changed after it was looked at: nothing lands.
    auto changed = other.files[0];
    changed.name = "Changed.nam";
    write(root / "stick2" / "Amp One.nam", R"({"architecture": "LSTM", "weights": ]1]})");
    check(import_file(store, changed, FileKind::Model).result == ImportResult::Failed && !fs::exists(root / "files" / "nam" / "Changed.nam"), "a damaged file was copied");
    write(root / "stick2" / "Amp One.nam", "{}");
    check(import_file(store, changed, FileKind::Model).result == ImportResult::Failed && !fs::exists(root / "files" / "nam" / "Changed.nam"), "a shortened file was copied");
    std::atomic<bool> stop{true};
    check(import_file(store, hall, FileKind::Reverb, &stop).message == "Stopped" && !fs::exists(root / "files" / "ir" / "reverb" / "Hall.flac"), "a stopped copy landed");
    for (const auto& entry : fs::recursive_directory_iterator(root / "files")) check(!entry.path().filename().string().starts_with(".import-"), "a temporary copy was left");
    // An import cut off by a power cut is cleared when the store opens.
    write(root / "files" / "ir" / "cab" / ".import-0123456789abcdef.part", "half");
    check(store.open().empty() && !fs::exists(root / "files" / "ir" / "cab" / ".import-0123456789abcdef.part") && fs::exists(root / "files" / "ir" / "cab" / "4x12.wav"),
          "an unfinished import not cleared");
    check(find_files(root / "nowhere").files.empty(), "a missing stick listed files");
}
// Sticks as peepo-usb reports them, the worker that looks through and
// imports one off the screen's thread, and deleting a file from the store.
void sticks(const fs::path& root) {
    const auto run = root / "run";
    check(list_usb_sticks(run).empty(), "a missing run directory listed sticks");
    write(run / "sdb1.info", "state=mounted\nlabel=KINGSTON\ntype=vfat\nmessage=\n");
    write(run / "sdc1.info", "state=unreadable\r\nlabel=\r\ntype=ext4\r\nmessage=formatted as ext4\r\n");
    write(run / "sdd.info", "state=refused\nlabel=SYSTEM\ntype=ext4\nmessage=the disk is in use by the system (/)\n");
    write(run / ".sde1.info.123", "state=mounted\n");
    write(run / "sdb1" / "Cabs" / "4x12.wav", wav(1, 1, 48000, 16, 4800));
    write(run / "sdb1" / "Amp.nam", model);
    write(run / "sdb1" / "Loose.wav", wav(1, 1, 48000, 16, 96000));
    const auto found = list_usb_sticks(run);
    check(found.size() == 3 && found[0].device == "sdb1" && found[0].state == UsbStick::State::Mounted && found[0].label == "KINGSTON" && found[0].type == "vfat" &&
          found[0].root == run / "sdb1" && found[1].state == UsbStick::State::Unreadable && found[1].label.empty() && found[1].message == "formatted as ext4" &&
          found[2].state == UsbStick::State::Refused && found[2].message == "the disk is in use by the system (/)", "sticks misread");

    FileStore files(root / "files");
    check(files.open().empty(), "files store did not open");
    ImportWorker worker;
    const auto wait = [&] {
        for (int i = 0; i < 500 && worker.status().busy; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        check(!worker.status().busy, "the worker did not finish");
    };
    const auto before = worker.serial();
    worker.scan(found[0].root);
    wait();
    auto status = worker.status();
    check(worker.serial() != before && status.job == ImportWorker::Status::Job::Scan && status.root == found[0].root && status.found.files.size() == 3, "the stick was not looked through");
    std::vector<ImportWorker::Item> items;
    for (const auto& f : status.found.files) items.push_back({f, f.kind == FileKind::Unsorted ? FileKind::Reverb : f.kind});
    worker.import(files, items);
    wait();
    status = worker.status();
    check(status.job == ImportWorker::Status::Job::Import && status.done == 3 && status.copied == 3 && status.failed.empty() && !status.stopped && status.found.files.size() == 3 &&
          fs::exists(root / "files" / "ir" / "reverb" / "Loose.wav") && fs::exists(root / "files" / "ir" / "cab" / "4x12.wav"), "the import did not copy every file");
    worker.import(files, items);
    wait();
    status = worker.status();
    check(status.copied == 0 && status.skipped == 3, "an import again did not skip what is there");
    // Stopped before it starts: nothing copied, nothing left behind.
    fs::remove(root / "files" / "nam" / "Amp.nam");
    worker.import(files, items);
    worker.stop();
    wait();
    check(worker.status().stopped || worker.status().done == 3, "stop was not reported");

    // Delete: a listed file goes, nothing outside the store does.
    const auto cabs = files.list("ir/cab", {"wav"});
    check(cabs.size() == 1 && files.remove(cabs[0]).empty() && !fs::exists(cabs[0].path), "a stored file was not deleted");
    check(!files.remove(cabs[0]).empty(), "deleting a file twice was not refused");
    write(root / "outside.wav", "x");
    check(!files.remove({"outside.wav", root / "outside.wav", "outside.wav"}).empty() && fs::exists(root / "outside.wav"), "a file outside the store was deleted");
    check(!files.remove({"x.wav", root / "files" / "ir" / "cab" / ".." / ".." / ".." / "outside.wav", "x"}).empty() && fs::exists(root / "outside.wav"), "a path out of the store was followed");
}

void foreign_bundles(const fs::path& root) {
    PatchStore patches(root);
    check(patches.open().empty(), "store did not open");
    // An Ingen bundle with none of the project's properties: no identity, name or layout.
    write(root / "p-bare" / "r000001.ingen" / "main.ttl",
          "@prefix ingen: <http://drobilla.net/ns/ingen#> .\n@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n"
          "<m3> a ingen:Block ; lv2:prototype <urn:peepo:demo:drive> .\n<m3/drive> ingen:value 70.0 .\n"
          "<> a ingen:Graph , lv2:Plugin ; ingen:block <m3> ; ingen:arc [ ingen:tail <in_l> ; ingen:head <m3/in> ] .\n");
    const auto bare = patches.find("p-bare");
    check(bare && bare->revision == 1 && bare->issue.empty() && bare->name == "p-bare", "bundle without project properties not offered");
    SimulatedEngine engine;
    auto events = run(engine, LoadPatch{bare->location, saved_blocks(bare->bundle), bare->id});
    const auto& loaded = engine.snapshot();
    check(events[0].accepted && loaded.patch_id == "p-bare" && loaded.name == "Untitled" && loaded.modules.size() == 3, "bare bundle did not load under its folder's identity");
    const auto* drive = find_module(loaded, 3);
    check(drive && drive->name == "Drive" && drive->x == 184 && drive->y == 276 && drive->parameters[0].value == 70 && loaded.connections.size() == 1, "bare bundle lost its graph or got no default layout");
    // A newer schema is not guessed at.
    auto newer = demo_graph(); newer.patch_id = "p-newer";
    auto text = write_main_ttl(newer);
    const std::string schema = "peepo:schemaVersion 1";
    text.replace(text.find(schema), schema.size(), "peepo:schemaVersion 2");
    write(root / "p-newer" / "r000001.ingen" / "main.ttl", text);
    const auto refused = patches.find("p-newer");
    check(refused && refused->revision == 0 && refused->issue.find("newer Peepo") != std::string::npos, "newer schema offered as loadable");
    // Plugin state that saved a file beside itself, as the example `params` plugin does.
    const auto stateful = root / "p-0001" / "r000001.ingen";
    write(stateful / "main.ttl", recorded);
    write(stateful / "m4" / "state.ttl",
          "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n@prefix pset: <http://lv2plug.in/ns/ext/presets#> .\n@prefix state: <http://lv2plug.in/ns/ext/state#> .\n"
          "<> a pset:Preset ; lv2:appliesTo <http://lv2plug.in/plugins/eg-params> ;\n\tstate:state [ <http://lv2plug.in/plugins/eg-params#path> <params.ttl> ;\n"
          "\t\t<http://lv2plug.in/plugins/eg-params#sample> <file:///nowhere/at/all.wav> ] .\n");
    auto found = patches.find("p-0001");
    check(found && found->revision == 1 && found->missing_files.size() == 2 && found->missing_files[0] == "m4: params.ttl", "missing state files not reported");
    write(stateful / "m4" / "params.ttl", "");
    found = patches.find("p-0001");
    check(found->missing_files.size() == 1 && found->missing_files[0] == "m4: file:///nowhere/at/all.wav", "a file kept with the bundle reported missing");
    // When the engine sees the folder under another name, its absolute paths are not this program's to check.
    PatchStore elsewhere(root, "/srv/patches");
    check(elsewhere.open().empty() && elsewhere.find("p-0001")->missing_files.empty(), "engine-side path checked on the wrong filesystem");
    fs::remove(stateful / "m4" / "state.ttl");
    check(patches.find("p-0001")->missing_files == std::vector<std::string>{"m4: m4/state.ttl"}, "missing state file not reported");
    // A saved model whose name has a space, as the engine names it: escaped.
    write(stateful / "m4" / "state.ttl",
          "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n@prefix pset: <http://lv2plug.in/ns/ext/presets#> .\n@prefix state: <http://lv2plug.in/ns/ext/state#> .\n"
          "<> a pset:Preset ; lv2:appliesTo <http://lv2plug.in/plugins/eg-params> ;\n\tstate:state [ <http://lv2plug.in/plugins/eg-params#path> <A1%20standard.nam> ] .\n");
    write(stateful / "m4" / "A1 standard.nam", "x");
    check(patches.find("p-0001")->missing_files.empty(), "an escaped file name kept with the bundle reported missing");
    fs::remove(stateful / "m4" / "A1 standard.nam");
    check(patches.find("p-0001")->missing_files == std::vector<std::string>{"m4: A1%20standard.nam"}, "a missing escaped file not reported");
}

void simulated(const fs::path& root) {
    PatchStore patches(root);
    check(patches.open().empty(), "store did not open");
    SimulatedEngine engine;
    std::string issue;
    auto graph = engine.snapshot();
    check(graph.patch_id.empty(), "a fresh graph claimed an identity");
    run(engine, SetParameter{3, "drive", 80});
    check(engine.snapshot().dirty, "edit did not mark the graph");
    // Two scenes, the second current when saved: drive 20 in the first, 80 in the second.
    for (Command c : {Command{AddScene{}}, Command{MarkSceneParameter{3, "drive"}}, Command{AddScene{}}, Command{SelectScene{0}},
                      Command{SetParameter{3, "drive", 20, true}}, Command{SelectScene{1}}, Command{SetSceneMode{true}}})
        check(run(engine, c)[0].accepted, "simulated scene command refused");
    check(find_module(engine.snapshot(), 3)->parameters[0].value == 80 && engine.snapshot().scenes.current == 1 &&
          (scene_changes(engine.snapshot(), 0) == std::vector<SceneChange>{{3, "drive", 20}}), "simulated scenes wrong");
    graph = engine.snapshot();
    const auto id = patches.new_id();
    auto staging = patches.stage(id, issue);
    auto events = run(engine, SaveGraph{SaveScope::Persistent, false, staging->location, id});
    check(events.size() == 1 && events[0].accepted && !engine.snapshot().dirty && engine.snapshot().patch_id == id, "simulated save failed");
    check(run(engine, SaveGraph{SaveScope::Persistent, true, staging->location, id})[0].accepted == false, "simulator claimed plugin state");
    check(patches.publish(*staging).empty(), "simulated save not publishable");
    const auto stored = patches.find(id);
    check(stored && stored->revision == 1 && stored->name == graph.name, "simulated save not stored");

    run(engine, NewPatch{"Blank"});
    check(engine.snapshot().patch_id.empty() && engine.snapshot().modules.size() == 2, "new patch kept the identity");
    events = run(engine, LoadPatch{stored->location, saved_blocks(stored->bundle)});
    check(events[0].accepted && events[0].message.find("Loaded") == 0, "simulated load failed");
    const auto& loaded = engine.snapshot();
    check(loaded.patch_id == id && !loaded.dirty && loaded.modules.size() == graph.modules.size() && loaded.connections.size() == graph.connections.size(), "loaded graph differs");
    check(find_module(loaded, 3)->parameters[0].value == 80, "loaded value differs");
    // Loading restores the scene current when saved; its values are the saved ones.
    check(loaded.scenes.scenes.size() == 2 && loaded.scenes.current == 1 && loaded.scenes.scene_mode && scene_changes(loaded, 1).empty() &&
          (scene_changes(loaded, 0) == std::vector<SceneChange>{{3, "drive", 20}}), "loading did not restore the current scene");

    // A plugin the engine lacks: refused before anything changes, as Ingen would load none of it.
    auto foreign = engine.snapshot();
    foreign.modules.push_back({9, "urn:test:not-installed", "Stranger", 600, 300, false, false, {}, {{"in", Direction::Input}, {"out", Direction::Output}}});
    foreign.connections.push_back({5, 9, "out", "in"});
    auto foreign_staging = patches.stage(id, issue);
    engine_writes(*foreign_staging, foreign);
    check(patches.publish(*foreign_staging).empty(), "foreign revision not published");
    const auto with_stranger = patches.find(id);
    run(engine, NewPatch{"Blank"});
    events = run(engine, LoadPatch{with_stranger->location, saved_blocks(with_stranger->bundle)});
    check(!events[0].accepted && events[0].message.find("Stranger") != std::string::npos && engine.snapshot().name == "Blank", "missing plugin not refused before loading");
    check(!run(engine, LoadPatch{"file:///nowhere/x.ingen"})[0].accepted && !run(engine, LoadPatch{"http://example.org/x.ingen"})[0].accepted, "unreadable location loaded");
}
}
int main() {
    try {
        const auto root = fs::temp_directory_path() / "peepo-storage-tests";
        fs::remove_all(root);
        bundles();
        store(root / "store");
        simulated(root / "simulated");
        foreign_bundles(root / "foreign");
        imports(root / "imports");
        sticks(root / "sticks");
        fs::remove_all(root);
        std::cout << "Bundles, revisions, interrupted saves, fallback, startup choice, foreign bundles, missing files, scenes, simulated save/load, file imports, sticks and deleting files passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
