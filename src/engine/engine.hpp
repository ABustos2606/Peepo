// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "core/euclidean.hpp"
#include "core/looper.hpp"
#include "core/model.hpp"
#include <optional>
#include <variant>

namespace peepo {
enum class EngineState { Ready, Disconnected, Error, Connecting };
struct EngineCapabilities {
    bool simulation = false;
    bool session_save = false;
    bool persistent_save = false;
    bool plugin_state = false;
    bool audio_processing = false;
};
struct EngineTelemetry {
    // Global tempo is independent of the displayed module or active graph.
    // Missing readings remain unknown, rather than displaying zero or stale load.
    std::optional<double> tempo_bpm;
    std::optional<double> cpu_percent;
};
using RequestId = std::uint64_t;
struct AddModule { std::string uri; int x, y; };
struct RemoveModule { InstanceId id; };
struct MoveModule { InstanceId id; int x, y; };
struct DuplicateModule { InstanceId id; int x, y; };
struct SetParameter { InstanceId id; std::string symbol; double value; };
struct SetBypass { InstanceId id; bool value; };
// The patch tempo: stored with the patch, and given to every module that follows it.
struct SetTempo { double bpm; };
// Give a module's file parameter a file, by its path on the engine's machine.
struct SetFile { InstanceId id; std::string path; };
struct Connect { Connection connection; };
struct Disconnect { Connection connection; };
struct LoadGraph { Graph graph; };
struct NewPatch { std::string name; };
struct RenamePatch { std::string name; };
enum class SaveScope { Session, Persistent };
// Persistent: write the whole graph, plugin state included, as a bundle at
// `location` (a file:// URI as the engine sees it, ending in .ingen) and give
// the graph the stored identity `patch_id`. The bundle is complete when the
// command is accepted; publishing it is the patch store's business.
struct SaveGraph {
    SaveScope scope = SaveScope::Session;
    bool include_plugin_state = false;
    std::string location, patch_id;
};
struct LoadSession {};
// One block of a saved bundle, as the patch store read it.
struct SavedBlock { std::string symbol, plugin, name; };
// Replace the graph with a saved bundle, plugin state included. A bundle that
// needs a plugin the engine lacks is refused before anything changes: Ingen
// would load none of it. (The interface can still offer the rest, rebuilt from
// the bundle's description with LoadGraph, without plugin state.)
struct LoadPatch {
    std::string location;
    std::vector<SavedBlock> blocks;
    // The stored identity, given to the loaded graph even when the bundle
    // lacks it (a bundle stripped of its project properties).
    std::string patch_id;
    // The bundle's tempo, set before the engine reads it so the tempo goes
    // straight from the old patch's to the new one's (MIDI clock followers
    // hear no other); empty for a bundle without one, which plays the default.
    std::optional<double> tempo;
};
// Pin a physical control, replacing whatever it had in this patch; saved with
// it. With `add`, a bypass joins the modules the control already switches.
struct Assign { Assignment assignment; bool add = false; };
// Clear a control; with a module, only that module's bypass on it.
struct Unassign { std::string control; InstanceId module = 0; };
using Command = std::variant<AddModule, RemoveModule, MoveModule, DuplicateModule, SetParameter, SetBypass, SetFile, SetTempo, Connect, Disconnect,
                             LoadGraph, NewPatch, RenamePatch, SaveGraph, LoadSession, LoadPatch, Assign, Unassign>;
struct EngineEvent {
    RequestId request = 0;
    bool accepted = true;
    std::string message;
    EngineState state = EngineState::Ready;
    std::optional<Graph> snapshot;
    std::optional<Graph> saved_session;
};
class Engine {
public:
    virtual ~Engine() = default;
    virtual RequestId submit(Command) = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual std::vector<EngineEvent> advance(unsigned milliseconds) = 0;
    virtual EngineState state() const = 0;
    virtual const Graph& snapshot() const = 0;
    virtual EngineCapabilities capabilities() const = 0;
    virtual EngineTelemetry telemetry() const { return {}; }
    // Live tuner and sequencer outputs are telemetry, never editable/saved parameters.
    virtual void watch_tuner(std::optional<InstanceId>) {}
    virtual std::optional<double> tuner_frequency(InstanceId) const { return {}; }
    virtual void watch_euclidean(std::optional<InstanceId>) {}
    virtual EuclideanSteps euclidean_steps(InstanceId) const { return {}; }
    virtual void watch_looper(std::optional<InstanceId>) {}
    virtual LooperStatus looper_status(InstanceId) const { return {}; }
    virtual bool looper_command(InstanceId, int, int, bool) { return false; }
    // Optional development controls; real adapters keep these unsupported.
    virtual bool simulate_state(EngineState) { return false; }
    virtual bool simulate_rejection() { return false; }
    virtual bool simulate_missing_module() { return false; }
};
}
