// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace peepo {
using InstanceId = std::uint64_t;
// Logical canvas extent; viewport movement never changes module positions.
inline constexpr int canvas_width = 4096, canvas_height = 2048;
inline constexpr int module_width = 84, module_height = 56, module_size = module_width;
inline constexpr int module_min_x = 24, module_min_y = 24;
// The patch tempo, in beats per minute, and its range.
inline constexpr double default_tempo = 120, minimum_tempo = 30, maximum_tempo = 300;
inline constexpr int module_max_x = 3984, module_max_y = 1920;
enum class ParameterKind { Continuous, Integer, Toggle, Enumeration };
// A MIDI message that sets a control (MIDI learn): the engine's own binding,
// saved with the patch. Channels are 1 to 16; `number` is the controller or
// the note, unused for pitch bend and channel pressure.
struct MidiBinding {
    enum class Kind { Controller, Note, Bender, Pressure };
    Kind kind = Kind::Controller;
    int channel = 1, number = 0;
    bool operator==(const MidiBinding&) const = default;
};
// How the bar shows it: "CC 7 / 3" (controller 7 on channel 3).
std::string midi_binding_text(const MidiBinding&);
struct Parameter {
    std::string symbol, name, unit;
    ParameterKind kind = ParameterKind::Continuous;
    double minimum = 0, maximum = 1, value = 0;
    bool logarithmic = false;
    std::vector<std::string> choices;
    // Set by MIDI learn; empty when no MIDI message sets this control.
    std::optional<MidiBinding> midi;
};
enum class Direction { Input, Output };
// Midi is an LV2 atom port that supports midi:MidiEvent, a MIDI connector.
// Atom is any other atom port, such as one a plugin takes its file on: it
// carries messages, not MIDI, and is never cabled.
enum class PortType { Audio, Control, Midi, Cv, Atom };
struct Port {
    std::string symbol;
    Direction direction;
    unsigned channels = 1;
    PortType type = PortType::Audio;
    std::vector<std::string> channel_symbols;
};
enum class Availability { Available, Untested, Unavailable };
// A file the plugin reads, chosen on its page: a NAM model or an impulse
// response. `property` is the plugin's patch:writable parameter (an
// atom:Path); the files offered are those in `folder` of the instrument's
// file store with one of `extensions`.
struct FileParameter {
    std::string property, label, folder;
    std::vector<std::string> extensions;
};
// One named page of a module's parameters, of up to four controls.
struct ParameterGroup {
    std::string name;
    std::vector<std::string> symbols;
};
struct ModuleDefinition {
    std::string uri, name, category, description;
    std::uint32_t color;
    std::vector<Port> ports;
    std::vector<Parameter> parameters;
    Availability availability = Availability::Available;
    std::string availability_reason;
    // Generated from upstream LV2 metadata, rather than an invented demo fixture.
    bool reference = false;
    // The plugin's own on/off control, when it has one. Such a module is
    // silent until that control is set, so it stays set; bypass is the
    // engine's, which passes the input through.
    std::string enable_symbol;
    // Controls whose default the project chose over the plugin's own. The
    // plugin starts at its own value, so adding the module sets these.
    std::vector<std::string> project_defaults;
    // Keep legacy identities loadable without offering duplicates in Add.
    bool hidden = false;
    // The plugin's own control for its output level, shown as Output. A module
    // with audio outputs and none of its own gets the general Output instead;
    // "none" leaves a pass-through module, such as a tuner, without either.
    std::string output_symbol;
    // Named pages for the parameter page, from the catalogue overlay. Controls
    // no group names follow on pages of their own.
    std::vector<ParameterGroup> groups;
    // The file it reads, when it has one; `property` is empty otherwise.
    FileParameter file;
    // The control that follows the patch tempo, in BPM, when it has one.
    std::string tempo_symbol;
    // Bypassed through its own on/off control instead, with the block left
    // running: for a plugin that does its own bypass, such as a delay whose
    // echoes ring on or whose memory keeps recording, which the engine's
    // bypass would stop.
    bool port_bypass = false;
    // Bypassed by the engine, which keeps the plugin running on silence and
    // adds its tail to the dry signal (urn:peepo:ns#spillBypass, engine patch
    // 0013): for a delay or reverb whose tail should ring on. Never beside
    // port_bypass.
    bool spill_bypass = false;
    // One card in Add for several plugins that do the same job, such as a
    // mono and a stereo compressor: the card's name and text, and this
    // plugin's pill on it, in the order the overlay lists them. Empty for a
    // module with a card of its own.
    std::string family, family_description, variant;
    int variant_order = 0;
};
// The general Output: not a plugin port but the block's own level, which the
// engine applies to its audio outputs (urn:peepo:ns#outputLevel, in dB).
inline constexpr const char* general_output_symbol = "peepo_output";
Parameter general_output_parameter();
struct Module {
    InstanceId id;
    std::string uri, name;
    int x, y;
    bool fixed = false, bypassed = false;
    std::vector<Parameter> parameters;
    // Retain a snapshot of ports, including when the plugin is unavailable.
    std::vector<Port> ports;
    // The file given to its file parameter, as the engine sees it; empty for none.
    std::string file;
};
struct Connection {
    InstanceId from, to;
    std::string output = "out", input = "in";
    bool operator==(const Connection&) const = default;
};
// A physical control pinned to something in this patch: an encoder to a
// parameter, a stomp switch to a parameter, to a module's software bypass or
// to a Loo-pi command. A Loo-pi command keeps its command number in
// `parameter` and its target (core/looper.hpp) in `minimum` and `maximum`.
enum class ControlAction { Parameter, Bypass, Looper };
// How a stomp switch drives a parameter: flip it on each press, or hold the
// top of the range only while pressed.
enum class SwitchMode { Toggle, Momentary };
struct Assignment {
    std::string control;
    InstanceId module = 0;
    std::string parameter;
    // The part of the parameter's range the control covers.
    double minimum = 0, maximum = 1;
    bool inverted = false;
    ControlAction action = ControlAction::Parameter;
    SwitchMode mode = SwitchMode::Toggle;
    // The target's plugin when assigned. A different module that later holds
    // the same identity is never controlled by mistake.
    std::string plugin;
};
// A MIDI control change a patch sends to every device as it loads, such as a
// pattern or preset on a pedal or drum machine. Channels are 1 to 16.
struct ControlChange {
    int channel = 1, number = 0, value = 0;
    bool operator==(const ControlChange&) const = default;
};
inline constexpr std::size_t max_load_messages = 6;
// As one engine property value: "1:7:100 3:10:64".
std::string encode_control_changes(const std::vector<ControlChange>&);
// Empty when the text is not such a list, or holds too many.
std::optional<std::vector<ControlChange>> decode_control_changes(const std::string&);
// Scenes (docs/44-scenes.md): saved states inside one patch. A scene holds
// every module's bypass and the value of each parameter marked per scene; the
// marked list is the patch's, the same for every scene.
inline constexpr std::size_t max_scenes = 8, max_scene_parameters = 16, max_scene_name = 12;
// A module a scene names. Like an assignment, it keeps the module's plugin, so
// a different module that later holds the same identity is never changed; an
// entry whose module is gone or holds another plugin is an orphan, kept and
// never applied. `block` is the engine's block symbol as read, kept for an
// orphan whose module is 0 so it is written back unchanged.
struct SceneTarget {
    InstanceId module = 0;
    std::string block, plugin;
    // The parameter, or empty for the module's bypass.
    std::string symbol;
    // The same target: by module, or by block for two orphans whose module is 0.
    bool same(const SceneTarget& o) const { return (module || o.module ? module == o.module : block == o.block) && plugin == o.plugin && symbol == o.symbol; }
    bool operator==(const SceneTarget&) const = default;
};
struct SceneEntry {
    SceneTarget target;
    // 1 when bypassed, for a bypass entry.
    double value = 0;
    bool operator==(const SceneEntry&) const = default;
};
struct Scene {
    std::string name;
    std::vector<SceneEntry> entries;
};
struct Scenes {
    std::vector<SceneTarget> parameters;
    std::vector<Scene> scenes;
    // The scene the live state was last set from, counted from 0; unused when
    // there are no scenes.
    std::size_t current = 0;
};
// "Scene 1" to "Scene 8".
std::string default_scene_name(std::size_t index);
// Empty when acceptable: 1 to 12 printable ASCII characters.
std::string validate_scene_name(const std::string&);
// The graph properties scenes are kept in: peepo:scene-count, -current
// (counted from 1, 0 with no scenes), -params, and -N-name and -N for each.
// Entries are separated by spaces, fields by '|'; plugin URIs hold neither.
struct SceneProperties {
    int count = 0, current = 0;
    std::string parameters;
    // Each scene's name and entries, in order.
    std::vector<std::pair<std::string, std::string>> scenes;
};
// `block_of` gives the engine's block symbol for a module (m3), `id_of` the
// module a block symbol names (0 when none does).
SceneProperties encode_scenes(const Scenes&, const std::function<std::string(InstanceId)>& block_of);
// Never fails: a malformed entry is dropped, a count over 8 cut, a missing or
// bad name replaced by the default, and the current scene kept in range.
Scenes decode_scenes(const SceneProperties&, const std::function<InstanceId(const std::string&)>& id_of);
struct Graph {
    std::string name = "First light";
    // The stored patch this graph was loaded from or last saved as; empty
    // when it has never been saved. Independent of the editable name.
    std::string patch_id;
    std::vector<Module> modules;
    std::vector<Connection> connections;
    std::vector<Assignment> assignments;
    bool dirty = false;
    // Beats per minute, for every module that follows the tempo.
    double tempo = default_tempo;
    // Sent to every MIDI device when the patch loads.
    std::vector<ControlChange> load_messages;
    // None for a patch without scenes, which behaves as before them.
    Scenes scenes;
};
// The module a scene entry or marked parameter names, or null when it is an
// orphan: its module is gone, holds another plugin, or lacks the parameter.
const Module* scene_target(const Graph&, const SceneTarget&);
bool scene_parameter_marked(const Graph&, InstanceId, const std::string& symbol);
// One change selecting a scene makes: a module's bypass, or a parameter.
struct SceneChange {
    InstanceId module = 0;
    // Empty for the bypass.
    std::string symbol;
    double value = 0;
    bool operator==(const SceneChange&) const = default;
};
// What differs between the live state and a scene: bypasses and marked
// parameters only, in the scene's order, orphans left out. Empty when the
// scene does not exist.
std::vector<SceneChange> scene_changes(const Graph&, std::size_t scene);
// The live state as a scene: every module's bypass and each marked parameter.
Scene capture_scene(const Graph&, std::string name);
// Scene management. Each returns an error, or empty when done; the graph is
// unchanged on an error. Adding copies the live state into the new scene,
// placed last and made current. Deleting the current scene is refused while
// others exist; deleting the only one returns the patch to no scenes, its
// marked parameters unmarked.
std::string add_scene(Graph&);
std::string rename_scene(Graph&, std::size_t scene, const std::string& name);
// The entries of `from` over those of `to`; the name stays.
std::string copy_scene(Graph&, std::size_t from, std::size_t to);
// The current scene stays current wherever it moves.
std::string move_scene(Graph&, std::size_t from, std::size_t to);
std::string delete_scene(Graph&, std::size_t scene);
// Marking copies the parameter's live value into every scene; unmarking
// leaves its live value for all of them.
std::string mark_scene_parameter(Graph&, InstanceId, const std::string& symbol);
std::string unmark_scene_parameter(Graph&, InstanceId, const std::string& symbol);
// An edit on the touch screen is written into the current scene: a bypass
// always, a parameter when it is marked. Returns whether the scene changed.
// Stomps, knobs, MIDI and learned controls are performance changes, which
// change the sound but never the stored scene, and do not call these.
bool record_scene_bypass(Graph&, InstanceId, bool bypassed);
bool record_scene_parameter(Graph&, InstanceId, const std::string& symbol, double value);
// Keeping the scenes in step with the modules. Call after the module is added
// to the graph: its bypass is written into every scene. After a duplicate is
// added: the original's entries are copied to it in every scene, and its
// marked parameters marked on the copy when they fit in the 16. Before or
// after a module is removed: its entries leave every scene. Orphans stay.
void scenes_module_added(Graph&, InstanceId);
void scenes_module_duplicated(Graph&, InstanceId original, InstanceId copy);
void scenes_module_removed(Graph&, const Module&);
// Empty when acceptable. Orphan entries are accepted; live ones must be in
// range, and each module and parameter appears once per scene.
std::string validate_scenes(const Graph&);
// Scenes read from the engine or a bundle, made to fit the modules: each live
// parameter value brought inside its range, and rounded for a discrete one,
// as the live values read with them are.
void fit_scenes(Graph&);
// The value a module's tempo control takes for a tempo: the tempo, inside the
// control's own range.
double tempo_value(const Parameter&, double tempo);
// A control measured in beats of its module's tempo (a quarter note is one
// beat), such as a delay's time, has this unit. It is shown as the note it
// lasts, and steps from one note value to the next.
inline constexpr const char* beats_unit = "beats";
struct NoteValue { const char* name; double beats; };
const std::vector<NoteValue>& note_values();
// The note a time in beats is, or empty when it is between notes.
std::string note_name(double beats);
// `steps` note values up (positive) or down from `beats`, inside the range.
double step_note(double beats, int steps, double minimum, double maximum);
struct PhysicalConnection {
    InstanceId from, to;
    std::string output, input;
    bool operator==(const PhysicalConnection&) const = default;
};
struct RoutingPlan { std::vector<PhysicalConnection> connections; std::string error; };
const std::vector<ModuleDefinition>& demo_catalogue();
const std::vector<ModuleDefinition>& reference_catalogue();
// Demo fixtures, the generated reference entries, then registered ones.
const std::deque<ModuleDefinition>& catalogue();
// Add entries an engine adapter can vouch for. Call before the interface
// starts; an entry whose URI is already known is ignored.
void register_definitions(std::vector<ModuleDefinition>);
// Offer exactly these modules, as a real engine's exported catalogue does:
// the demo and reference entries go, the IN and OUT endpoints stay. Call
// before the interface starts.
void replace_catalogue(std::vector<ModuleDefinition>);
const ModuleDefinition* definition(const std::string& uri);
const Module* find_module(const Graph&, InstanceId);
Module* find_module(Graph&, InstanceId);
Graph demo_graph();
Graph parallel_demo_graph();
Graph empty_graph(std::string name);
Graph missing_module_graph();
const std::vector<Port>& module_ports(const Module&);
// A card is module_height tall unless a side has more than two audio or MIDI
// connectors; then it grows so each keeps a band of connector_pitch.
inline constexpr int connector_pitch = 28;
int card_height(const std::vector<Port>&);
int module_height_of(const Module&);
// A new module's name: the plugin's, or with the lowest free number from 2
// when another module already has it ("Delay", "Delay 2", "Delay 3"). A copy
// is numbered from the plugin's name too, so a copy of "Delay 2" is "Delay 3".
std::string numbered_name(const Graph&, const std::string& name);
bool module_available(const Module&);
std::string validate_patch_name(const std::string&);
// Stored patch identities: 1-64 lowercase letters, digits and hyphens.
bool valid_patch_id(const std::string&);
// Physical control identities, such as enc1 or stomp3: the same characters.
bool valid_control_id(const std::string&);
// The module an assignment controls, or null when it is orphaned: its module
// is gone, holds another plugin, or lacks the parameter.
const Module* assignment_target(const Graph&, const Assignment&);
const Parameter* assignment_parameter(const Graph&, const Assignment&);
// A Loo-pi command assignment's command number, when it names one Loo-pi
// accepts and a valid target; empty otherwise.
std::optional<int> looper_assignment_command(const Assignment&);
inline int looper_assignment_target(const Assignment& a) { return static_cast<int>(a.minimum); }
// An assignment as one engine property value, naming its block by symbol (m3).
std::string encode_assignment(const Assignment&, const std::string& block);
// Back again; the module is left 0 and the block symbol returned through `block`.
bool decode_assignment(const std::string& control, const std::string& text, Assignment&, std::string& block);
// A stomp can switch the bypass of several modules at once. Its property then
// holds one encoded assignment per module, separated by spaces (no field can
// hold one); these are the parts of such a value.
std::vector<std::string> assignment_parts(const std::string& text);
// What a control holds after assigning `a`: `a` alone or, when `add` is set
// on a bypass, beside the other modules whose bypass the control switches.
std::vector<Assignment> assignments_after(const Graph&, const Assignment& a, bool add);
// The modules whose bypass a control switches, in assignment order.
std::vector<const Assignment*> bypass_assignments(const Graph&, const std::string& control);
std::string validate_parameter(const Parameter&);
std::string validate_graph(const Graph&);
std::string validate_connection(const Graph&, const Connection&, bool retain_unavailable = false);
RoutingPlan expand_connection(const Graph&, const Connection&);
double normalized_value(const Parameter&);
double value_from_normalized(const Parameter&, double);
std::string format_value(const Parameter&);
}
