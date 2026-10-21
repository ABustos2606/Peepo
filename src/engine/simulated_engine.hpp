// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "engine/engine.hpp"
#include <deque>

namespace peepo {
class SimulatedEngine final : public Engine {
public:
    RequestId submit(Command) override;
    void connect() override;
    void disconnect() override { set_state(EngineState::Disconnected); }
    std::vector<EngineEvent> advance(unsigned milliseconds) override;
    EngineState state() const override { return state_; }
    const Graph& snapshot() const override { return graph_; }
    // Persistent saves write main.ttl only: simulated modules have no plugin state.
    EngineCapabilities capabilities() const override { return {true, true, true, false, false}; }
    EngineTelemetry telemetry() const override {
        // The patch tempo; the CPU figure is a fixed display fixture, not a measurement.
        return {graph_.tempo, state_ == EngineState::Ready ? std::optional<double>{12.0} : std::nullopt};
    }
    bool simulate_state(EngineState state) override { set_state(state); return true; }
    bool simulate_rejection() override { reject_next(); return true; }
    bool simulate_missing_module() override;
    void set_state(EngineState);
    void reject_next() { reject_next_ = true; }
private:
    struct Pending { RequestId id; Command command; std::uint64_t due; };
    std::string apply(const Command&);
    Graph graph_ = demo_graph();
    EngineState state_ = EngineState::Ready;
    std::uint64_t clock_ = 0;
    std::optional<std::uint64_t> connect_due_;
    std::optional<Graph> saved_session_;
    std::string note_;
    RequestId next_request_ = 1;
    InstanceId next_instance_ = 6;
    bool reject_next_ = false;
    std::deque<Pending> pending_;
    // A control waiting for MIDI learn. Nothing plays MIDI here, so it learns
    // the next unused controller on channel 1 a moment later.
    struct Learning { InstanceId id; std::string symbol; std::uint64_t due; };
    std::optional<Learning> learning_;
    std::vector<EngineEvent> events_;
};
}
