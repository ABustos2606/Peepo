// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "engine/ingen/session.hpp"
#include "engine/ingen/transport.hpp"

namespace peepo {
// The engine contract over a running Ingen. Audio belongs to the engine
// process: connecting, disconnecting or destroying this object never starts,
// stops or restarts it, and a lost connection is retried, not replaced.
class IngenEngine final : public Engine {
public:
    // Without a transport, a socket to `options.address` is used.
    explicit IngenEngine(ingen::Options = {}, std::unique_ptr<ingen::Transport> = nullptr);
    ~IngenEngine() override;
    RequestId submit(Command) override;
    void connect() override;
    void disconnect() override;
    std::vector<EngineEvent> advance(unsigned milliseconds) override;
    EngineState state() const override { return session_.state(); }
    const Graph& snapshot() const override { return session_.snapshot(); }
    // The session snapshot is held by the interface: routing, values and layout.
    // A persistent save is the engine's own bundle, plugin state included.
    EngineCapabilities capabilities() const override { return {false, true, true, true, true}; }
    // No tempo yet. CPU is Ingen's mean run load: the share of the audio
    // period spent processing, averaged since the engine started.
    EngineTelemetry telemetry() const override { return session_.telemetry(); }
    void watch_tuner(std::optional<InstanceId> id) override { session_.watch_tuner(id); flush(); }
    std::optional<double> tuner_frequency(InstanceId id) const override { return session_.tuner_frequency(id); }
    void watch_euclidean(std::optional<InstanceId> id) override { session_.watch_euclidean(id); flush(); }
    EuclideanSteps euclidean_steps(InstanceId id) const override { return session_.euclidean_steps(id); }
    void watch_looper(std::optional<InstanceId> id) override { session_.watch_looper(id); flush(); }
    LooperStatus looper_status(InstanceId id) const override { return session_.looper_status(id); }
    bool looper_command(InstanceId id, int command, int target, bool press) override {
        const bool sent = session_.looper_command(id, command, target, press); flush(); return sent;
    }
private:
    void attempt();
    void lost(std::string reason);
    void flush();
    ingen::Options options_;
    ingen::Session session_;
    std::unique_ptr<ingen::Transport> transport_;
    // wanted: the interface asked for a connection. open: one is established.
    bool wanted_ = false, open_ = false, connecting_ = false, reported_ = false;
    unsigned retry_in_ = 0;
};
}
