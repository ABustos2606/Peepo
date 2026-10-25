// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "engine/engine.hpp"
#include "engine/ingen/protocol.hpp"
#include <map>
#include <set>

namespace peepo::ingen {
struct Options {
    // unix:///path/to/socket (not on Windows) or tcp://host:port.
    std::string address = "unix:///tmp/ingen.sock";
    unsigned request_timeout_ms = 3000;
    // Wait between connection attempts after a failure or a lost engine.
    unsigned reconnect_ms = 1000;
    // An engine message never legitimately approaches this; beyond it the
    // stream is treated as broken and the connection is replaced.
    std::size_t max_message_bytes = std::size_t{4} << 20;
    std::size_t max_pending = 256;
};
// The protocol state machine, without a socket: bytes in, bytes out, engine
// events. It mirrors the engine's graph and never reports an edit as accepted
// before the engine has answered it. Not thread-safe; one thread drives it.
class Session {
public:
    explicit Session(Options = {});
    // Reported by whoever owns the connection.
    void opening();
    void opened();
    void closed(std::string reason, bool retrying);
    void received(std::string_view bytes);
    // Complete requests for the engine, in order.
    std::vector<std::string> take_outgoing();
    // Set when the stream can no longer be trusted and must be replaced.
    std::optional<std::string> take_fault();

    RequestId submit(Command);
    std::vector<EngineEvent> advance(unsigned milliseconds);
    EngineState state() const { return state_; }
    const Graph& snapshot() const { return graph_; }
    EngineTelemetry telemetry() const {
        const bool ready = state_ == EngineState::Ready;
        return {ready ? std::optional<double>{graph_.tempo} : std::nullopt, ready ? cpu_ : std::nullopt};
    }
    void watch_tuner(std::optional<InstanceId>);
    std::optional<double> tuner_frequency(InstanceId) const;
    void watch_euclidean(std::optional<InstanceId>);
    EuclideanSteps euclidean_steps(InstanceId) const;
    void watch_looper(std::optional<InstanceId>);
    LooperStatus looper_status(InstanceId) const;
    bool looper_command(InstanceId, int command, int target, bool press);
private:
    enum class Purpose { Edit, Sync, Endpoints, Refresh, Monitor };
    struct Local { std::string path, predicate; Value value; };
    using TransactionId = unsigned long long;
    struct Transaction {
        TransactionId id = 0;
        RequestId request = 0;
        Purpose purpose = Purpose::Edit;
        std::set<int> waiting;
        std::vector<int> sequences;
        // The earliest failing step decides the result; later ones only follow from it.
        int status = 0, failed = 0;
        // The Get whose answer replaces the mirrored graph.
        int replaces = 0;
        unsigned age = 0;
        // Values the engine does not echo to the client that set them.
        std::vector<Local> local;
        // Properties removed, by path and predicate, likewise not echoed.
        std::vector<std::pair<std::string, std::string>> dropped;
        std::optional<bool> dirty;
        std::string message;
        // A save or load answered: what to do next, under the same request.
        enum class Next { None, Saved, Loaded } next = Next::None;
        // Load: the bundle, and the blocks the engine should then have.
        std::string location, patch_id;
        std::vector<SavedBlock> expected;
        bool verify = false;
        // Save: the identity to put back if the engine refuses it.
        std::optional<std::string> restore_id;
        // Session save: keep the graph once accepted.
        bool snapshot = false;
    };
    Transaction& begin(RequestId, Purpose);
    void send(Transaction&, std::string (*writer)(int, std::string_view), std::string_view subject);
    void send(Transaction&, std::string text, int number);
    int sequence() { return next_sequence_++; }
    void handle(const Message&);
    void apply(const Message&);
    void merge(const std::string& path, const Properties&);
    void erase(const std::string& path);
    void learned(const std::string& port);
    void finish_ready();
    void finish(Transaction&);
    void fail_edits(const std::string& message);
    void reject(RequestId, std::string message);
    void emit(RequestId, bool accepted, std::string message, bool with_session = false);
    void ready();
    void ensure_endpoints();
    void refresh(std::string path, bool replace);
    void rebuild();
    std::string plan(const Command&, Transaction&);
    void give_file(Transaction&, const std::string& path, const ModuleDefinition&, const std::string& file);
    void set_tempo(Transaction&, double bpm);
    void load_messages(Transaction&, const std::vector<ControlChange>&);
    void name_patch(Transaction&, const std::string& name);
    void identify(Transaction&, const std::string& id);
    void mark(Transaction&, bool modified);
    void control_property(Transaction&, const std::string& control, const std::string& text);
    // A control's assignments as its property value; those whose block is gone are left out.
    std::string control_text(const std::vector<Assignment>&) const;
    // A block's bypass and a control's value, as SetBypass and SetParameter send them.
    void bypass_block(Transaction&, const Module&, const std::string& path, bool bypassed);
    void set_value(Transaction&, const std::string& path, const std::string& symbol, double value);
    // The scene properties, those that differ from the engine's; the slots
    // these scenes do not use are emptied. A module's block is its path's
    // symbol (`current`) or m<id>, as for a module the engine has yet to make.
    void write_scenes(Transaction&, const Scenes&, bool current = true);
    // Scene edits are made on a copy of the graph, then written.
    std::string edit_scenes(Transaction&, const std::function<std::string(Graph&)>&);
    void proceed(Transaction&);
    // What a load restored; `failed` when it restored nothing it should have.
    std::string verified(const Transaction&, bool& failed);
    std::string clear_engine_graph(Transaction&);
    std::string create_graph(const Graph&, Transaction&);
    std::string address(InstanceId, const std::string& symbol, Direction, bool current) const;

    Options options_;
    EngineState state_ = EngineState::Disconnected;
    Graph graph_;
    std::optional<Graph> saved_session_;
    // The unsaved-changes marker is a property of the engine's graph, so a
    // restarted interface sees it. The last value sent, until it is echoed.
    std::optional<bool> sent_modified_;
    // Likewise each scene property, so edits in flight build on one another.
    std::map<std::string, Value> sent_scenes_;
    std::optional<double> cpu_;
    std::optional<InstanceId> tuner_;
    std::string tuner_port_;
    std::optional<double> tuner_frequency_;
    unsigned tuner_age_ = 0;
    std::optional<InstanceId> euclidean_;
    std::array<std::string, euclidean_tracks> euclidean_ports_{};
    EuclideanSteps euclidean_steps_{};
    unsigned euclidean_age_ = 0;
    // MIDI learn: the port waiting for a MIDI message, and one whose wait was
    // ended here. Ingen cannot end a wait, so a binding that still arrives on
    // that port is removed again.
    std::string learning_, cancelled_learn_;
    std::optional<InstanceId> looper_;
    std::array<std::string, looper_outputs> looper_ports_{};
    LooperStatus looper_status_{};

    // The engine's objects under ingen:/main, by path.
    std::map<std::string, Properties> objects_;
    std::vector<std::pair<std::string, std::string>> arcs_;
    std::set<std::string> plugins_;
    // The graph needs rebuilding; the interface has not seen the latest one.
    bool mirror_changed_ = false, unreported_ = false;
    std::map<std::string, InstanceId> ids_;
    std::map<InstanceId, std::string> paths_;
    InstanceId next_instance_ = 3;
    std::set<std::string> expected_copies_;

    // The engine writes relative to its base <ingen:/> (older serd does).
    turtle::Reader reader_{"ingen:/"};
    std::string buffer_;
    int depth_ = 0;
    std::vector<std::string> outgoing_;
    std::optional<std::string> fault_;
    int next_sequence_ = 1;
    RequestId next_request_ = 1;
    TransactionId next_transaction_ = 1;
    std::map<TransactionId, Transaction> transactions_;
    std::map<int, TransactionId> sequences_;
    std::vector<EngineEvent> events_;
};
}
