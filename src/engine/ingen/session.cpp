// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "engine/ingen/session.hpp"
#include "core/tuning.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <type_traits>

namespace peepo::ingen {
namespace {
const std::string main_path = "ingen:/main";
// The graph's own audio and MIDI ports stand for the fixed IN and OUT modules.
// Seen from inside the graph an engine input is a source, so it is one of IN's
// outputs. The graph's first two ports, control and notify, are Ingen's own.
struct Endpoint { const char* symbol; InstanceId module; const char* port; bool midi = false; };
constexpr Endpoint endpoints[] = {{"in_l", 1, "out"}, {"in_r", 1, "out_r"}, {"out_l", 2, "in"}, {"out_r", 2, "in_r"},
                                  {"midi_in", 1, "midi", true}, {"midi_out", 2, "midi", true}};
std::string iri(std::string_view space, std::string_view local) { return std::string(space) + std::string(local); }
const std::string type = std::string(uri::rdf_type);
const std::string block_type = iri(uri::ingen, "Block"), arc_type = iri(uri::ingen, "Arc");
const std::string value_key = iri(uri::ingen, "value"), enabled_key = iri(uri::ingen, "enabled");
const std::string x_key = iri(uri::ingen, "canvasX"), y_key = iri(uri::ingen, "canvasY");
const std::string name_key = iri(uri::peepo, "displayName"), patch_name_key = iri(uri::peepo, "patchName");
const std::string patch_id_key = iri(uri::peepo, "patchId"), modified_key = iri(uri::peepo, "modified");
const std::string tempo_key = iri(uri::peepo, "tempo");
// The control changes sent as the patch loads: "1:7:100 3:10:64".
const std::string load_messages_key = iri(uri::peepo, "midiOnLoad");
// The general Output is the block's own property, in dB, not a plugin port.
const std::string output_level_key = iri(uri::peepo, "outputLevel");
// The file last given to a block's file parameter. The plugin keeps the file
// in its own state; this names it for every client and survives a restart.
const std::string file_key = iri(uri::peepo, "file");
// MIDI learn's result on a control port: [ a midi:Controller ; midi:controllerNumber 519 ].
const std::string binding_key = iri(uri::midi, "binding");
// One graph property per pinned control: peepo:control-enc1 "parameter|m3|...".
const std::string control_prefix = iri(uri::peepo, "control-");
// Where the engine may write or read a bundle: a plain file URI to a .ingen
// directory. Ingen does not decode escaped characters consistently.
bool bundle_location(const std::string& location) {
    return location.starts_with("file:///") && location.ends_with(".ingen") && safe_iri(location) && location.find('%') == std::string::npos &&
           location.find("/../") == std::string::npos;
}
std::string joined(const std::vector<std::string>& names) {
    std::string text;
    for (const auto& name : names) text += (text.empty() ? "" : ", ") + name;
    return text;
}
std::string child(const std::string& parent, std::string_view symbol) { return parent + "/" + std::string(symbol); }
std::string parent_of(const std::string& path) { return path.substr(0, path.rfind('/')); }
std::string symbol_of(const std::string& path) { return path.substr(path.rfind('/') + 1); }
bool under_main(const std::string& path) { return path == main_path || path.starts_with(main_path + "/"); }
Value number(double value) { return {Value::Kind::Number, value, false, {}}; }
std::optional<double> finite(const Value* value) {
    if (value && value->kind == Value::Kind::Number && std::isfinite(value->number)) return value->number;
    return std::nullopt;
}
std::string position(int x, int y) {
    return "ingen:canvasX " + float_literal(std::clamp(x, module_min_x, module_max_x)) + " ;\n\t\tingen:canvasY " +
           float_literal(std::clamp(y, module_min_y, module_max_y));
}
// Paths the engine is given for a plugin to read: absolute, with nothing a
// literal cannot carry. Spaces are fine in an atom:Path.
bool file_path(const std::string& path) {
    if (path.empty() || path[0] != '/' || path.size() > 1024 || path.find("/../") != std::string::npos) return false;
    return std::none_of(path.begin(), path.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f; });
}
// The control port a plugin takes patch messages on: its atom input, one
// that is not for MIDI when it has both.
std::string message_port(const ModuleDefinition& d) {
    for (const auto kind : {PortType::Atom, PortType::Midi})
        for (const auto& p : d.ports) if (p.type == kind && p.direction == Direction::Input) return p.symbol;
    return {};
}
// Ingen numbers a controller or note binding channel * 256 + number, and
// counts channels from 0; pitch bend and pressure carry only the channel.
constexpr std::pair<MidiBinding::Kind, std::string_view> binding_types[] = {
    {MidiBinding::Kind::Controller, "Controller"}, {MidiBinding::Kind::Note, "NoteOn"}, {MidiBinding::Kind::Bender, "Bender"}, {MidiBinding::Kind::Pressure, "ChannelPressure"}};
std::optional<MidiBinding> midi_binding(const Value* value) {
    if (!value || value->kind != Value::Kind::Object || value->number < 0 || value->number != std::round(value->number) || value->number >= 16 * 256) return std::nullopt;
    const auto n = static_cast<int>(value->number);
    for (const auto& [kind, local] : binding_types) {
        if (value->text != iri(uri::midi, local)) continue;
        const bool numbered = kind == MidiBinding::Kind::Controller || kind == MidiBinding::Kind::Note;
        if (numbered && n % 256 < 128) return MidiBinding{kind, n / 256 + 1, n % 256};
        if (!numbered && n < 16) return MidiBinding{kind, n + 1, 0};
    }
    return std::nullopt;
}
Value binding_value(const MidiBinding& b) {
    const bool numbered = b.kind == MidiBinding::Kind::Controller || b.kind == MidiBinding::Kind::Note;
    for (const auto& [kind, local] : binding_types)
        if (kind == b.kind) return {Value::Kind::Object, double(numbered ? (b.channel - 1) * 256 + b.number : b.channel - 1), false, iri(uri::midi, local)};
    return {};
}
std::string binding_literal(const MidiBinding& b) {
    const auto value = binding_value(b);
    const char* field = b.kind == MidiBinding::Kind::Controller ? "controllerNumber" : b.kind == MidiBinding::Kind::Note ? "noteNumber" : "channel";
    return "[\n\t\ta <" + value.text + "> ;\n\t\tmidi:" + field + " \"" + std::to_string(static_cast<int>(value.number)) + "\"^^xsd:int\n\t]";
}
// A block whose plugin spills when bypassed carries the mark from the start,
// so the engine saves it with the patch.
std::string block(const std::string& plugin, const std::string& name, int x, int y, bool bypassed, bool spill) {
    return "a ingen:Block ;\n\t\tlv2:prototype <" + plugin + "> ;\n\t\t" + position(x, y) + " ;\n\t\tpeepo:displayName " + string_literal(name) +
           (bypassed ? " ;\n\t\tingen:enabled false" : "") + (spill ? " ;\n\t\tpeepo:spillBypass true" : "");
}
}
Session::Session(Options options) : options_(std::move(options)) { rebuild(); }
std::vector<std::string> Session::take_outgoing() { auto out = std::move(outgoing_); outgoing_.clear(); return out; }
std::optional<std::string> Session::take_fault() { auto fault = std::move(fault_); fault_.reset(); return fault; }

void Session::opening() {
    if (state_ == EngineState::Connecting) return;
    state_ = EngineState::Connecting;
    emit(0, true, "");
}
void Session::opened() {
    state_ = EngineState::Connecting;
    reader_ = turtle::Reader("ingen:/"); buffer_.clear(); depth_ = 0;
    outgoing_.clear(); outgoing_.push_back(prefixes());
    plugins_.clear();
    // The engine sends nothing on connect; ask for everything. The mirrored
    // graph is kept until the answer arrives, then replaced by it.
    auto& sync = begin(0, Purpose::Sync);
    send(sync, get, "ingen:/plugins");
    sync.replaces = next_sequence_;
    send(sync, get, main_path);
}
void Session::closed(std::string reason, bool retrying) {
    state_ = retrying ? EngineState::Connecting : EngineState::Disconnected;
    // A lost connection cancels requests; reconnecting never applies stale edits.
    fail_edits("Engine disconnected");
    transactions_.clear(); sequences_.clear(); outgoing_.clear(); buffer_.clear(); expected_copies_.clear();
    depth_ = 0; cpu_.reset(); fault_.reset(); sent_modified_.reset();
    tuner_.reset(); tuner_port_.clear(); tuner_frequency_.reset();
    looper_.reset(); looper_ports_ = {}; looper_status_ = {};
    learning_.clear(); cancelled_learn_.clear();
    euclidean_.reset(); euclidean_ports_ = {}; euclidean_steps_ = {}; euclidean_age_ = 0;
    emit(0, true, std::move(reason));
}
void Session::fail_edits(const std::string& message) {
    for (const auto& [id, pending] : transactions_) if (pending.purpose == Purpose::Edit) emit(pending.request, false, message);
}
void Session::emit(RequestId request, bool accepted, std::string message, bool with_session) {
    if (mirror_changed_) rebuild();
    unreported_ = false;
    events_.push_back({request, accepted, std::move(message), state_, graph_, with_session ? saved_session_ : std::nullopt});
}
void Session::reject(RequestId request, std::string message) { events_.push_back({request, false, std::move(message), state_, graph_, {}}); }

Session::Transaction& Session::begin(RequestId request, Purpose purpose) {
    const auto id = next_transaction_++;
    auto& created = transactions_[id];
    created.id = id; created.request = request; created.purpose = purpose;
    return created;
}
void Session::send(Transaction& to, std::string text, int number) {
    outgoing_.push_back(std::move(text));
    to.waiting.insert(number); to.sequences.push_back(number);
    sequences_[number] = to.id;
}
void Session::send(Transaction& to, std::string (*writer)(int, std::string_view), std::string_view subject) {
    const auto number = sequence();
    send(to, writer(number, subject), number);
}
void Session::refresh(std::string path, bool replace) {
    auto& pending = begin(0, Purpose::Refresh);
    if (replace) pending.replaces = next_sequence_;
    send(pending, get, path);
}

void Session::received(std::string_view bytes) {
    if (state_ == EngineState::Disconnected || fault_) return;
    buffer_.append(bytes);
    std::size_t start = 0;
    // One socket read is not one message: the engine ends each with a NUL.
    for (auto end = buffer_.find('\0'); end != std::string::npos && !fault_; end = buffer_.find('\0', start)) {
        std::vector<turtle::Triple> triples;
        const auto issue = reader_.parse(std::string_view(buffer_).substr(start, end - start), triples);
        start = end + 1;
        if (!issue.empty()) { fault_ = "Engine sent an unreadable message (" + issue + ")"; break; }
        for (const auto& message : decode(triples)) handle(message);
        if (depth_ == 0) finish_ready();
    }
    buffer_.erase(0, start);
    if (buffer_.size() > options_.max_message_bytes) fault_ = "Engine message exceeds the size limit";
}
void Session::handle(const Message& message) {
    switch (message.type) {
    case Message::Type::BundleStart: ++depth_; break;
    case Message::Type::BundleEnd: depth_ = std::max(0, depth_ - 1); break;
    case Message::Type::Response: {
        const auto owner = sequences_.find(message.sequence);
        if (owner == sequences_.end()) break;
        auto& pending = transactions_.at(owner->second);
        if (message.status != 0 && (pending.status == 0 || message.sequence < pending.failed)) {
            pending.status = message.status; pending.failed = message.sequence;
        }
        pending.waiting.erase(message.sequence);
        // The description follows its response inside the same bundle.
        if (message.sequence == pending.replaces && message.status == 0) {
            pending.replaces = 0;
            objects_.clear(); arcs_.clear();
            mirror_changed_ = unreported_ = true;
        }
        break;
    }
    default: apply(message);
    }
}
void Session::merge(const std::string& path, const Properties& properties) {
    // Atom commands are transient events, not editable/saved control values.
    if (properties.size() == 1 && properties.front().first == value_key && symbol_of(path) == "commands") {
        for (const auto& [id, block_path] : paths_) {
            const auto* module = find_module(graph_, id);
            if (block_path == parent_of(path) && module && looper_uri(module->uri)) return;
        }
    }
    auto& object = objects_[path];
    const bool output_value = properties.size() == 1 && properties.front().first == value_key &&
        has(object, type, iri(uri::lv2, "OutputPort")) && has(object, type, iri(uri::lv2, "ControlPort"));
    // A Put or Set replaces every value of the predicates it names.
    for (const auto& [key, ignored] : properties) std::erase_if(object, [&](const auto& entry) { return entry.first == key; });
    for (const auto& entry : properties)
        if (std::find(object.begin(), object.end(), entry) == object.end()) object.push_back(entry);
    // A Get returns the port's initial value in Ingen, not its DSP output.
    // Only single-value output notifications carry current pitch or step.
    bool monitored_output = path == tuner_port_;
    if (path == tuner_port_ && output_value) {
        if (const auto* value = find(properties, value_key)) {
            tuner_frequency_ = finite(value); tuner_age_ = 0;
        }
    }
    for (std::size_t i = 0; i < euclidean_ports_.size(); ++i) {
        if (!euclidean_ports_[i].empty() && path == euclidean_ports_[i]) {
            monitored_output = true;
            if (output_value) {
                if (const auto value = finite(find(properties, value_key)); value && *value >= 0 && *value < euclidean_max_steps) {
                    euclidean_steps_[i] = static_cast<int>(std::round(*value));
                    euclidean_age_ = 0;
                } else {
                    euclidean_steps_[i].reset();
                }
            }
        }
    }
    for (std::size_t i = 0; i < looper_ports_.size(); ++i) {
        if (!looper_ports_[i].empty() && path == looper_ports_[i]) {
            monitored_output = true;
            if (output_value) looper_status_[i] = finite(find(properties, value_key));
        }
    }
    // Output meters must not rebuild the graph or interrupt touch gestures.
    if (!output_value && !monitored_output) mirror_changed_ = unreported_ = true;
}
void Session::watch_tuner(std::optional<InstanceId> id) {
    std::string port;
    if (id && state_ == EngineState::Ready) {
        const auto* module = find_module(graph_, *id);
        const auto path = paths_.find(*id);
        if (module && !module->bypassed && tuner_uri(module->uri) && path != paths_.end()) {
            const auto candidate = child(path->second, "freq_out");
            const auto found = objects_.find(candidate);
            if (found != objects_.end() && has(found->second, type, iri(uri::lv2, "OutputPort")) &&
                has(found->second, type, iri(uri::lv2, "ControlPort"))) port = candidate;
        }
    }
    if (port == tuner_port_) return;
    if (state_ == EngineState::Ready && !tuner_port_.empty() && objects_.contains(tuner_port_)) {
        auto& tx = begin(0, Purpose::Monitor);
        const auto seq = sequence();
        send(tx, set(seq, tuner_port_, "ingen:broadcast", "false"), seq);
    }
    tuner_port_ = port; tuner_ = port.empty() ? std::nullopt : id;
    tuner_frequency_.reset(); tuner_age_ = 0;
    if (!port.empty()) {
        auto& tx = begin(0, Purpose::Monitor);
        const auto seq = sequence();
        send(tx, set(seq, port, "ingen:broadcast", "true"), seq);
    }
}
std::optional<double> Session::tuner_frequency(InstanceId id) const {
    return state_ == EngineState::Ready && tuner_ == id && tuner_age_ < 750 ? tuner_frequency_ : std::nullopt;
}
void Session::watch_euclidean(std::optional<InstanceId> id) {
    std::array<std::string, euclidean_tracks> ports{};
    if (id && state_ == EngineState::Ready) {
        const auto* module = find_module(graph_, *id);
        const auto path = paths_.find(*id);
        if (module && !module->bypassed && euclidean_uri(module->uri) && path != paths_.end()) {
            for (std::size_t i = 0; i < ports.size(); ++i) {
                const auto candidate = child(path->second, "current_step_out_" + std::to_string(i + 1));
                const auto found = objects_.find(candidate);
                if (found != objects_.end() && has(found->second, type, iri(uri::lv2, "OutputPort")) &&
                    has(found->second, type, iri(uri::lv2, "ControlPort"))) ports[i] = candidate;
            }
        }
    }
    if (ports == euclidean_ports_) return;
    if (state_ == EngineState::Ready) {
        Transaction* tx = nullptr;
        for (const auto& port : euclidean_ports_)
            if (!port.empty() && objects_.contains(port)) {
                if (!tx) tx = &begin(0, Purpose::Monitor);
                const auto seq = sequence();
                send(*tx, set(seq, port, "ingen:broadcast", "false"), seq);
            }
    }
    const bool any = std::any_of(ports.begin(), ports.end(), [](const std::string& p) { return !p.empty(); });
    euclidean_ports_ = ports;
    euclidean_ = any ? id : std::nullopt;
    euclidean_steps_ = {};
    euclidean_age_ = 0;
    if (any) {
        auto& tx = begin(0, Purpose::Monitor);
        for (const auto& port : euclidean_ports_)
            if (!port.empty()) {
                const auto seq = sequence();
                send(tx, set(seq, port, "ingen:broadcast", "true"), seq);
            }
    }
}
EuclideanSteps Session::euclidean_steps(InstanceId id) const {
    return state_ == EngineState::Ready && euclidean_ == id && euclidean_age_ < 750 ? euclidean_steps_ : EuclideanSteps{};
}
void Session::watch_looper(std::optional<InstanceId> id) {
    std::array<std::string, looper_outputs> ports{};
    if (id && state_ == EngineState::Ready) {
        const auto* module = find_module(graph_, *id);
        const auto path = paths_.find(*id);
        // Bypass keeps the looper running, including its telemetry.
        if (module && looper_uri(module->uri) && path != paths_.end()) {
            // Ingen sends every subscribed output 25 times a second, changed or
            // not (measured on the Pi): only the loops that exist are watched.
            const auto loops = std::find_if(module->parameters.begin(), module->parameters.end(), [](const Parameter& p) { return p.symbol == "loops"; });
            const auto count = loops == module->parameters.end() ? looper_loops : std::size_t(std::clamp(std::lround(loops->value), 1L, long(looper_loops)));
            for (std::size_t i = 0; i < ports.size(); ++i) {
                if (i < looper_loops * looper_fields && i / looper_fields >= count) continue;
                const auto candidate = child(path->second, looper_symbol(i));
                const auto found = objects_.find(candidate);
                if (found != objects_.end() && has(found->second, type, iri(uri::lv2, "OutputPort")) &&
                    has(found->second, type, iri(uri::lv2, "ControlPort"))) ports[i] = candidate;
            }
        }
    }
    if (ports == looper_ports_) return;
    if (state_ == EngineState::Ready) {
        auto& tx = begin(0, Purpose::Monitor);
        for (const auto& port : looper_ports_) if (!port.empty() && objects_.contains(port)) {
            const auto seq = sequence(); send(tx, set(seq, port, "ingen:broadcast", "false"), seq);
        }
        for (const auto& port : ports) if (!port.empty()) {
            const auto seq = sequence(); send(tx, set(seq, port, "ingen:broadcast", "true"), seq);
        }
    }
    // Adding or removing a loop keeps the values of the loops still watched.
    if (looper_ != id) looper_status_ = {};
    else for (std::size_t i = 0; i < ports.size(); ++i) if (ports[i] != looper_ports_[i]) looper_status_[i].reset();
    looper_ports_ = ports; looper_ = id;
}
LooperStatus Session::looper_status(InstanceId id) const {
    // Values are kept between broadcasts: a stable empty/muted loop is not stale.
    const auto* module = find_module(graph_, id);
    return state_ == EngineState::Ready && looper_ == id && module && looper_uri(module->uri) ? looper_status_ : LooperStatus{};
}
bool Session::looper_command(InstanceId id, int command, int target, bool press) {
    const auto* module = find_module(graph_, id);
    const auto path = paths_.find(id);
    if (state_ != EngineState::Ready || !module || !looper_uri(module->uri) || path == paths_.end() ||
        !looper_command_supported(command) || target < 0 || target > 9) return false;
    const auto port = child(path->second, "commands");
    const auto found = objects_.find(port);
    if (found == objects_.end() || !has(found->second, type, iri(uri::lv2, "InputPort")) ||
        !has(found->second, type, iri(uri::atom, "AtomPort"))) return false;
    // Typed xsd:int is mapped to atom:Int by Ingen's Sratom reader.
    auto& tx = begin(0, Purpose::Monitor);
    const auto seq = sequence();
    const auto value = target * 256 + command * 2 + int(press);
    send(tx, set(seq, port, "ingen:value", "\"" + std::to_string(value) + "\"^^xsd:int"), seq);
    return true;
}
void Session::learned(const std::string& port) {
    // The engine announces what a port learned. Its wait is over, and the
    // patch has changed; unless the wait was ended here, then it goes again.
    if (state_ != EngineState::Ready) return;
    auto& tx = begin(0, Purpose::Edit);
    if (port == cancelled_learn_) {
        cancelled_learn_.clear();
        std::erase_if(objects_[port], [](const auto& entry) { return entry.first == binding_key; });
        const auto number = sequence();
        send(tx, remove_property(number, port, "midi:binding"), number);
    } else if (port == learning_) {
        // Only the client that asked marks it: another, such as peepo-midi, only watches.
        learning_.clear();
        if (!sent_modified_.value_or(graph_.dirty)) mark(tx, true);
    }
    if (tx.waiting.empty()) transactions_.erase(tx.id);
}
void Session::erase(const std::string& path) {
    const auto prefix = path + "/";
    std::erase_if(objects_, [&](const auto& entry) { return entry.first == path || entry.first.starts_with(prefix); });
    // The engine drops a deleted block's cables without announcing each one.
    std::erase_if(arcs_, [&](const auto& arc) {
        return arc.first == path || arc.second == path || arc.first.starts_with(prefix) || arc.second.starts_with(prefix);
    });
    mirror_changed_ = unreported_ = true;
}
void Session::apply(const Message& message) {
    auto arc_end = [&](const Properties& body, std::string_view local) {
        const auto* end = find(body, iri(uri::ingen, local));
        return end && end->kind == Value::Kind::Uri ? normal_path(end->text) : std::string();
    };
    switch (message.type) {
    case Message::Type::Put:
        if (message.subject == "ingen:/engine") {
            // Mean share of the audio period spent processing since the engine started.
            if (const auto load = finite(find(message.body, iri(uri::ingen, "meanRunLoad")))) cpu_ = *load * 100.0;
        } else if (has(message.body, type, arc_type)) {
            const std::pair arc{arc_end(message.body, "tail"), arc_end(message.body, "head")};
            if (!arc.first.empty() && !arc.second.empty() && std::find(arcs_.begin(), arcs_.end(), arc) == arcs_.end()) {
                arcs_.push_back(arc); mirror_changed_ = unreported_ = true;
            }
        } else if (under_main(message.subject)) merge(message.subject, message.body);
        else if (has(message.body, type, iri(uri::lv2, "Plugin")) || has(message.body, type, iri(uri::ingen, "Internal"))) plugins_.insert(message.subject);
        break;
    case Message::Type::Set:
        if (!under_main(message.subject)) break;
        merge(message.subject, {{message.property, message.value}});
        if (message.property == binding_key && message.value.kind == Value::Kind::Object) learned(message.subject);
        break;
    case Message::Type::Patch:
        if (!under_main(message.subject)) break;
        for (const auto& removed : message.remove) {
            const bool any = removed.second.kind == Value::Kind::Uri && removed.second.text == iri(uri::patch, "wildcard");
            std::erase_if(objects_[message.subject], [&](const auto& entry) { return entry.first == removed.first && (any || entry.second == removed.second); });
        }
        merge(message.subject, message.body);
        break;
    case Message::Type::Delete:
        if (!message.subject.empty()) {
            if (under_main(message.subject) && message.subject != main_path) erase(message.subject);
        } else if (has(message.body, type, arc_type)) {
            const auto tail = arc_end(message.body, "tail"), head = arc_end(message.body, "head"), all = arc_end(message.body, "incidentTo");
            const auto removed = std::erase_if(arcs_, [&](const auto& arc) {
                return all.empty() ? arc.first == tail && arc.second == head : arc.first == all || arc.second == all;
            });
            if (removed) mirror_changed_ = unreported_ = true;
        }
        break;
    case Message::Type::Copy:
        // A copy is announced, but its contents are not sent: ask for them.
        if (under_main(message.destination) && !expected_copies_.erase(message.destination)) refresh(message.destination, false);
        break;
    default: break;
    }
}
void Session::finish_ready() {
    std::vector<TransactionId> done;
    for (const auto& [id, pending] : transactions_) if (pending.waiting.empty()) done.push_back(id);
    for (const auto id : done) {
        auto node = transactions_.extract(id);
        for (const auto number : node.mapped().sequences) sequences_.erase(number);
        finish(node.mapped());
    }
}
void Session::finish(Transaction& done) {
    bool ok = done.status == 0;
    switch (done.purpose) {
    case Purpose::Edit: {
        if (ok && done.next != Transaction::Next::None) { proceed(done); break; }
        if (!ok && done.restore_id && state_ == EngineState::Ready) {
            // A refused save must not leave the graph claiming an identity
            // nothing on disk has. Sent before the re-read below, which then sees it.
            auto& back = begin(0, Purpose::Refresh);
            const auto number = sequence();
            send(back, set_on_graph(number, main_path, "peepo:patchId", string_literal(*done.restore_id)), number);
        }
        if (ok) {
            for (const auto& [path, predicate] : done.dropped)
                if (const auto object = objects_.find(path); object != objects_.end()) std::erase_if(object->second, [&](const auto& entry) { return entry.first == predicate; });
            for (const auto& local : done.local) merge(local.path, {{local.predicate, local.value}});
            mirror_changed_ = true;
        } else if (done.sequences.size() > 1 && state_ == EngineState::Ready) {
            // Some steps of a failed multi-step edit may have taken effect.
            refresh(main_path, true);
        }
        auto message = ok ? done.message : std::string(status_text(done.status));
        if (ok && done.verify) {
            bool failed = false;
            message = verified(done, failed);
            ok = !failed;
        }
        if (ok && done.snapshot) {
            if (mirror_changed_) rebuild();
            saved_session_ = graph_;
        }
        emit(done.request, ok, std::move(message), ok && done.snapshot);
        // Once nothing is in flight the mirror says what the engine holds,
        // including a marker another client changed.
        if (std::none_of(transactions_.begin(), transactions_.end(), [](const auto& t) { return t.second.purpose == Purpose::Edit; })) sent_modified_.reset();
        break;
    }
    case Purpose::Sync:
        if (ok) ensure_endpoints();
        else fault_ = std::string("Engine refused to describe its graph: ") + status_text(done.status);
        break;
    case Purpose::Endpoints:
        if (ok) ready();
        else { state_ = EngineState::Error; emit(0, false, std::string("Engine could not create the audio endpoints: ") + status_text(done.status)); }
        break;
    case Purpose::Refresh:
        // A single object may have vanished again; only a failed full refresh matters.
        if (!ok && done.replaces) fault_ = std::string("Engine refused to describe its graph: ") + status_text(done.status);
        break;
    case Purpose::Monitor:
        if (!ok) { tuner_frequency_.reset(); euclidean_steps_ = {}; looper_status_ = {}; }
        break;
    }
}
void Session::ensure_endpoints() {
    Transaction* creating = nullptr;
    for (const auto& endpoint : endpoints) {
        const auto path = child(main_path, endpoint.symbol);
        const bool input = endpoint.module == 1;
        const auto existing = objects_.find(path);
        const std::string direction = input ? "a lv2:InputPort" : "a lv2:OutputPort";
        if (existing == objects_.end()) {
            if (!creating) creating = &begin(0, Purpose::Endpoints);
            const auto number = sequence();
            // A MIDI endpoint is a sequence of MIDI events; Ingen gives it a JACK MIDI port.
            send(*creating, put(number, path, endpoint.midi ? direction + " , atom:AtomPort ;\n\t\tatom:bufferType atom:Sequence ;\n\t\tatom:supports midi:MidiEvent"
                                                            : direction + " , lv2:AudioPort"), number);
        } else if (!has(existing->second, type, endpoint.midi ? iri(uri::atom, "AtomPort") : iri(uri::lv2, "AudioPort")) ||
                   !has(existing->second, type, iri(uri::lv2, input ? "InputPort" : "OutputPort"))) {
            state_ = EngineState::Error;
            emit(0, false, std::string("Engine port ") + endpoint.symbol + (endpoint.midi ? " is not the expected MIDI endpoint" : " is not the expected audio endpoint"));
            return;
        }
    }
    if (!creating) ready();
}
void Session::ready() {
    state_ = EngineState::Ready;
    mirror_changed_ = true;
    emit(0, true, "");
}

void Session::rebuild() {
    mirror_changed_ = false;
    Graph graph;
    graph.name = "Untitled";
    if (const auto root = objects_.find(main_path); root != objects_.end()) {
        if (const auto* name = find(root->second, patch_name_key); name && name->kind == Value::Kind::String && validate_patch_name(name->text).empty())
            graph.name = name->text;
        if (const auto* id = find(root->second, patch_id_key); id && id->kind == Value::Kind::String && valid_patch_id(id->text)) graph.patch_id = id->text;
        if (const auto* modified = find(root->second, modified_key); modified && modified->kind == Value::Kind::Boolean) graph.dirty = modified->boolean;
        if (const auto tempo = finite(find(root->second, tempo_key)); tempo && *tempo >= minimum_tempo && *tempo <= maximum_tempo) graph.tempo = *tempo;
        if (const auto* sent = find(root->second, load_messages_key); sent && sent->kind == Value::Kind::String)
            if (auto messages = decode_control_changes(sent->text)) graph.load_messages = std::move(*messages);
    }
    const auto& in = demo_catalogue()[0];
    const auto& out = demo_catalogue()[1];
    graph.modules.push_back({1, in.uri, in.name, 32, 132, true, false, {}, in.ports});
    graph.modules.push_back({2, out.uri, out.name, 664, 132, true, false, {}, out.ports});

    std::vector<std::string> blocks;
    for (const auto& [path, properties] : objects_)
        if (parent_of(path) == main_path && has(properties, type, block_type)) blocks.push_back(path);
    // Identities last as long as the object does. A block the interface made is
    // named after its ID; anything else gets a number nobody has used.
    std::erase_if(ids_, [&](const auto& entry) { return std::find(blocks.begin(), blocks.end(), entry.first) == blocks.end(); });
    std::set<InstanceId> used{1, 2};
    for (const auto& [path, id] : ids_) used.insert(id);
    auto own_id = [](const std::string& symbol) -> InstanceId {
        const bool numbered = symbol.size() > 1 && symbol.size() < 17 && symbol[0] == 'm' && std::all_of(symbol.begin() + 1, symbol.end(), [](char c) { return c >= '0' && c <= '9'; });
        return numbered ? std::stoull(symbol.substr(1)) : 0;
    };
    // Named blocks first, so that a stranger never takes one of their numbers.
    for (const bool named : {true, false})
        for (const auto& path : blocks) {
            if (ids_.contains(path)) continue;
            auto id = own_id(symbol_of(path));
            if (named && (id < 3 || used.contains(id))) continue;
            if (!named) { while (used.contains(next_instance_)) ++next_instance_; id = next_instance_; }
            ids_[path] = id; used.insert(id);
            next_instance_ = std::max(next_instance_, id + 1);
        }
    paths_.clear();
    for (const auto& [path, id] : ids_) paths_[id] = path;

    int unplaced = 0;
    for (const auto& [id, path] : paths_) {
        const auto& properties = objects_.at(path);
        const auto* prototype = find(properties, iri(uri::lv2, "prototype"));
        Module m{id, prototype && prototype->kind == Value::Kind::Uri ? prototype->text : std::string("urn:peepo:missing:unknown"), {}, 0, 0, false, false, {}, {}};
        const auto* known = definition(m.uri);
        const auto* name = find(properties, name_key);
        m.name = name && name->kind == Value::Kind::String && !name->text.empty() ? name->text : known ? known->name : symbol_of(path);
        const auto x = finite(find(properties, x_key)), y = finite(find(properties, y_key));
        if (x && y) {
            m.x = static_cast<int>(std::clamp(std::round(*x), double(module_min_x), double(module_max_x)));
            m.y = static_cast<int>(std::clamp(std::round(*y), double(module_min_y), double(module_max_y)));
        } else {
            // Made by another client without a position: lay them out in a row.
            m.x = std::min(module_max_x, 184 + 144 * unplaced++); m.y = 276;
        }
        if (const auto* enabled = find(properties, enabled_key); enabled && enabled->kind == Value::Kind::Boolean) m.bypassed = !enabled->boolean;
        if (const auto* file = find(properties, file_key); file && file->kind == Value::Kind::String && file_path(file->text)) m.file = file->text;
        // A plugin's own on/off control stays on: off, it writes silence. Earlier
        // versions bypassed by clearing it, so a cleared one still reads as
        // bypassed; a plugin that bypasses itself is bypassed that way now.
        if (known && !known->enable_symbol.empty())
            if (const auto own = objects_.find(child(path, known->enable_symbol)); own != objects_.end() && finite(find(own->second, value_key)).value_or(0) < 0.5) m.bypassed = true;

        struct Child { double index; std::string symbol; const Properties* properties; };
        std::vector<Child> children;
        const auto prefix = path + "/";
        for (auto it = objects_.lower_bound(prefix); it != objects_.end() && it->first.starts_with(prefix); ++it)
            if (parent_of(it->first) == path) children.push_back({finite(find(it->second, iri(uri::lv2, "index"))).value_or(1e9), symbol_of(it->first), &it->second});
        std::stable_sort(children.begin(), children.end(), [](const Child& a, const Child& b) { return a.index < b.index; });
        for (const auto& port : children) {
            auto is = [&](std::string_view space, std::string_view local) { return has(*port.properties, type, iri(space, local)); };
            const bool input = is(uri::lv2, "InputPort");
            if (!input && !is(uri::lv2, "OutputPort")) continue;
            const auto direction = input ? Direction::Input : Direction::Output;
            if (is(uri::lv2, "AudioPort")) m.ports.push_back({port.symbol, direction, 1, PortType::Audio, {}});
            else if (is(uri::lv2, "CVPort")) m.ports.push_back({port.symbol, direction, 1, PortType::Cv, {}});
            else if (is(uri::atom, "AtomPort"))
                m.ports.push_back({port.symbol, direction, 1, has(*port.properties, iri(uri::atom, "supports"), iri(uri::midi, "MidiEvent")) ? PortType::Midi : PortType::Atom, {}});
            else if (input && is(uri::lv2, "ControlPort")) {
                // Ranges, units and names come from the catalogue: the engine
                // sends only the current value, and does not check it.
                Parameter parameter{port.symbol, port.symbol, "", ParameterKind::Continuous, 0, 1, 0, false, {}};
                bool described = false;
                if (known) for (const auto& candidate : known->parameters) if (candidate.symbol == port.symbol) { parameter = candidate; described = true; }
                // A catalogue entry lists the controls worth showing. What it
                // leaves out (an enable switch, a control marked as not for
                // interfaces) stays out; only an unknown plugin shows them all.
                if (known && !described) continue;
                auto value = finite(find(*port.properties, value_key)).value_or(parameter.value);
                if (!described) { parameter.minimum = std::min(0.0, value); parameter.maximum = std::max(1.0, value); }
                if (parameter.kind != ParameterKind::Continuous) value = std::round(value);
                parameter.value = std::clamp(value, parameter.minimum, parameter.maximum);
                parameter.midi = midi_binding(find(*port.properties, binding_key));
                m.parameters.push_back(std::move(parameter));
            }
        }
        if (known)
            for (auto parameter : known->parameters)
                if (parameter.symbol == general_output_symbol) {
                    parameter.value = std::clamp(finite(find(properties, output_level_key)).value_or(parameter.value), parameter.minimum, parameter.maximum);
                    m.parameters.push_back(std::move(parameter));
                }
        if (known) std::stable_sort(m.parameters.begin(), m.parameters.end(), [&](const Parameter& a, const Parameter& b) {
            auto rank = [&](const Parameter& p) {
                return std::find_if(known->parameters.begin(), known->parameters.end(), [&](const Parameter& k) { return k.symbol == p.symbol; }) - known->parameters.begin();
            };
            return rank(a) < rank(b);
        });
        graph.modules.push_back(std::move(m));
    }
    auto end = [&](const std::string& path, bool source) -> std::optional<std::pair<InstanceId, std::string>> {
        const auto owner = parent_of(path);
        if (owner == main_path) {
            for (const auto& endpoint : endpoints)
                if (symbol_of(path) == endpoint.symbol && (endpoint.module == 1) == source) return std::pair{endpoint.module, std::string(endpoint.port)};
            return std::nullopt;
        }
        if (const auto id = ids_.find(owner); id != ids_.end()) return std::pair{id->second, symbol_of(path)};
        return std::nullopt;
    };
    for (const auto& [tail, head] : arcs_) {
        const auto from = end(tail, true), to = end(head, false);
        if (!from || !to) continue;
        Connection cable{from->first, to->first, from->second, to->second};
        if (std::find(graph.connections.begin(), graph.connections.end(), cable) == graph.connections.end()) graph.connections.push_back(std::move(cable));
    }
    if (const auto root = objects_.find(main_path); root != objects_.end())
        for (const auto& [key, value] : root->second) {
            if (!key.starts_with(control_prefix) || value.kind != Value::Kind::String) continue;
            for (const auto& part : assignment_parts(value.text)) {
                Assignment a;
                std::string block;
                if (!decode_assignment(key.substr(control_prefix.size()), part, a, block)) continue;
                // A block that is gone leaves the assignment orphaned: shown, never applied.
                if (const auto id = ids_.find(child(main_path, block)); id != ids_.end()) a.module = id->second;
                graph.assignments.push_back(std::move(a));
            }
        }
    graph_ = std::move(graph);
}

std::string Session::address(InstanceId id, const std::string& symbol, Direction direction, bool current) const {
    if (id == 1 || id == 2) {
        for (const auto& endpoint : endpoints)
            if (endpoint.module == id && symbol == endpoint.port && (id == 1) == (direction == Direction::Output)) return child(main_path, endpoint.symbol);
        return {};
    }
    if (!lv2_symbol(symbol)) return {};
    if (!current) return child(child(main_path, "m" + std::to_string(id)), symbol);
    const auto path = paths_.find(id);
    return path == paths_.end() ? std::string() : child(path->second, symbol);
}
void Session::name_patch(Transaction& to, const std::string& name) {
    const auto number = sequence();
    send(to, set_on_graph(number, main_path, "peepo:patchName", string_literal(name)), number);
    to.local.push_back({main_path, patch_name_key, {Value::Kind::String, 0, false, name}});
}
void Session::identify(Transaction& to, const std::string& id) {
    const auto number = sequence();
    send(to, set_on_graph(number, main_path, "peepo:patchId", string_literal(id)), number);
    to.local.push_back({main_path, patch_id_key, {Value::Kind::String, 0, false, id}});
}
void Session::mark(Transaction& to, bool modified) {
    const auto number = sequence();
    send(to, set_on_graph(number, main_path, "peepo:modified", modified ? "true" : "false"), number);
    to.local.push_back({main_path, modified_key, {Value::Kind::Boolean, 0, modified, {}}});
    sent_modified_ = modified;
}
void Session::control_property(Transaction& to, const std::string& control, const std::string& text) {
    // An empty value is an unpinned control; the engine has no way to remove a property.
    const auto number = sequence();
    send(to, set_on_graph(number, main_path, "peepo:control-" + control, string_literal(text)), number);
    to.local.push_back({main_path, control_prefix + control, {Value::Kind::String, 0, false, text}});
}
std::string Session::control_text(const std::vector<Assignment>& held) const {
    std::string text;
    for (const auto& a : held)
        if (const auto path = paths_.find(a.module); path != paths_.end())
            text += (text.empty() ? "" : " ") + encode_assignment(a, symbol_of(path->second));
    return text;
}
void Session::proceed(Transaction& done) {
    // The engine has answered the save or load itself. Its identity steps
    // took effect; what remains is to read back and clear the marker.
    for (const auto& local : done.local) merge(local.path, {{local.predicate, local.value}});
    auto& next = begin(done.request, Purpose::Edit);
    next.message = std::move(done.message);
    next.expected = std::move(done.expected);
    next.location = std::move(done.location);
    next.patch_id = std::move(done.patch_id);
    if (done.next == Transaction::Next::Loaded) {
        // A load is answered before the loaded graph is announced. A Get sent
        // now is queued behind it, so its answer is the loaded graph.
        // A patch saved before its modules spilled is marked now, before the
        // Get, so what is read back is what the next save writes.
        for (const auto& block : next.expected)
            if (const auto* known = definition(block.plugin); known && known->spill_bypass) {
                const auto number = sequence();
                send(next, set(number, child(main_path, block.symbol), "peepo:spillBypass", "true"), number);
            }
        next.replaces = next_sequence_;
        send(next, get, main_path);
        next.verify = true;
        if (!next.patch_id.empty()) identify(next, next.patch_id);
    }
    mark(next, false);
}
std::string Session::verified(const Transaction& done, bool& failed) {
    if (mirror_changed_) rebuild();
    // The engine reports success even when it could not read the bundle at all.
    std::vector<std::string> lost;
    for (const auto& block : done.expected) if (!objects_.contains(child(main_path, block.symbol))) lost.push_back(block.name);
    failed = !done.expected.empty() && lost.size() == done.expected.size();
    if (failed) return "The engine restored nothing from " + done.location + ". Can the engine see the patch folder at that path?";
    auto text = "Loaded " + graph_.name + ".";
    if (!lost.empty()) text += " The engine did not restore: " + joined(lost) + ".";
    return text;
}
std::string Session::clear_engine_graph(Transaction& to) {
    for (const auto& [id, path] : paths_) send(to, remove, path);
    // Assignments belong to the patch being replaced.
    if (const auto root = objects_.find(main_path); root != objects_.end())
        for (const auto& [key, value] : Properties(root->second))
            if (key.starts_with(control_prefix) && value.kind == Value::Kind::String && !value.text.empty()) control_property(to, key.substr(control_prefix.size()), "");
    // Cables between two endpoints outlive every block; the rest go with theirs.
    for (const auto& [tail, head] : arcs_)
        if (parent_of(tail) == main_path && parent_of(head) == main_path) {
            const auto number = sequence();
            send(to, remove_arc(number, tail, head), number);
        }
    return {};
}
std::string Session::create_graph(const Graph& graph, Transaction& to) {
    // Everything is checked before the first request: a refused patch must
    // leave the engine's current one untouched.
    if (auto issue = validate_graph(graph); !issue.empty()) return issue;
    for (const auto& m : graph.modules) {
        if (m.fixed) {
            if (!((m.id == 1 && m.uri == demo_catalogue()[0].uri) || (m.id == 2 && m.uri == demo_catalogue()[1].uri)))
                return "Patch endpoints do not match the engine";
            continue;
        }
        if (m.id < 3 || !safe_iri(m.uri) || !plugins_.contains(m.uri)) return std::string(status_text(22)) + ": " + m.name;
        for (const auto& parameter : m.parameters) if (!lv2_symbol(parameter.symbol)) return "Invalid parameter symbol";
    }
    for (const auto& cable : graph.connections)
        if (address(cable.from, cable.output, Direction::Output, false).empty() || address(cable.to, cable.input, Direction::Input, false).empty())
            return "Cable uses a port the engine does not have";
    clear_engine_graph(to);
    auto set_local = [&](const std::string& path, std::string_view property, const std::string& key, std::string literal, Value value) {
        const auto number = sequence();
        send(to, set(number, path, property, literal), number);
        to.local.push_back({path, key, std::move(value)});
    };
    for (const auto& m : graph.modules) {
        if (m.fixed) continue;
        const auto path = child(main_path, "m" + std::to_string(m.id));
        const auto number = sequence();
        const auto* known = definition(m.uri);
        const bool own_switch = known && !known->enable_symbol.empty(), by_port = own_switch && known->port_bypass;
        send(to, put(number, path, block(m.uri, m.name, m.x, m.y, m.bypassed && !by_port, known && known->spill_bypass)), number);
        if (own_switch) {
            const double on = by_port && m.bypassed ? 0 : 1;
            set_local(child(path, known->enable_symbol), "ingen:value", value_key, float_literal(on), ingen::number(on));
        }
        for (const auto& parameter : m.parameters)
            if (parameter.symbol == general_output_symbol) set_local(path, "peepo:outputLevel", output_level_key, float_literal(parameter.value), ingen::number(parameter.value));
            else {
                set_local(child(path, parameter.symbol), "ingen:value", value_key, float_literal(parameter.value), ingen::number(parameter.value));
                if (parameter.midi) set_local(child(path, parameter.symbol), "midi:binding", binding_key, binding_literal(*parameter.midi), binding_value(*parameter.midi));
            }
        if (known && !m.file.empty()) give_file(to, path, *known, m.file);
        next_instance_ = std::max(next_instance_, m.id + 1);
    }
    for (const auto& cable : graph.connections) {
        const auto number = sequence();
        send(to, put_arc(number, main_path + "/", address(cable.from, cable.output, Direction::Output, false),
                         address(cable.to, cable.input, Direction::Input, false)), number);
    }
    name_patch(to, graph.name);
    identify(to, graph.patch_id);
    set_tempo(to, std::clamp(graph.tempo, minimum_tempo, maximum_tempo));
    load_messages(to, graph.load_messages);
    // One property per control, holding every module a stomp switches.
    std::map<std::string, std::string> controls;
    for (const auto& a : graph.assignments) {
        auto& text = controls[a.control];
        text += (text.empty() ? "" : " ") + encode_assignment(a, "m" + std::to_string(a.module));
    }
    for (const auto& [control, text] : controls) control_property(to, control, text);
    return {};
}
void Session::set_tempo(Transaction& to, double bpm) {
    const auto number = sequence();
    send(to, set_on_graph(number, main_path, "peepo:tempo", float_literal(bpm)), number);
    to.local.push_back({main_path, tempo_key, ingen::number(bpm)});
}
void Session::load_messages(Transaction& to, const std::vector<ControlChange>& messages) {
    const auto text = encode_control_changes(messages);
    const auto number = sequence();
    send(to, set_on_graph(number, main_path, "peepo:midiOnLoad", string_literal(text)), number);
    to.local.push_back({main_path, load_messages_key, {Value::Kind::String, 0, false, text}});
}
void Session::give_file(Transaction& to, const std::string& path, const ModuleDefinition& d, const std::string& file) {
    // The plugin is told on its control port, as a patch:Set with a path; it
    // loads the file in the background and keeps it in its own state.
    auto number = sequence();
    send(to, set(number, child(path, message_port(d)), "ingen:activity",
                 "[\n\t\ta patch:Set ;\n\t\tpatch:property <" + d.file.property + "> ;\n\t\tpatch:value " + string_literal(file) + "^^atom:Path\n\t]"), number);
    number = sequence();
    send(to, set(number, path, "peepo:file", string_literal(file)), number);
    to.local.push_back({path, file_key, {Value::Kind::String, 0, false, file}});
}
std::string Session::plan(const Command& command, Transaction& to) {
    to.dirty = true;
    auto set_local = [&](const std::string& path, std::string_view property, const std::string& key, std::string literal, Value value) {
        const auto number = sequence();
        send(to, set(number, path, property, literal), number);
        to.local.push_back({path, key, std::move(value)});
    };
    return std::visit([&](const auto& c) -> std::string {
        using T = std::decay_t<decltype(c)>;
        std::string result;
        if constexpr (std::is_same_v<T, AddModule>) {
            const auto* known = definition(c.uri);
            if (!known || known->category == "Endpoint" || known->availability != Availability::Available) return "Module is unavailable or untested";
            if (!safe_iri(c.uri) || !plugins_.contains(c.uri)) return status_text(22);
            if (next_instance_ == std::numeric_limits<InstanceId>::max()) return "Instance ID space exhausted";
            const auto number = sequence();
            const auto path = child(main_path, "m" + std::to_string(next_instance_++));
            send(to, put(number, path, block(c.uri, numbered_name(graph_, known->name), c.x, c.y, false, known->spill_bypass)), number);
            // Some plugins start switched off and stay silent until told otherwise.
            if (!known->enable_symbol.empty()) set_local(child(path, known->enable_symbol), "ingen:value", value_key, float_literal(1), ingen::number(1));
            for (const auto& symbol : known->project_defaults)
                for (const auto& parameter : known->parameters)
                    if (parameter.symbol == symbol) set_local(child(path, symbol), "ingen:value", value_key, float_literal(parameter.value), ingen::number(parameter.value));
            for (const auto& parameter : known->parameters)
                if (parameter.symbol == known->tempo_symbol) {
                    const auto value = tempo_value(parameter, graph_.tempo);
                    set_local(child(path, parameter.symbol), "ingen:value", value_key, float_literal(value), ingen::number(value));
                }
        } else if constexpr (std::is_same_v<T, Connect>) {
            if (auto issue = validate_connection(graph_, c.connection); !issue.empty()) return issue;
            const auto tail = address(c.connection.from, c.connection.output, Direction::Output, true);
            const auto head = address(c.connection.to, c.connection.input, Direction::Input, true);
            if (tail.empty() || head.empty()) return "Cable uses a port the engine does not have";
            const auto number = sequence();
            send(to, put_arc(number, main_path + "/", tail, head), number);
        } else if constexpr (std::is_same_v<T, Disconnect>) {
            if (std::find(graph_.connections.begin(), graph_.connections.end(), c.connection) == graph_.connections.end()) return "Cable is missing";
            const auto number = sequence();
            send(to, remove_arc(number, address(c.connection.from, c.connection.output, Direction::Output, true),
                                address(c.connection.to, c.connection.input, Direction::Input, true)), number);
        } else if constexpr (std::is_same_v<T, LoadGraph>) {
            to.dirty = c.graph.dirty;
            result = create_graph(c.graph, to);
        } else if constexpr (std::is_same_v<T, LoadSession>) {
            if (!saved_session_) return "No session snapshot saved";
            to.dirty = false;
            result = create_graph(*saved_session_, to);
        } else if constexpr (std::is_same_v<T, SetTempo>) {
            if (!std::isfinite(c.bpm) || c.bpm < minimum_tempo || c.bpm > maximum_tempo) return "Tempo is outside 30 to 300 BPM";
            if (c.follow) to.dirty.reset();
            set_tempo(to, c.bpm);
            for (const auto& m : graph_.modules) {
                const auto* known = definition(m.uri);
                const auto found = paths_.find(m.id);
                if (!known || known->tempo_symbol.empty() || found == paths_.end() || !module_available(m)) continue;
                for (const auto& p : m.parameters)
                    if (p.symbol == known->tempo_symbol) {
                        const auto value = tempo_value(p, c.bpm);
                        set_local(child(found->second, p.symbol), "ingen:value", value_key, float_literal(value), ingen::number(value));
                    }
            }
        } else if constexpr (std::is_same_v<T, SetLoadMessages>) {
            if (decode_control_changes(encode_control_changes(c.messages)) != c.messages) return "Invalid MIDI messages";
            load_messages(to, c.messages);
        } else if constexpr (std::is_same_v<T, NewPatch>) {
            if (auto issue = validate_patch_name(c.name); !issue.empty()) return issue;
            clear_engine_graph(to);
            set_tempo(to, default_tempo);
            load_messages(to, {});
            for (const char* side : {"l", "r"}) {
                const auto number = sequence();
                send(to, put_arc(number, main_path + "/", main_path + "/in_" + side, main_path + "/out_" + side), number);
            }
            name_patch(to, c.name);
            identify(to, "");
        } else if constexpr (std::is_same_v<T, RenamePatch>) {
            if (auto issue = validate_patch_name(c.name); !issue.empty()) return issue;
            name_patch(to, c.name);
        } else if constexpr (std::is_same_v<T, SaveGraph>) {
            if (c.scope == SaveScope::Session) {
                // Kept by the interface, not by the engine: routing, values and layout only.
                if (c.include_plugin_state) return "Unsupported capability: plugin state capture in a session snapshot";
                to.dirty = false; to.snapshot = true;
                to.message = "Session snapshot saved in memory. Closing the app clears it; plugin state is not captured.";
            } else {
                // The engine writes the graph, plugin state included, then
                // answers. The marker is cleared only after that answer.
                if (!bundle_location(c.location)) return "Invalid save location";
                if (!valid_patch_id(c.patch_id)) return "Invalid patch identity";
                to.dirty.reset();
                to.restore_id = graph_.patch_id;
                identify(to, c.patch_id);
                const auto number = sequence();
                send(to, set_on_graph(number, main_path, "peepo:schemaVersion", "\"1\"^^xsd:int"), number);
                const auto saving = sequence();
                send(to, copy(saving, main_path, c.location), saving);
                to.next = Transaction::Next::Saved;
                to.message = "Saved " + graph_.name + ".";
            }
        } else if constexpr (std::is_same_v<T, Assign>) {
            auto a = c.assignment;
            const auto* m = find_module(graph_, a.module);
            const auto found = paths_.find(a.module);
            if (!valid_control_id(a.control) || !m || found == paths_.end()) return "Invalid control assignment";
            a.plugin = m->uri;
            if (!assignment_target(graph_, a)) return "Control target is unavailable";
            if (const auto* p = assignment_parameter(graph_, a); p && (a.minimum < p->minimum || a.maximum > p->maximum || a.minimum > a.maximum))
                return "Invalid assignment parameter range";
            control_property(to, a.control, control_text(assignments_after(graph_, a, c.add)));
        } else if constexpr (std::is_same_v<T, Unassign>) {
            const auto cleared = [&](const Assignment& a) { return a.control == c.control && (!c.module || a.module == c.module); };
            if (std::none_of(graph_.assignments.begin(), graph_.assignments.end(), cleared)) return "Control is not assigned";
            std::vector<Assignment> kept;
            for (const auto& a : graph_.assignments) if (a.control == c.control && !cleared(a)) kept.push_back(a);
            control_property(to, c.control, control_text(kept));
        } else if constexpr (std::is_same_v<T, LoadPatch>) {
            if (!bundle_location(c.location)) return "Invalid patch location";
            std::vector<std::string> absent;
            for (const auto& block : c.blocks) {
                if (!lv2_symbol(block.symbol) || !safe_iri(block.plugin)) return "The saved patch has a block the engine cannot address";
                if (!plugins_.contains(block.plugin)) absent.push_back(block.name.empty() ? block.plugin : block.name);
                else to.expected.push_back(block);
            }
            // Ingen loads none of a bundle when one of its plugins is missing.
            if (!absent.empty()) return "Not installed in the engine: " + joined(absent);
            if (!c.patch_id.empty() && !valid_patch_id(c.patch_id)) return "Invalid patch identity";
            to.location = c.location;
            to.patch_id = c.patch_id;
            to.dirty.reset();
            clear_engine_graph(to);
            // The bundle brings its own identity and name. Graph properties
            // outlive the blocks, so one without them, or one the engine cannot
            // read, must not leave the emptied graph claiming the previous patch's.
            identify(to, "");
            name_patch(to, "");
            load_messages(to, {});
            // The bundle's own tempo, which it then sets again; one from before
            // tempo was saved plays at the default.
            const auto tempo = c.tempo.value_or(default_tempo);
            set_tempo(to, std::isfinite(tempo) ? std::clamp(tempo, minimum_tempo, maximum_tempo) : default_tempo);
            const auto number = sequence();
            send(to, copy(number, c.location, main_path), number);
            to.next = Transaction::Next::Loaded;
        } else {
            const auto* m = find_module(graph_, c.id);
            const auto found = paths_.find(c.id);
            if (!m || (!m->fixed && found == paths_.end())) return "Module is missing";
            if constexpr (std::is_same_v<T, RemoveModule>) {
                if (m->fixed) return "Endpoints cannot be deleted";
                send(to, remove, found->second);
                // Its controls go with it, so nothing later given its identity
                // inherits them; a stomp keeps the other modules it switches.
                std::set<std::string> touched;
                for (const auto& a : graph_.assignments) if (a.module == c.id) touched.insert(a.control);
                for (const auto& control : touched) {
                    std::vector<Assignment> kept;
                    for (const auto& a : graph_.assignments) if (a.control == control && a.module != c.id) kept.push_back(a);
                    control_property(to, control, control_text(kept));
                }
            } else if constexpr (std::is_same_v<T, MoveModule>) {
                if (m->fixed) return "Endpoints are fixed";
                const auto number = sequence();
                send(to, put(number, found->second, position(c.x, c.y)), number);
                to.local.push_back({found->second, x_key, ingen::number(std::clamp(c.x, module_min_x, module_max_x))});
                to.local.push_back({found->second, y_key, ingen::number(std::clamp(c.y, module_min_y, module_max_y))});
            } else if constexpr (std::is_same_v<T, DuplicateModule>) {
                if (m->fixed) return "Endpoints cannot be duplicated";
                if (!module_available(*m)) return "Unavailable m cannot be duplicated";
                if (next_instance_ == std::numeric_limits<InstanceId>::max()) return "Instance ID space exhausted";
                // The engine copies the block with its values,
                // announces the copy without its contents, and leaves it in place.
                const auto copy_path = child(main_path, "m" + std::to_string(next_instance_++));
                expected_copies_.insert(copy_path);
                auto number = sequence();
                send(to, copy(number, found->second, copy_path), number);
                number = sequence();
                // The engine applies a block's output level only when told, so the copy is told too.
                std::string level;
                for (const auto& p : m->parameters) if (p.symbol == general_output_symbol) level = " ;\n\t\tpeepo:outputLevel " + float_literal(p.value);
                const auto* known = definition(m->uri);
                const auto name = numbered_name(graph_, known ? known->name : m->name);
                send(to, put(number, copy_path, position(c.x, c.y) + " ;\n\t\tpeepo:displayName " + string_literal(name) + level), number);
                // Nor does it copy the plugin's file: the copy is given it again.
                if (known && !m->file.empty()) give_file(to, copy_path, *known, m->file);
                send(to, get, copy_path);
            } else if constexpr (std::is_same_v<T, SetBypass>) {
                if (m->fixed) return "Endpoints cannot be bypassed";
                if (!module_available(*m)) return "Module is unavailable";
                // The engine's bypass passes each input to its output. A plugin's
                // own switch would only mute it, so that stays on, unless the
                // plugin bypasses itself: then the switch is the bypass, and the
                // block keeps running.
                const auto* known = definition(m->uri);
                const bool by_port = known && !known->enable_symbol.empty() && known->port_bypass;
                if (known && !known->enable_symbol.empty()) {
                    const double on = by_port && c.value ? 0 : 1;
                    set_local(child(found->second, known->enable_symbol), "ingen:value", value_key, float_literal(on), ingen::number(on));
                }
                const bool enabled = by_port || !c.value;
                set_local(found->second, "ingen:enabled", enabled_key, enabled ? "true" : "false", {Value::Kind::Boolean, 0, enabled, {}});
            } else if constexpr (std::is_same_v<T, SetFile>) {
                if (m->fixed || !module_available(*m)) return "Module is unavailable";
                const auto* known = definition(m->uri);
                if (!known || known->file.property.empty() || message_port(*known).empty()) return "This module reads no file";
                if (!file_path(c.path)) return "Invalid file path";
                give_file(to, found->second, *known, c.path);
            } else if constexpr (std::is_same_v<T, LearnMidi>) {
                if (m->fixed || !module_available(*m)) return "Module is unavailable";
                const auto it = std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == c.symbol; });
                // The general Output is the block's own level, not a port Ingen can bind.
                if (it == m->parameters.end() || c.symbol == general_output_symbol || !lv2_symbol(c.symbol)) return "This control cannot be set by MIDI";
                const auto port = child(found->second, c.symbol);
                // Ingen keeps the old binding beside a newly learned one, so it goes first.
                auto number = sequence();
                send(to, remove_property(number, port, "midi:binding"), number);
                to.dropped.push_back({port, binding_key});
                // Until something is learned, the patch is as it was.
                if (!it->midi) to.dirty.reset();
                if (c.forget) {
                    if (port == learning_) { cancelled_learn_ = port; learning_.clear(); }
                } else {
                    number = sequence();
                    send(to, set(number, port, "midi:binding", "patch:wildcard"), number);
                    learning_ = port;
                    if (cancelled_learn_ == port) cancelled_learn_.clear();
                }
            } else if constexpr (std::is_same_v<T, SetParameter>) {
                if (m->fixed || !module_available(*m)) return "Module is unavailable";
                const auto it = std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == c.symbol; });
                if (it == m->parameters.end() || !lv2_symbol(c.symbol)) return "Parameter is missing";
                // The engine stores whatever it is sent, so the range is enforced here.
                if (!std::isfinite(c.value) || c.value < it->minimum || c.value > it->maximum) return "Value is outside the parameter range";
                if (it->kind != ParameterKind::Continuous && c.value != std::round(c.value)) return "Parameter requires a discrete value";
                if (c.symbol == general_output_symbol) set_local(found->second, "peepo:outputLevel", output_level_key, float_literal(c.value), ingen::number(c.value));
                else set_local(child(found->second, c.symbol), "ingen:value", value_key, float_literal(c.value), ingen::number(c.value));
            }
        }
        return result;
    }, command);
}
RequestId Session::submit(Command command) {
    const auto id = next_request_++;
    if (state_ != EngineState::Ready) { reject(id, "Engine is unavailable"); return id; }
    if (mirror_changed_ && depth_ == 0) rebuild();
    if (transactions_.size() >= options_.max_pending) { reject(id, "Engine is busy; the edit was not sent"); return id; }
    const auto written = outgoing_.size();
    const auto marked = sent_modified_;
    auto& pending = begin(id, Purpose::Edit);
    if (auto issue = plan(command, pending); !issue.empty()) {
        for (const auto number : pending.sequences) sequences_.erase(number);
        outgoing_.resize(written);
        sent_modified_ = marked;
        transactions_.erase(pending.id);
        reject(id, std::move(issue));
        return id;
    }
    // The marker changes with the first edit after a save, and is sent once.
    if (pending.dirty && *pending.dirty != sent_modified_.value_or(graph_.dirty)) mark(pending, *pending.dirty);
    if (pending.waiting.empty()) {
        // Nothing for the engine to do, such as a snapshot of an unchanged graph.
        auto node = transactions_.extract(pending.id);
        finish(node.mapped());
    }
    return id;
}
std::vector<EngineEvent> Session::advance(unsigned milliseconds) {
    if (!tuner_port_.empty() && state_ == EngineState::Ready) {
        tuner_age_ = std::min(10000u, tuner_age_ + milliseconds);
    }
    if (euclidean_ && state_ == EngineState::Ready) {
        euclidean_age_ = std::min(10000u, euclidean_age_ + milliseconds);
    }
    std::vector<TransactionId> late;
    for (auto& [id, pending] : transactions_) if ((pending.age += milliseconds) >= options_.request_timeout_ms) late.push_back(id);
    bool resynchronize = false;
    for (const auto id : late) {
        auto node = transactions_.extract(id);
        for (const auto number : node.mapped().sequences) sequences_.erase(number);
        if (node.mapped().purpose == Purpose::Edit) { emit(node.mapped().request, false, "Engine did not answer in time"); resynchronize = true; }
        else if (node.mapped().purpose == Purpose::Monitor) { tuner_frequency_.reset(); euclidean_steps_ = {}; looper_status_ = {}; }
        else fault_ = "Engine did not answer while synchronizing";
    }
    // What a late edit did is unknown; read the graph again instead of guessing.
    if (resynchronize && !fault_ && state_ == EngineState::Ready) refresh(main_path, true);
    // Changes made by another client, or by the engine itself.
    if (unreported_ && depth_ == 0 && state_ == EngineState::Ready) emit(0, true, "");
    auto out = std::move(events_); events_.clear();
    return out;
}
}
