// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "engine/engine.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>

// An Ingen graph bundle, as the engine writes it with patch:Copy to a
// directory ending in .ingen: main.ttl describes the graph, relative to the
// bundle, and each stateful block keeps its LV2 state in a subdirectory.
// See docs/17-patch-storage.md for the recorded format.
namespace peepo {
inline constexpr int bundle_schema_version = 1;
struct BundleBlock {
    // Relative to the graph: `m3`, the engine's name for module 3.
    std::string symbol, plugin, name;
    bool positioned = false, enabled = true;
    double x = 0, y = 0;
    // Control input values by port symbol.
    std::map<std::string, double> values;
    // Relative path of its saved plugin state, when it has one.
    std::string state;
    // The file given to its file parameter (peepo:file), when it has one.
    std::string file;
};
struct Bundle {
    std::string id, name;
    int schema = 0;
    bool modified = false;
    // The patch tempo; bundles saved before it existed have none.
    std::optional<double> tempo;
    // The control changes sent as it loads (peepo:midiOnLoad).
    std::vector<ControlChange> load_messages;
    std::vector<BundleBlock> blocks;
    // Tail and head, relative to the graph: `in_l`, `m3/out`.
    std::vector<std::pair<std::string, std::string>> arcs;
    // Control assignments by control identity, as stored (see encode_assignment).
    std::map<std::string, std::string> controls;
};
// Read main.ttl. Returns what was wrong, or an empty string.
std::string read_bundle(std::string_view main_ttl, Bundle&);
// The blocks a load must find in the engine, in the order they were saved.
std::vector<SavedBlock> saved_blocks(const Bundle&);
// The bundle as an editable graph, described by the catalogue. Blocks whose
// plugin the catalogue does not know become missing-module placeholders.
Graph bundle_graph(const Bundle&);
// What the simulator writes in place of the engine: main.ttl and
// manifest.ttl in the subset read_bundle reads, without plugin state.
std::string write_main_ttl(const Graph&);
std::string write_manifest_ttl();
}
