// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// A conventional effect on the Pi, through the same adapter and store as the UI:
//   test-graph [clouds]   IN -> MDA Overdrive (or Clouds) -> OUT, for measuring
//   set SYMBOL VALUE      change one control of the effect in the running graph
//   restore ID            load the newest revision of a saved patch
//   combine               put Overdrive before Clouds in the running patch,
//                         save it as a new patch and reload it from the card
// Never removes or overwrites a saved patch.
#include "catalogue/catalogue.hpp"
#include "engine/ingen/ingen_engine.hpp"
#include "storage/patch_store.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <tuple>
using namespace peepo;
using Clock = std::chrono::steady_clock;
namespace {
const std::string overdrive = "http://drobilla.net/plugins/mda/Overdrive";
const std::string clouds = "http://polyeffects.com/lv2/polyclouds#granular";
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
    std::cout << "PASS " << message << std::endl;
}
struct Client {
    IngenEngine engine{ingen::Options{"unix:///run/peepo/ingen.sock"}};
    Client() {
        engine.connect();
        const auto until = Clock::now() + std::chrono::seconds(10);
        while (engine.state() != EngineState::Ready && Clock::now() < until) tick();
        require(engine.state() == EngineState::Ready, "connect to the running Pi engine");
    }
    std::vector<EngineEvent> tick() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return engine.advance(1);
    }
    void command(Command command, const std::string& description) {
        const auto id = engine.submit(std::move(command));
        const auto start = Clock::now();
        while (Clock::now() - start < std::chrono::seconds(10))
            for (const auto& event : tick()) if (event.request == id) {
                require(event.accepted, description + (event.accepted ? "" : ": " + event.message));
                return;
            }
        throw std::runtime_error("timeout: " + description);
    }
};
const Module* by_uri(const Graph& g, const std::string& uri) {
    const auto it = std::find_if(g.modules.begin(), g.modules.end(), [&](const Module& m) { return m.uri == uri; });
    return it == g.modules.end() ? nullptr : &*it;
}
// The graph without transient instance numbers, controls rounded to 1e-6.
std::string canonical(Graph graph) {
    std::sort(graph.modules.begin(), graph.modules.end(), [](const auto& a, const auto& b) { return std::tie(a.fixed, a.uri, a.x, a.y) > std::tie(b.fixed, b.uri, b.x, b.y); });
    std::map<InstanceId, InstanceId> ids;
    InstanceId next = 1;
    for (auto& module : graph.modules) { ids[module.id] = module.fixed ? module.id : 100 + next++; }
    for (auto& module : graph.modules) {
        module.id = ids.at(module.id);
        for (auto& p : module.parameters) p.value = std::round(p.value * 1e6) / 1e6;
        std::sort(module.parameters.begin(), module.parameters.end(), [](const auto& a, const auto& b) { return a.symbol < b.symbol; });
    }
    for (auto& c : graph.connections) { c.from = ids.at(c.from); c.to = ids.at(c.to); }
    for (auto& a : graph.assignments) a.module = ids.at(a.module);
    std::sort(graph.modules.begin(), graph.modules.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(graph.connections.begin(), graph.connections.end(), [](const auto& a, const auto& b) { return std::tie(a.from, a.to, a.output, a.input) < std::tie(b.from, b.to, b.output, b.input); });
    return write_main_ttl(graph);
}
void show(const Graph& g) {
    std::cout << "GRAPH " << g.name << " id=" << g.patch_id << " dirty=" << g.dirty << '\n';
    for (const auto& m : g.modules) {
        std::cout << "MODULE " << m.id << ' ' << m.name << " <" << m.uri << ">" << (m.bypassed ? " bypassed" : "") << '\n';
        if (!m.fixed) for (const auto& p : m.parameters) std::cout << "  " << p.symbol << '=' << p.value << '\n';
    }
    for (const auto& c : g.connections) std::cout << "CABLE " << c.from << '.' << c.output << " -> " << c.to << '.' << c.input << '\n';
}
}

int main(int argc, char** argv) try {
    const std::string mode = argc > 1 ? argv[1] : "show";
    std::vector<ModuleDefinition> definitions;
    const auto issue = load_catalogue("/opt/peepo/share/peepo/catalogue.json", "/opt/peepo/share/peepo/catalogue-overlay.json", definitions);
    require(issue.empty(), "load the installed catalogue and overlay" + (issue.empty() ? "" : ": " + issue));
    replace_catalogue(std::move(definitions));
    Client client;
    const auto current = client.engine.snapshot();
    if (mode == "show") { show(current); return 0; }

    if (mode == "test-graph") {
        const bool granular = argc > 2 && std::string(argv[2]) == "clouds";
        const auto& uri = granular ? clouds : overdrive;
        const std::string name = granular ? "Mist" : "Overdrive";
        auto graph = empty_graph(name + " gain test");
        graph.connections.clear();
        client.command(LoadGraph{graph}, "load an empty test graph");
        client.command(AddModule{uri, 312, 132}, "add " + name);
        const auto* effect = by_uri(client.engine.snapshot(), uri);
        require(effect != nullptr, "the engine lists the added " + name);
        const auto id = effect->id;
        const std::string in_l = granular ? "l_in" : "left_in", in_r = granular ? "r_in" : "right_in";
        const std::string out_l = granular ? "l_out" : "left_out", out_r = granular ? "r_out" : "right_out";
        client.command(Connect{{1, id, "out", in_l}}, "IN left -> " + name + " left");
        client.command(Connect{{1, id, "out_r", in_r}}, "IN right -> " + name + " right");
        client.command(Connect{{id, 2, out_l, "in"}}, name + " left -> OUT left");
        client.command(Connect{{id, 2, out_r, "in_r"}}, name + " right -> OUT right");
        show(client.engine.snapshot());
        return 0;
    }
    if (mode == "set") {
        if (argc != 4) throw std::runtime_error("usage: set SYMBOL VALUE");
        const auto* effect = by_uri(current, overdrive) ? by_uri(current, overdrive) : by_uri(current, clouds);
        require(effect != nullptr, "the running graph has Overdrive or Clouds");
        client.command(SetParameter{effect->id, argv[2], std::stod(argv[3])}, "set " + effect->name + " " + argv[2] + " to " + argv[3]);
        return 0;
    }

    PatchStore store("/var/lib/peepo/patches");
    require(store.open().empty(), "open the Pi's patch store");
    if (mode == "restore") {
        if (argc != 3) throw std::runtime_error("usage: restore PATCH_ID");
        const auto stored = store.find(argv[2]);
        require(stored && stored->issue.empty() && stored->missing_files.empty(), "find a complete saved patch " + std::string(argv[2]));
        client.command(LoadPatch{stored->location, saved_blocks(stored->bundle), stored->id}, "reload " + stored->name + " revision " + std::to_string(stored->revision));
        show(client.engine.snapshot());
        return 0;
    }
    if (mode != "combine") throw std::runtime_error("usage: peepo_effect_live [show|test-graph|set SYMBOL VALUE|restore ID|combine]");

    // IN -> Overdrive -> Clouds -> OUT, keeping Clouds and its controls as they are.
    require(!current.dirty, "the running patch has no unsaved edits");
    const auto* granular = by_uri(current, clouds);
    require(granular != nullptr && !by_uri(current, overdrive), "the running patch has Clouds and no Overdrive yet");
    const auto clouds_id = granular->id;
    std::vector<Connection> into_clouds;
    for (const auto& c : current.connections) if (c.from == 1 && c.to == clouds_id) into_clouds.push_back(c);
    require(into_clouds.size() == 2, "Clouds is fed by both IN channels");
    client.command(AddModule{overdrive, granular->x, granular->y + 120}, "add MDA Overdrive");
    const auto* od = by_uri(client.engine.snapshot(), overdrive);
    require(od != nullptr, "the engine lists the added Overdrive");
    const auto od_id = od->id;
    for (const auto& c : into_clouds) client.command(Disconnect{c}, "disconnect IN." + c.output + " from Clouds." + c.input);
    client.command(Connect{{1, od_id, "out", "left_in"}}, "IN left -> Overdrive left");
    client.command(Connect{{1, od_id, "out_r", "right_in"}}, "IN right -> Overdrive right");
    client.command(Connect{{od_id, clouds_id, "left_out", "l_in"}}, "Overdrive left -> Clouds left");
    client.command(Connect{{od_id, clouds_id, "right_out", "r_in"}}, "Overdrive right -> Clouds right");
    // A light drive, with the level chosen from the measured gain (see docs).
    const double drive = argc > 2 ? std::stod(argv[2]) : 0.3, muffle = argc > 3 ? std::stod(argv[3]) : 0.2, output = argc > 4 ? std::stod(argv[4]) : 0.5;
    client.command(SetParameter{od_id, "drive", drive}, "set Overdrive drive");
    client.command(SetParameter{od_id, "muffle", muffle}, "set Overdrive muffle");
    client.command(SetParameter{od_id, "output", output}, "set Overdrive output");
    client.command(RenamePatch{"Overdrive into Clouds"}, "name the combined patch");

    const auto id = store.new_id();
    std::string staging_issue;
    const auto staging = store.stage(id, staging_issue);
    require(staging.has_value(), "stage the combined patch as a new patch: " + staging_issue);
    client.command(SaveGraph{SaveScope::Persistent, true, staging->location, id}, "save the combined patch with plugin state");
    require(store.publish(*staging).empty(), "publish revision 1 atomically");
    const auto saved = client.engine.snapshot();
    require(saved.patch_id == id && !saved.dirty, "the engine plays the saved identity with no unsaved edits");
    const auto stored = store.find(id);
    require(stored && stored->revision == 1 && stored->issue.empty() && stored->missing_files.empty(), "the patch store lists a complete readable bundle");
    auto disk = bundle_graph(stored->bundle);
    disk.dirty = false;
    require(canonical(disk) == canonical(saved), "the bundle on the card matches the running graph");

    // Change the drive, then reload from the card: the saved value must come back.
    client.command(SetParameter{od_id, "drive", drive < 0.5 ? drive + 0.2 : drive - 0.2}, "change the drive temporarily");
    require(client.engine.snapshot().dirty, "the running patch now differs from the saved one");
    client.command(LoadPatch{stored->location, saved_blocks(stored->bundle), id}, "reload the combined patch from the card");
    const auto restored = client.engine.snapshot();
    require(canonical(restored) == canonical(saved), "name, identity, both modules, all cables, every control and layout restored");
    show(restored);
    std::cout << "SAVED " << stored->name << " id=" << id << " path=" << stored->path.generic_string() << '\n';
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << std::endl; return 1; }
