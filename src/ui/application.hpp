// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "controls/controls.hpp"
#include "engine/engine.hpp"
#include "midi/settings.hpp"
#include "storage/file_store.hpp"
#include "storage/patch_store.hpp"
#include "lvgl.h"
#include <array>
#include <deque>
#include <functional>
#include <map>

namespace peepo {
enum class Page { Patch, Browser, Parameters, Patches, Settings, ModuleActions, NamePatch, ConfirmReplace, CvConnect, ConfirmAction,
                  Controls, AssignPicker, AssignmentDetail, Diagnostics, Power, Halting, FilePicker, Tempo, Midi, MidiLearn, PatchMidi, PatchActions, Scenes };
class Application {
public:
    // Without a store the Patches page offers the in-memory session snapshot.
    explicit Application(Engine&, PatchStore* = nullptr);
    ~Application();
    void tick(unsigned milliseconds);
    const Graph& graph() const { return graph_; }
    Page page() const { return page_; }
    bool pending() const { return !pending_.empty(); }
    const std::string& message() const { return message_; }
    lv_obj_t* screen() const { return screen_; }
    // A raw event from a physical control, or from keys standing in for one.
    void control(const ControlEvent&);
    const ControlLayout& control_layout() const { return layout_; }
    // How the instrument powers off, returning why it could not. Left unset
    // where the interface must never do it, such as on a desktop.
    void set_power_off(std::function<std::string()> power_off) { power_off_ = std::move(power_off); rebuild_ = true; }
    // The same for restarting the machine.
    void set_restart(std::function<std::string()> restart) { restart_ = std::move(restart); rebuild_ = true; }
    // How the instrument is reached on the network, shown in Settings so it
    // can be found without a keyboard. Left unset where it means nothing.
    struct Network { bool connected = false; std::string text; };
    void set_network(std::function<Network()> network) { network_ = std::move(network); rebuild_ = true; }
    // The machine's readings for Diagnostics, from the platform. Unknown ones
    // are shown as not measured. Read every couple of seconds on every page,
    // so a supply that drops too low is noticed even with Diagnostics closed.
    struct System {
        std::optional<double> temperature_c;
        std::optional<std::uint64_t> memory_total, memory_available;
        std::optional<bool> under_voltage;
        std::optional<double> uptime_s;
    };
    void set_system(std::function<System()> system) { system_ = std::move(system); system_age_ = 2000; }
    // The files plugins can be given (models, impulse responses); none without a store.
    void set_files(FileStore* files) { files_ = files && files->usable() ? files : nullptr; rebuild_ = true; }
    // Instrument-wide MIDI: the settings file and what peepo-midi hears.
    // Without it, Settings offers no MIDI page.
    void set_midi(const midi::Link* link) { midi_ = link; midi_settings_ = link ? link->settings() : midi::Settings{}; midi_age_ = 1000; rebuild_ = true; }
private:
    enum class Action { Navigate, Add, Module, Port, SelectCable, DeleteCable, ClearCable, Pan, Home,
                        OpenActions, Edit, Duplicate, Parameter, Bypass, Remove, SaveSession, LoadSession, State, Reject,
                        NameEntry, NameInput, Keyboard, CommitName, Confirm, CancelReplace, ParameterPage, BrowserGroup, BrowserPick, BrowserVariant, MissingFixture,
                        ParallelDemo, CvChoice, Slide,
                        SavePatch, LoadStored, DeleteStored, StartupStored, OpenPatch, ConfirmAction, CancelAction,
                        AssignControl, EditControl, ClearControl, PickModule, PickTarget, InvertControl, SwitchModeChoice, PowerOff,
                        Restart, BrowserCategory, Revert, ChooseFile, PickFile, TempoStep, Tap, LooperCommand, LooperSection, LooperTarget, LooperSelect, LooperCount, LooperBind, ModulePage,
                        MidiToggle, MidiTempoSource, MidiChannel, MidiDevice, MidiLearn, MidiForget, ClockRestart,
                        LoadMessage, AddLoadMessage, RemoveLoadMessage,
                        SceneSelect, SceneAdd, SceneRename, ScenePick, SceneCancel, SceneMode, SceneMark, SceneParameter };
    struct Binding {
        Application* app;
        Action action;
        InstanceId id = 0;
        std::string symbol;
        int number = 0;
        lv_obj_t* value_label = nullptr;
        lv_obj_t* widget = nullptr;
        // A value bar's range, step (0: any), value, how it reads, and what a
        // release or a turn does with it.
        double low = 0, high = 1, step = 0, at = 0;
        std::function<std::string(double)> shown;
        std::function<void(double)> set;
    };
    // How a bar on a page's list looks: its value on the right (in an ink;
    // 0 is white), a line under its name (0 is grey), filled when lit (0 is
    // green), its name's case kept, still (shown, not tapped), its edge.
    struct BarLook {
        std::string value;
        std::uint32_t value_ink = 0;
        std::string caption;
        std::uint32_t caption_ink = 0;
        bool lit = false, keep_case = false, still = false;
        std::uint32_t fill = 0, edge = 0;
    };
    // A header pill: what it says and does; the page's main action is white.
    struct Pill { std::string name; Action action; InstanceId id = 0; std::string symbol; int number = 0; bool primary = false, disabled = false; };
    std::vector<Pill> header_pills() const;
    // The page's list: compact bars two to a row, right of the rail, as on a
    // module page. Bars, choices, value bars, headings and notes go in it.
    lv_obj_t* page_list(lv_obj_t* parent);
    lv_obj_t* list_bar(lv_obj_t* list, const std::string& name, const BarLook&, Action = Action::Navigate, InstanceId = 0, std::string symbol = {}, int number = 0);
    // Every choice in one bar, the chosen one lit; a tap takes one (its index is the number).
    // As wide as a bar, or `width`; lit in `fill` (0 is green).
    void choice_bar(lv_obj_t* list, const std::vector<std::string>& choices, int chosen, Action, InstanceId = 0, std::string symbol = {},
                    int width = 0, std::uint32_t fill = 0);
    // Dragged sideways like a module's bar, or turned by encoder 2.
    lv_obj_t* value_bar(lv_obj_t* list, const std::string& name, double low, double high, double step, double value,
                        std::function<std::string(double)> shown, std::function<void(double)> set);
    static void event(lv_event_t*);
    void handle(Binding&, lv_event_t*);
    RequestId submit(Command);
    // `scene`: an edit on the touch screen, written into the current scene.
    void stream_parameter(InstanceId, const std::string& symbol, double value, bool scene = false);
    void send_stream();
    void follow_poly_voices(bool replaced);
    void navigate(Page);
    void rebuild();
    void patch_view(lv_obj_t*);
    void browser(lv_obj_t* header, lv_obj_t* content, lv_obj_t* foot);
    void browser_pick(const std::string& uri, bool reveal);
    void add_module(const std::string& uri);
    void parameters(lv_obj_t*);
    // One parameter as a bar: its name, its value, and a fill as far as the
    // value goes. Drag it sideways to change it, tap it to flip or step it.
    // Smaller bars leave room to scroll; a few-choice mode can show every
    // choice as a segment that a drag or a tap picks.
    void parameter_bar(lv_obj_t*, const Module&, const Parameter&, int width = 372, int height = 48, bool segmented = false);
    // The module page's list of bars, right of the rail.
    lv_obj_t* bar_list(lv_obj_t* parent, const Module&, int top);
    // A list of compact bars in two columns, right of the rail; it scrolls.
    lv_obj_t* scroll_list(lv_obj_t* parent, int top);
    // The rail left of a long list: a line and a ball where the list is
    // scrolled to. Dragging along it scrolls the list.
    void scroll_rail(lv_obj_t* parent, lv_obj_t* list, int top, int height, std::uint32_t ink);
    // A page of its own on two tabs (Aura, Discipline): the main controls and
    // stomps, or everything else. Returns the list it scrolls in.
    lv_obj_t* pedal_view(lv_obj_t*, const Module&);
    void file_bar(lv_obj_t*, const Module&);
    // Move the encoder highlight on the module page, keeping it in view.
    void focus_parameter(std::size_t);
    // Turn the highlighted (or a pinned) parameter by encoder detents.
    void turn_parameter(InstanceId, const std::string& symbol, const Gesture&, const Assignment* pin);
    // A push on the highlighted bar: flip a switch, step a mode, choose a file,
    // or start and stop turning its value with the select encoder.
    void activate_parameter();
    // Where Back leads from the page in view.
    Page back_page() const;
    // Open a module's page; the same module keeps its highlight and scroll.
    void open_module(InstanceId);
    void file_picker(lv_obj_t*);
    void tempo(lv_obj_t*);
    void midi_page(lv_obj_t*);
    void midi_learn(lv_obj_t*);
    // The control changes this patch sends as it loads.
    void patch_midi(lv_obj_t*);
    // Change the MIDI settings and write them, for peepo-midi and peepo-route to read.
    void change_midi(const std::function<void(midi::Settings&)>&);
    // A held bar: its own page, with MIDI learn and, in a patch with scenes,
    // whether it changes per scene.
    void open_learn(InstanceId, const std::string& symbol);
    // The patch's scenes by bank, and the parameters marked per scene.
    void scenes_page(lv_obj_t*);
    // In scene mode, what stomps A to C play and D and E do.
    std::string scene_hint() const;
    void tap_tempo();
    void set_tempo(double bpm);
    void tuner_meter(lv_obj_t*);
    void update_tuner();
    void euclidean_view(lv_obj_t*, const Module&);
    void update_euclidean();
    void looper_view(lv_obj_t*, const Module&);
    void update_looper();
    void release_looper();
    void patches(lv_obj_t*);
    void settings(lv_obj_t*);
    void diagnostics(lv_obj_t*);
    void power(lv_obj_t*);
    void halting(lv_obj_t*);
    void power_action(bool restart);
    void update_system();
    void module_actions(lv_obj_t*);
    void name_patch(lv_obj_t*);
    void confirm_replace(lv_obj_t*);
    void cv_connect(lv_obj_t*);
    void connect_modules(InstanceId from, InstanceId to);
    void commit_name();
    void replace_patch(Command, Page origin);
    void confirm_action(lv_obj_t*);
    void stored_patches(lv_obj_t*);
    // Load, startup or delete one saved patch.
    void patch_actions(lv_obj_t*);
    void refresh_stored();
    void save_patch(bool as_new);
    void start_save(const std::string& id);
    void load_stored(const std::string& id, bool allow_missing, bool guarded);
    void ask(std::string title, std::string heading, std::string body, std::string button, std::function<void()> act);
    std::string status_text() const;
    std::string engine_banner() const;
    void gesture(const Gesture&);
    void encoder(const Gesture&);
    void stomp(const Gesture&);
    // A stomp pressed while a module's actions are open: it switches that
    // module's bypass from then on, beside any others, or no longer does.
    void quick_assign(const std::string& control);
    // A stomp pressed while binding on the Loo-pi page: it sends the picked
    // command to the picked target from then on, or no longer does.
    void looper_bind(const std::string& control);
    // Give Footswitch modules listening to a stomp its press or release;
    // false when none listens. What changed is added to `said`.
    bool drive_footswitches(const std::string& control, bool pressed, std::vector<std::string>& said);
    // Whether a control's assignments all lead to modules that are gone.
    bool orphaned(const std::string& control) const;
    void step_patch(int direction);
    const Assignment* pinned(const std::string& control) const;
    // Encoder 1 on the patch: the module highlighted, in signal order.
    void focus_module(int steps);
    void controls(lv_obj_t*);
    void assign_picker(lv_obj_t*);
    void assignment_detail(lv_obj_t*);
    std::string assignment_text(const std::string& control) const;
    void update_viewport();
    void update_cables();
    void update_metrics();
    void update_network();
    void indicate_targets(bool);
    // Where a new card of this height goes, and panning to show one.
    std::optional<std::pair<int, int>> vacant_slot(int height = module_height) const;
    void reveal(int x, int y, int height = module_height);
    void bind(lv_obj_t*, Action, InstanceId = 0, std::string = {}, int = 0, lv_obj_t* = nullptr);
    lv_obj_t* button(lv_obj_t*, const std::string&, int, int, int, Action, InstanceId = 0, std::string = {}, int = 0);
    void message(std::string);
    Engine& engine_;
    Graph graph_;
    Graph saved_;
    bool has_saved_ = false;
    Page page_ = Page::Patch;
    InstanceId selected_ = 0;
    bool rebuild_ = true;
    // Engine news nobody here asked for, and when the page was last built.
    bool news_ = false;
    unsigned long long rebuilt_ms_ = 0;
    std::string message_ = "Drag space to pan. Hold a module for actions.";
    enum class PendingKind { Edit, Rename, Replace, Add, Save, SaveAs };
    std::map<RequestId, PendingKind> pending_;
    // Module names of adds in flight, to say which plugin failed.
    std::map<RequestId, std::string> adding_;
    // MIDI to CV poly modules and their cabled voices when last seen.
    std::map<InstanceId, int> poly_voices_;
    // Saved patches on disk; null when the application has no store.
    PatchStore* store_ = nullptr;
    std::vector<StoredPatch> stored_;
    // The saved patch whose actions are open, and how far the list was scrolled.
    std::string patch_selected_;
    // The page's list, the page it was built for, and how far each page's list
    // was scrolled; opening a page from the patch starts them at the top.
    lv_obj_t* page_list_ = nullptr;
    Page page_list_page_ = Page::Patch;
    std::map<Page, int> list_scroll_;
    // A save the engine is writing, published once it answers.
    std::optional<PatchStore::Staging> staging_;
    std::string save_as_id_, load_note_;
    bool saving_as_ = false;
    // Load the startup patch once the engine is ready, if it plays no stored patch.
    bool startup_pending_ = false;
    // The connection: whether it was ever ready, and the last reason given for its state.
    bool ever_ready_ = false;
    EngineState last_state_ = EngineState::Ready;
    std::string engine_note_;
    struct Confirmation { std::string title, heading, body, button; std::function<void()> act; Page back = Page::Patches; };
    Confirmation confirmation_;
    std::function<std::string()> power_off_;
    std::function<std::string()> restart_;
    std::function<Network()> network_;
    std::function<System()> system_;
    FileStore* files_ = nullptr;
    const midi::Link* midi_ = nullptr;
    midi::Settings midi_settings_;
    // What peepo-midi last said it hears, read every second.
    midi::Status midi_status_;
    unsigned midi_age_ = 0;
    // MIDI learn: the control, the request that starts it, and whether the
    // engine waits for a MIDI message now.
    InstanceId learn_id_ = 0;
    std::string learn_symbol_;
    RequestId learn_request_ = 0;
    bool learn_waiting_ = false;
    TapTempo tapper_;
    unsigned long long clock_ms_ = 0;
    // When the last message was given: the Loo-pi page's status line waits
    // a few seconds before replacing it.
    unsigned long long message_ms_ = 0;
    // A tempo on its way to the engine, so steps build on it rather than on the last answer.
    std::optional<double> tempo_sent_;
    System reading_;
    // Whether the supply was ever too low while the interface ran.
    bool low_voltage_seen_ = false;
    unsigned system_age_ = 0;
    // What the halting page says: shutting down or restarting.
    bool restarting_ = false;
    // Settings shows the network, read again every couple of seconds.
    lv_obj_t* network_value_ = nullptr;
    unsigned network_age_ = 0;
    // Physical controls: gestures, and the bars the encoders move through on a module page.
    ControlLayout layout_;
    ControlRecognizer recognizer_;
    struct Bar { InstanceId id; std::string symbol; lv_obj_t* widget; bool file = false; };
    std::vector<Bar> bars_;
    // The highlighted bar, and whether encoder 1 turns its value rather than moving on.
    std::size_t parameter_focus_ = 0;
    bool adjusting_ = false;
    // A parameter to highlight once the module page is built, as when a pinned encoder shows its target.
    std::string focus_symbol_;
    // The module page's list and where it was scrolled, kept across rebuilds.
    lv_obj_t* parameter_list_ = nullptr;
    int parameter_scroll_ = 0;
    // A bar being dragged: the normalized value it started from, and whether it moved.
    double drag_origin_ = 0;
    // A press that went up or down first is a scroll: the bar leaves it alone.
    bool bar_moving_ = false, bar_dragged_ = false, bar_scrolling_ = false;
    // The module highlighted on the patch by encoder 1.
    InstanceId patch_focus_ = 0;
    // The control last used, highlighted on the Controls page; the one being assigned or edited.
    std::string active_control_, editing_control_;
    InstanceId picking_module_ = 0;
    // The module browser: the group filter (empty for all), the picked module,
    // and where the cards were scrolled.
    std::string browser_group_, browser_pick_;
    // A finer category inside the open group; empty for the whole group.
    std::string browser_category_;
    int browser_scroll_ = 0;
    lv_obj_t *browser_list_ = nullptr, *browser_add_ = nullptr;
    // Every variant a card offers maps to it; a family's pills, by variant.
    std::map<std::string, lv_obj_t*> browser_cards_, browser_pills_;
    // The variant each family's card last picked, kept between visits.
    std::map<std::string, std::string> browser_variant_;
    // What the encoders step through: groups shown, and usable modules in card order.
    std::vector<std::string> browser_groups_, browser_order_;
    bool renaming_ = false;
    // The name being typed is the current scene's.
    bool naming_scene_ = false;
    std::string draft_name_;
    // On the Scenes page, a tap on a scene picks where Copy to copies, or
    // which scene Delete deletes.
    enum class ScenePick { None, CopyTo, Delete };
    ScenePick scene_pick_ = ScenePick::None;
    // What to say once the engine accepts a request, in place of the plain acknowledgement.
    std::map<RequestId, std::function<std::string()>> accepted_notes_;
    std::optional<Command> replacement_;
    Page confirm_origin_ = Page::Patches;
    lv_obj_t *screen_ = nullptr, *canvas_ = nullptr, *footer_ = nullptr;
    lv_obj_t *tempo_value_ = nullptr, *cpu_value_ = nullptr;
    lv_obj_t *tuner_note_ = nullptr, *tuner_detail_ = nullptr, *tuner_status_ = nullptr, *tuner_needle_ = nullptr;
    int looper_section_ = 0, looper_target_ = 0, looper_selected_ = 1;
    // The tab in view on a page with two: 0 the main controls, 1 the rest.
    int pedal_page_ = 0;
    struct LooperPress { InstanceId id; int command, target; };
    std::map<lv_obj_t*, LooperPress> looper_pressed_;
    // Binding a stomp: on while BIND is lit; the command tapped, or -1.
    bool looper_binding_ = false;
    int looper_bind_command_ = -1, looper_bind_target_ = 0;
    // Stomps holding a Loo-pi command down, so the release reaches the loop
    // that got the press even if the selection changed in between.
    std::map<std::string, LooperPress> looper_stomps_;
    struct LooperCard { lv_obj_t *card = nullptr, *state = nullptr, *time = nullptr, *cycle = nullptr, *bar = nullptr; };
    std::array<LooperCard, looper_loops> looper_cards_{};
    std::vector<lv_obj_t*> looper_commands_;
    lv_obj_t* euclidean_status_ = nullptr;
    std::array<lv_obj_t*, euclidean_tracks> euclidean_rings_{};
    std::array<lv_obj_t*, euclidean_tracks> euclidean_halos_{};
    std::array<std::array<lv_obj_t*, euclidean_max_steps>, euclidean_tracks> euclidean_dots_{};
    std::array<lv_obj_t*, euclidean_tracks> euclidean_step_labels_{};
    std::array<std::array<lv_obj_t*, 3>, euclidean_tracks> euclidean_tiles_{};
    lv_indev_t* pointer_input_ = nullptr;
    lv_indev_t* keypad_input_ = nullptr;
    std::deque<Binding> bindings_;
    std::map<InstanceId, lv_obj_t*> module_widgets_;
    std::multimap<std::pair<InstanceId, std::string>, lv_obj_t*> input_jacks_;
    std::vector<lv_obj_t*> dots_;
    std::optional<Connection> selected_cable_;
    // One drawn line per channel of a logical cable.
    struct Cable { lv_obj_t* line; Connection connection; std::array<lv_point_precise_t, 20> points{}; RoutingPlan routing; unsigned out_channel = 0, in_channel = 0; bool cv = false; };
    std::deque<Cable> cables_;
    lv_obj_t* preview_ = nullptr;
    std::array<lv_point_precise_t, 20> preview_points_{};
    InstanceId cable_source_ = 0;
    std::string cable_port_;
    // A continuous gesture in progress: one request in flight, newest value wins.
    struct Stream { InstanceId id; std::string symbol; double latest = 0; std::optional<double> sent; RequestId in_flight = 0; bool scene = false; };
    std::optional<Stream> stream_;
    lv_point_t press_{};
    int start_x_ = 0, start_y_ = 0;
    int pan_x_ = 0, pan_y_ = 0;
    bool dragging_ = false, panning_ = false, long_pressed_ = false;
    InstanceId pressed_module_ = 0, held_module_ = 0, second_module_ = 0;
    lv_indev_t *pressed_indev_ = nullptr, *held_indev_ = nullptr, *second_indev_ = nullptr;
    lv_point_t second_press_{};
    std::uint32_t second_press_ms_ = 0;
    bool hold_connected_ = false;
    // Control (CV) cables and hold-to-connect port choices: the port is chosen from a list when several exist.
    bool cable_cv_ = false, cv_only_ = false;
    InstanceId cv_from_ = 0, cv_to_ = 0;
    std::string cv_output_;
    std::map<InstanceId, lv_obj_t*> cv_inputs_;
};
}
