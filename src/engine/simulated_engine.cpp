// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "engine/simulated_engine.hpp"
#include "storage/bundle.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <limits>

namespace peepo {
namespace {
// file:///C:/x or file:///home/x as a local path; empty for anything else.
std::filesystem::path local_path(const std::string& location) {
    if (!location.starts_with("file:///") || !location.ends_with(".ingen") || location.find('%') != std::string::npos) return {};
    auto path = location.substr(7);
    if (path.size() > 3 && path[2] == ':') path.erase(0, 1);
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}
}
RequestId SimulatedEngine::submit(Command c) {
    const auto id = next_request_++;
    if (state_ != EngineState::Ready) {
        events_.push_back({id, false, "Engine is unavailable", state_, graph_}); return id;
    }
    pending_.push_back({id, std::move(c), clock_ + 100});
    return id;
}
void SimulatedEngine::set_state(EngineState state) {
    connect_due_.reset();
    state_ = state;
    // A connection loss cancels requests; reconnect never applies stale edits.
    if (state != EngineState::Ready) {
        for (const auto& p : pending_) events_.push_back({p.id, false, "Engine disconnected", state, {}});
        pending_.clear();
    }
    events_.push_back({0, true, "", state, graph_});
}
void SimulatedEngine::connect() {
    set_state(EngineState::Connecting); connect_due_ = clock_ + 100;
}
bool SimulatedEngine::simulate_missing_module() {
    if (state_ != EngineState::Ready || !pending_.empty()) return false;
    graph_ = missing_module_graph();
    events_.push_back({0, true, "Missing module fixture loaded. Orange cables are inactive.", state_, graph_});
    return true;
}
std::vector<EngineEvent> SimulatedEngine::advance(unsigned milliseconds) {
    clock_ += milliseconds;
    if (connect_due_ && clock_ >= *connect_due_) set_state(EngineState::Ready);
    if (learning_ && clock_ >= learning_->due && state_ == EngineState::Ready) {
        int number = 20;
        for (const auto& m : graph_.modules) for (const auto& p : m.parameters) if (p.midi) number = std::max(number, p.midi->number + 1);
        if (auto* m = find_module(graph_, learning_->id))
            for (auto& p : m->parameters) if (p.symbol == learning_->symbol) { p.midi = MidiBinding{MidiBinding::Kind::Controller, 1, std::min(number, 127)}; graph_.dirty = true; }
        learning_.reset();
        events_.push_back({0, true, {}, state_, graph_});
    }
    while (!pending_.empty() && pending_.front().due <= clock_) {
        auto p = std::move(pending_.front()); pending_.pop_front();
        std::string error;
        if (state_ != EngineState::Ready) error = "Engine is unavailable";
        else if (reject_next_) { error = "Simulated command rejection"; reject_next_ = false; }
        else error = apply(p.command);
        EngineEvent event{p.id, error.empty(), error, state_, graph_};
        if (error.empty() && std::holds_alternative<SaveGraph>(p.command) && std::get<SaveGraph>(p.command).scope == SaveScope::Session) {
            event.saved_session = saved_session_;
            event.message = "Session snapshot saved in memory. Closing the app clears it.";
        } else if (error.empty()) event.message = std::move(note_);
        note_.clear();
        events_.push_back(std::move(event));
    }
    auto result = std::move(events_); events_.clear(); return result;
}
std::string SimulatedEngine::apply(const Command& command) {
    bool following = false;
    auto error = std::visit([&](const auto& c) -> std::string {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, AddModule>) {
            const auto* d = definition(c.uri);
            if (!d || d->category == "Endpoint" || d->availability != Availability::Available) return "Module is unavailable or untested";
            if (next_instance_ == std::numeric_limits<InstanceId>::max()) return "Instance ID space exhausted";
            graph_.modules.push_back({next_instance_++, d->uri, numbered_name(graph_, d->name), std::clamp(c.x, module_min_x, module_max_x),
                std::clamp(c.y, module_min_y, module_max_y), false, false, d->parameters, d->ports});
            for (auto& p : graph_.modules.back().parameters) if (p.symbol == d->tempo_symbol) p.value = tempo_value(p, graph_.tempo);
            scenes_module_added(graph_, graph_.modules.back().id);
        } else if constexpr (std::is_same_v<T, Connect>) {
            auto issue = validate_connection(graph_, c.connection);
            if (!issue.empty()) return issue;
            graph_.connections.push_back(c.connection);
        } else if constexpr (std::is_same_v<T, Disconnect>) {
            const auto count = std::erase(graph_.connections, c.connection);
            if (!count) return "Cable is missing";
        } else if constexpr (std::is_same_v<T, LoadGraph>) {
            if (auto issue = validate_graph(c.graph); !issue.empty()) return issue;
            graph_ = c.graph;
            for (const auto& m : graph_.modules) next_instance_ = std::max(next_instance_, m.id + 1);
        } else if constexpr (std::is_same_v<T, SetTempo>) {
            if (!std::isfinite(c.bpm) || c.bpm < minimum_tempo || c.bpm > maximum_tempo) return "Tempo is outside 30 to 300 BPM";
            graph_.tempo = c.bpm;
            if (c.follow) following = true;
            for (auto& m : graph_.modules)
                if (const auto* d = definition(m.uri); d && !d->tempo_symbol.empty() && module_available(m))
                    for (auto& p : m.parameters) if (p.symbol == d->tempo_symbol) p.value = tempo_value(p, c.bpm);
        } else if constexpr (std::is_same_v<T, SetLoadMessages>) {
            if (decode_control_changes(encode_control_changes(c.messages)) != c.messages) return "Invalid MIDI messages";
            graph_.load_messages = c.messages;
        } else if constexpr (std::is_same_v<T, NewPatch>) {
            if (auto issue = validate_patch_name(c.name); !issue.empty()) return issue;
            graph_ = empty_graph(c.name);
        } else if constexpr (std::is_same_v<T, RenamePatch>) {
            if (auto issue = validate_patch_name(c.name); !issue.empty()) return issue;
            graph_.name = c.name;
        } else if constexpr (std::is_same_v<T, SaveGraph>) {
            if (c.include_plugin_state) return "Unsupported capability: plugin state capture";
            if (c.scope == SaveScope::Persistent) {
                // Written in the engine's bundle layout, so the patch store
                // treats both alike. Simulated modules have no plugin state.
                const auto path = local_path(c.location);
                if (path.empty() || !valid_patch_id(c.patch_id)) return "Invalid save location";
                auto saved = graph_;
                saved.patch_id = c.patch_id; saved.dirty = false;
                std::error_code failure;
                std::filesystem::create_directories(path, failure);
                std::ofstream main(path / "main.ttl", std::ios::binary), manifest(path / "manifest.ttl", std::ios::binary);
                main << write_main_ttl(saved); manifest << write_manifest_ttl();
                if (failure || !main || !manifest) return "Could not write the patch";
                graph_ = std::move(saved);
                note_ = "Saved " + graph_.name + ".";
                return {};
            }
            saved_session_ = graph_; saved_session_->dirty = false; graph_.dirty = false;
        } else if constexpr (std::is_same_v<T, LoadSession>) {
            if (!saved_session_) return "No session snapshot saved";
            graph_ = *saved_session_;
        } else if constexpr (std::is_same_v<T, Assign>) {
            auto a = c.assignment;
            const auto* m = find_module(graph_, a.module);
            if (!valid_control_id(a.control) || !m) return "Invalid control assignment";
            a.plugin = m->uri;
            if (!assignment_target(graph_, a)) return "Control target is unavailable";
            if (const auto* p = assignment_parameter(graph_, a); p && (a.minimum < p->minimum || a.maximum > p->maximum || a.minimum > a.maximum))
                return "Invalid assignment parameter range";
            auto held = assignments_after(graph_, a, c.add);
            std::erase_if(graph_.assignments, [&](const Assignment& old) { return old.control == a.control; });
            for (auto& entry : held) graph_.assignments.push_back(std::move(entry));
        } else if constexpr (std::is_same_v<T, Unassign>) {
            if (!std::erase_if(graph_.assignments, [&](const Assignment& old) { return old.control == c.control && (!c.module || old.module == c.module); }))
                return "Control is not assigned";
        } else if constexpr (std::is_same_v<T, SelectScene>) {
            if (c.scene >= graph_.scenes.scenes.size()) return "No such scene.";
            // All at once, as the engine's atomic bundle; playing, so not an edit.
            for (const auto& change : scene_changes(graph_, c.scene)) {
                auto* m = find_module(graph_, change.module);
                if (change.symbol.empty()) m->bypassed = change.value != 0;
                else for (auto& p : m->parameters) if (p.symbol == change.symbol) p.value = change.value;
            }
            graph_.scenes.current = c.scene;
            following = true;
        } else if constexpr (std::is_same_v<T, AddScene>) {
            if (auto issue = add_scene(graph_); !issue.empty()) return issue;
        } else if constexpr (std::is_same_v<T, RenameScene>) {
            if (auto issue = rename_scene(graph_, c.scene, c.name); !issue.empty()) return issue;
        } else if constexpr (std::is_same_v<T, CopyScene>) {
            if (auto issue = copy_scene(graph_, c.from, c.to); !issue.empty()) return issue;
        } else if constexpr (std::is_same_v<T, MoveScene>) {
            if (auto issue = move_scene(graph_, c.from, c.to); !issue.empty()) return issue;
        } else if constexpr (std::is_same_v<T, DeleteScene>) {
            if (auto issue = delete_scene(graph_, c.scene); !issue.empty()) return issue;
        } else if constexpr (std::is_same_v<T, MarkSceneParameter>) {
            if (auto issue = c.marked ? mark_scene_parameter(graph_, c.id, c.symbol) : unmark_scene_parameter(graph_, c.id, c.symbol); !issue.empty()) return issue;
        } else if constexpr (std::is_same_v<T, SetSceneMode>) {
            if (auto issue = set_scene_mode(graph_, c.scene_mode); !issue.empty()) return issue;
        } else if constexpr (std::is_same_v<T, LoadPatch>) {
            const auto path = local_path(c.location);
            std::ifstream in(path / "main.ttl", std::ios::binary);
            if (path.empty() || !in) return "Saved patch is not readable";
            std::ostringstream text; text << in.rdbuf();
            Bundle bundle;
            if (auto issue = read_bundle(text.str(), bundle); !issue.empty()) return issue;
            std::vector<std::string> absent;
            for (const auto& block : saved_blocks(bundle)) {
                const auto* known = definition(block.plugin);
                if (!known || known->availability != Availability::Available) absent.push_back(block.name);
            }
            // As Ingen does: one missing plugin and none of the bundle loads.
            if (!absent.empty()) {
                std::string names;
                for (const auto& name : absent) names += (names.empty() ? "" : ", ") + name;
                return "Not installed in the engine: " + names;
            }
            auto loaded = bundle_graph(bundle);
            loaded.dirty = false;
            if (!c.patch_id.empty()) loaded.patch_id = c.patch_id;
            if (auto issue = validate_graph(loaded); !issue.empty()) return issue;
            graph_ = std::move(loaded);
            for (const auto& m : graph_.modules) next_instance_ = std::max(next_instance_, m.id + 1);
            note_ = "Loaded " + graph_.name + ".";
        } else {
            auto* m = find_module(graph_, c.id);
            if (!m) return "Module is missing";
            if constexpr (std::is_same_v<T, RemoveModule>) {
                if (m->fixed) return "Endpoints cannot be deleted";
                std::erase_if(graph_.connections, [&](const Connection& edge) { return edge.from == c.id || edge.to == c.id; });
                std::erase_if(graph_.assignments, [&](const Assignment& a) { return a.module == c.id; });
                scenes_module_removed(graph_, *m);
                std::erase_if(graph_.modules, [&](const Module& item) { return item.id == c.id; });
            } else if constexpr (std::is_same_v<T, MoveModule>) {
                if (m->fixed) return "Endpoints are fixed";
                m->x = std::clamp(c.x, module_min_x, module_max_x);
                m->y = std::clamp(c.y, module_min_y, module_max_y);
            } else if constexpr (std::is_same_v<T, DuplicateModule>) {
                if (m->fixed) return "Endpoints cannot be duplicated";
                if (!module_available(*m)) return "Unavailable module cannot be duplicated";
                if (next_instance_ == std::numeric_limits<InstanceId>::max()) return "Instance ID space exhausted";
                auto copy = *m;
                copy.id = next_instance_++;
                const auto* d = definition(m->uri);
                copy.name = numbered_name(graph_, d ? d->name : m->name);
                copy.x = std::clamp(c.x, module_min_x, module_max_x);
                copy.y = std::clamp(c.y, module_min_y, module_max_y);
                const auto original = m->id, made = copy.id;
                graph_.modules.push_back(std::move(copy));
                scenes_module_duplicated(graph_, original, made);
            } else if constexpr (std::is_same_v<T, SetBypass>) {
                if (m->fixed) return "Endpoints cannot be bypassed";
                if (!module_available(*m)) return "Module is unavailable";
                m->bypassed = c.value;
                if (c.scene) record_scene_bypass(graph_, c.id, c.value);
            } else if constexpr (std::is_same_v<T, SetFile>) {
                // No audio here: the file is only recorded, as the engine records it.
                if (!module_available(*m)) return "Module is unavailable";
                const auto* d = definition(m->uri);
                if (!d || d->file.property.empty()) return "This module reads no file";
                if (c.path.empty() || c.path.size() > 1024) return "Invalid file path";
                m->file = c.path;
            } else if constexpr (std::is_same_v<T, LearnMidi>) {
                if (!module_available(*m)) return "Module is unavailable";
                auto it = std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == c.symbol; });
                if (it == m->parameters.end() || c.symbol == general_output_symbol) return "This control cannot be set by MIDI";
                it->midi.reset();
                if (c.forget) { if (learning_ && learning_->id == c.id && learning_->symbol == c.symbol) learning_.reset(); }
                else learning_ = Learning{c.id, c.symbol, clock_ + 1500};
            } else if constexpr (std::is_same_v<T, SetParameter>) {
                if (!module_available(*m)) return "Module is unavailable";
                auto it = std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == c.symbol; });
                if (it == m->parameters.end()) return "Parameter is missing";
                if (!std::isfinite(c.value) || c.value < it->minimum || c.value > it->maximum) return "Value is outside the parameter range";
                if (it->kind != ParameterKind::Continuous && c.value != std::round(c.value)) return "Parameter requires a discrete value";
                it->value = c.value;
                if (c.scene) record_scene_parameter(graph_, c.id, c.symbol, c.value);
            }
        }
        return {};
    }, command);
    if (error.empty() && !std::holds_alternative<LoadGraph>(command) && !std::holds_alternative<SaveGraph>(command) &&
        !std::holds_alternative<LoadSession>(command) && !std::holds_alternative<LoadPatch>(command) && !following) graph_.dirty = true;
    return error;
}
}
