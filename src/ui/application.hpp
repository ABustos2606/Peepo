// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "controls/controls.hpp"
#include "engine/engine.hpp"
#include "storage/file_store.hpp"
#include "storage/patch_store.hpp"
#include "lvgl.h"
#include <array>
#include <deque>
#include <functional>
#include <map>

namespace peepo {
enum class Page { Patch, Browser, Parameters, Patches, Settings, ModuleActions, NamePatch, ConfirmReplace, CvConnect, ConfirmAction,
                  Controls, AssignPicker, AssignmentDetail, Diagnostics, Power, Halting, FilePicker, Tempo };
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
private:
    enum class Action { Navigate, Add, Module, Port, SelectCable, DeleteCable, ClearCable, Pan, Home,
                        OpenActions, Edit, Duplicate, Parameter, Bypass, Remove, SaveSession, LoadSession, State, Reject,
                        NameEntry, NameInput, Keyboard, CommitName, Confirm, CancelReplace, ParameterPage, BrowserGroup, BrowserPick, MissingFixture,
                        ParallelDemo, CvChoice, CvPage,
                        SavePatch, LoadStored, DeleteStored, StartupStored, PatchPage, ConfirmAction, CancelAction,
                        AssignControl, EditControl, ClearControl, PickModule, PickTarget, PickerPage, InvertControl, SwitchModeChoice, RangeEnd, PowerOff,
                        Restart, BrowserCategory, Revert, ChooseFile, PickFile, FilePage, TempoStep, TempoSlider, Tap };
    struct Binding {
        Application* app;
        Action action;
        InstanceId id = 0;
        std::string symbol;
        int number = 0;
        lv_obj_t* value_label = nullptr;
    };
    static void event(lv_event_t*);
    void handle(Binding&, lv_event_t*);
    RequestId submit(Command);
    void stream_parameter(InstanceId, const std::string& symbol, double value);
    void send_stream();
    void navigate(Page);
    void rebuild();
    void patch_view(lv_obj_t*);
    void browser(lv_obj_t* header, lv_obj_t* content, lv_obj_t* foot);
    void browser_pick(const std::string& uri, bool reveal);
    void add_module(const std::string& uri);
    void parameters(lv_obj_t*);
    // One parameter as a bar: its name, its value, and a fill as far as the
    // value goes. Drag it sideways to change it, tap it to flip or step it.
    void parameter_bar(lv_obj_t*, const Module&, const Parameter&);
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
    void tap_tempo();
    void set_tempo(double bpm);
    void tuner_meter(lv_obj_t*);
    void update_tuner();
    void euclidean_view(lv_obj_t*, const Module&);
    void update_euclidean();
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
    std::optional<std::pair<int, int>> vacant_slot() const;
    void reveal(int x, int y);
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
    std::string message_ = "Drag space to pan. Hold a module for actions. Drag output to input.";
    enum class PendingKind { Edit, Rename, Replace, Add, Save, SaveAs };
    std::map<RequestId, PendingKind> pending_;
    // Module names of adds in flight, to say which plugin failed.
    std::map<RequestId, std::string> adding_;
    // Saved patches on disk; null when the application has no store.
    PatchStore* store_ = nullptr;
    std::vector<StoredPatch> stored_;
    std::size_t patch_page_ = 0;
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
    TapTempo tapper_;
    unsigned long long clock_ms_ = 0;
    // A tempo on its way to the engine, so steps build on it rather than on the last answer.
    std::optional<double> tempo_sent_;
    std::size_t file_page_ = 0;
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
    bool bar_moving_ = false, bar_dragged_ = false;
    // The module highlighted on the patch by encoder 1.
    InstanceId patch_focus_ = 0;
    // The control last used, highlighted on the Controls page; the one being assigned or edited.
    std::string active_control_, editing_control_;
    InstanceId picking_module_ = 0;
    std::size_t picker_page_ = 0;
    // The module browser: the group filter (empty for all), the picked module,
    // and where the cards were scrolled.
    std::string browser_group_, browser_pick_;
    // A finer category inside the open group; empty for the whole group.
    std::string browser_category_;
    int browser_scroll_ = 0;
    lv_obj_t *browser_list_ = nullptr, *browser_add_ = nullptr;
    std::map<std::string, lv_obj_t*> browser_cards_;
    // What the encoders step through: groups shown, and usable modules in card order.
    std::vector<std::string> browser_groups_, browser_order_;
    bool renaming_ = false;
    std::string draft_name_;
    std::optional<Command> replacement_;
    Page confirm_origin_ = Page::Patches;
    lv_obj_t *screen_ = nullptr, *canvas_ = nullptr, *footer_ = nullptr;
    lv_obj_t *tempo_value_ = nullptr, *cpu_value_ = nullptr;
    lv_obj_t *tuner_note_ = nullptr, *tuner_detail_ = nullptr, *tuner_status_ = nullptr, *tuner_needle_ = nullptr;
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
    struct Stream { InstanceId id; std::string symbol; double latest = 0; std::optional<double> sent; RequestId in_flight = 0; };
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
    std::size_t cv_page_ = 0;
    std::map<InstanceId, lv_obj_t*> cv_inputs_;
};
}
