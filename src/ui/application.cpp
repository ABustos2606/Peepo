// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "ui/application.hpp"
#include "core/tuning.hpp"
#include "ui/fonts/fonts.h"
// v9.5.0 exposes the hit-test payload through this header. Keep this usage
// isolated and recheck it when upgrading the pinned LVGL dependency.
#include "src/core/lv_obj_event_private.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace peepo {
namespace {
// Black, with colour only where it means something: a module's group, a
// state, the one action that matters on a page.
constexpr std::uint32_t background = 0x000000, surface = 0x0f0f0f, border = 0x262626, rim = 0x3f3f3f, pressed_fill = 0x1c1c1c;
constexpr std::uint32_t text = 0xffffff, muted = 0x9ca3af, faint = 0x737373, accent = 0x4ade80, warning = 0xfb923c, cable_ink = 0x7dd3fc;
lv_color_t color(std::uint32_t c) { return lv_color_hex(c); }
void sample_cable(lv_obj_t* line, std::array<lv_point_precise_t, 20>& points, double ax, double ay, double bx, double by, bool cv) {
    const double k = std::max(40.0, std::abs(cv ? by - ay : bx - ax) * 0.5);
    const double c1x = cv ? ax : ax + k, c1y = cv ? ay - k : ay;
    const double c2x = cv ? bx : bx - k, c2y = cv ? by + k : by;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(points.size() - 1);
        const double u = 1.0 - t;
        const double x = u * u * u * ax + 3.0 * u * u * t * c1x + 3.0 * u * t * t * c2x + t * t * t * bx;
        const double y = u * u * u * ay + 3.0 * u * u * t * c1y + 3.0 * u * t * t * c2y + t * t * t * by;
        points[i] = {static_cast<lv_value_precise_t>(std::round(x)), static_cast<lv_value_precise_t>(std::round(y))};
    }
    lv_line_set_points(line, points.data(), static_cast<std::uint32_t>(points.size()));
}
std::string upper(std::string value) {
    for (auto& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}
// Modules come in a few groups, merged from the catalogue's categories the
// way the Beebo firmware merges them. The group gives a module its colour
// everywhere: browser pill and tag, square on the canvas, parameter tiles. A
// category no group names falls into the last.
struct ModuleGroup { const char* name; std::uint32_t fill, bright; std::vector<std::string> categories; };
const std::vector<ModuleGroup>& module_groups() {
    static const std::vector<ModuleGroup> groups = {
        {"GAIN", 0xa8452f, 0xff8a65, {"Drive", "Amp", "Cabinet", "Dynamics"}},
        {"MOD", 0x9b3f86, 0xf472d0, {"Modulation", "Filter"}},
        {"TIME", 0x5b3fa8, 0xa78bfa, {"Delay", "Reverb"}},
        {"TEXTURE", 0x1f6f8f, 0x38bdf8, {"Texture", "Resonator", "Pitch", "Sound source"}},
        {"CV + UTIL", 0x6b6a24, 0xe8d44d, {"Control", "Utility"}}};
    return groups;
}
std::size_t module_group(const std::string& category) {
    const auto& groups = module_groups();
    for (std::size_t i = 0; i < groups.size(); ++i)
        if (std::find(groups[i].categories.begin(), groups[i].categories.end(), category) != groups[i].categories.end()) return i;
    return groups.size() - 1;
}
// Control cables and sockets share the CV group's yellow.
const std::uint32_t control_ink = module_groups().back().bright;
lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h, std::uint32_t fill = surface) {
    auto* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color(fill), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 12, 0);
    lv_obj_set_style_border_width(o, 1, 0); lv_obj_set_style_border_color(o, color(border), 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
// A bare full-width band: header, content or footer.
lv_obj_t* band(lv_obj_t* parent, int y, int h) {
    auto* o = panel(parent, 0, y, 800, h, background);
    lv_obj_set_style_radius(o, 0, 0); lv_obj_set_style_border_width(o, 0, 0);
    return o;
}
lv_obj_t* label(lv_obj_t* parent, const std::string& value, int x, int y, int w = 0,
                std::uint32_t ink = text, const lv_font_t* font = &lv_font_montserrat_18) {
    auto* l = lv_label_create(parent); lv_label_set_text(l, value.c_str());
    lv_obj_set_pos(l, x, y); if (w) lv_obj_set_width(l, w);
    lv_obj_set_style_text_font(l, font, 0); lv_obj_set_style_text_color(l, color(ink), 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}
int snap(int value) { return static_cast<int>(std::round(value / 24.0)) * 24; }
std::string metric_text(std::optional<double> reading, bool cpu) {
    if (!reading || !std::isfinite(*reading) || (cpu ? *reading < 0 || *reading > 100 : *reading <= 0)) return "--";
    std::ostringstream value;
    value << std::fixed << std::setprecision(std::abs(*reading - std::round(*reading)) < 0.05 ? 0 : 1) << *reading;
    if (cpu) value << '%';
    return value.str();
}
enum class ModuleLayout { Generic, Tuner, Euclidean };
struct EuclideanTrackStyle { std::uint32_t bright, fill, ring_dim; int radius; };
constexpr std::array<EuclideanTrackStyle, euclidean_tracks> euclidean_styles = {{
    {0x38bdf8, 0x143a52, 0x1c3d52, 124},
    {0x4ade80, 0x143d28, 0x1a3d28, 96},
    {0xe8d44d, 0x423c14, 0x3d3818, 68},
    {0xf472d0, 0x421838, 0x3d1c35, 40},
}};
bool has_euclidean_controls(const Module& m) {
    for (int i = 1; i <= 4; ++i) {
        const auto n = std::to_string(i);
        for (const auto* prefix : {"steps", "beats", "shift", "is_enabled"})
            if (std::none_of(m.parameters.begin(), m.parameters.end(), [&](const Parameter& p) { return p.symbol == std::string(prefix) + n; }))
                return false;
    }
    return true;
}
bool is_euclidean_toggle(const Parameter& p) {
    return p.kind == ParameterKind::Toggle || (p.symbol.starts_with("is_enabled") && p.minimum == 0 && p.maximum == 1);
}
ModuleLayout module_layout(const Module& m) {
    // Match by stable URI and parameter schema, never by the editable name.
    // An unexpected/extended schema must keep every control in the generic view.
    const auto* d = definition(m.uri);
    if (!d || m.parameters.size() != d->parameters.size()) return ModuleLayout::Generic;
    for (const auto& expected : d->parameters) {
        const auto it = std::find_if(m.parameters.begin(), m.parameters.end(), [&](const Parameter& p) { return p.symbol == expected.symbol; });
        if (it == m.parameters.end() || it->kind != expected.kind || it->choices != expected.choices || !validate_parameter(*it).empty())
            return ModuleLayout::Generic;
    }
    if (tuner_uri(m.uri) && std::any_of(m.parameters.begin(), m.parameters.end(), [](const Parameter& p) { return p.symbol == "tuning"; })) return ModuleLayout::Tuner;
    if (euclidean_uri(m.uri) && has_euclidean_controls(m)) return ModuleLayout::Euclidean;
    return ModuleLayout::Generic;
}
// Vertical placement of one connector on its edge of the square. Every audio
// channel has its own connector, so a stereo port shows two (left above
// right). A single connector stays centred; several share the edge in equal
// touch bands.
std::vector<const Port*> cv_ports(const Module& m, Direction direction) {
    std::vector<const Port*> found;
    for (const auto& p : module_ports(m)) if (p.type == PortType::Cv && p.direction == direction) found.push_back(&p);
    return found;
}
std::vector<const Port*> connectable_ports(const Module& from, const Module& to, const std::string& output_symbol, bool cv_only) {
    std::vector<const Port*> found;
    if (output_symbol.empty()) {
        for (const auto& out : module_ports(from)) {
            if (out.direction != Direction::Output) continue;
            if (out.type != PortType::Cv && (cv_only || out.type != PortType::Audio)) continue;
            const bool has_target = std::any_of(module_ports(to).begin(), module_ports(to).end(), [&](const Port& in) {
                return in.direction == Direction::Input && in.type == out.type;
            });
            if (has_target) found.push_back(&out);
        }
    } else {
        const auto& from_ports = module_ports(from);
        const auto out = std::find_if(from_ports.begin(), from_ports.end(), [&](const Port& p) {
            return p.direction == Direction::Output && p.symbol == output_symbol;
        });
        if (out == from_ports.end()) return found;
        for (const auto& in : module_ports(to))
            if (in.direction == Direction::Input && in.type == out->type) found.push_back(&in);
    }
    return found;
}
struct Jack { int centre, height; };
std::optional<Jack> jack(const Module& m, const std::string& symbol, Direction direction, unsigned channel = 0) {
    int count = 0, index = -1;
    for (const auto& p : module_ports(m)) {
        if (p.type != PortType::Audio || p.direction != direction) continue;
        if (p.symbol == symbol && channel < p.channels) index = count + static_cast<int>(channel);
        count += static_cast<int>(p.channels);
    }
    if (index < 0) return {};
    return Jack{(2 * index + 1) * module_height / (2 * count), std::min(44, module_height / count)};
}
// A module's colour: its group's, or the warning colour when it is missing.
std::uint32_t module_ink(const Module& m) {
    const auto* d = definition(m.uri);
    if (!module_available(m) || !d) return warning;
    return m.fixed ? muted : module_groups()[module_group(d->category)].bright;
}
std::uint32_t bar_ink(const Module& m, const std::string& symbol) {
    if (euclidean_uri(m.uri) && !symbol.empty() && symbol.back() >= '1' && symbol.back() <= '4')
        return euclidean_styles[static_cast<std::size_t>(symbol.back() - '1')].bright;
    return module_ink(m);
}
const Parameter& parameter(const Module& m, const std::string& symbol) {
    return *std::find_if(m.parameters.begin(), m.parameters.end(), [&](const Parameter& p) { return p.symbol == symbol; });
}
// A module's darker group colour, which a parameter bar fills with.
std::uint32_t module_fill(const Module& m) {
    const auto* d = definition(m.uri);
    if (!module_available(m) || !d || m.fixed) return 0x333333;
    return module_groups()[module_group(d->category)].fill;
}
// The module page in order: the overlay's named groups first, each under its
// name, then whatever they leave out ("More" when the module has groups at all).
struct ParameterSection { std::string name; std::vector<const Parameter*> parameters; };
std::vector<ParameterSection> parameter_sections(const Module& m) {
    std::vector<ParameterSection> sections;
    std::vector<std::string> placed;
    if (const auto* d = definition(m.uri))
        for (const auto& g : d->groups) {
            ParameterSection section{g.name, {}};
            for (const auto& s : g.symbols)
                for (const auto& p : m.parameters)
                    if (p.symbol == s && std::find(placed.begin(), placed.end(), s) == placed.end()) { placed.push_back(s); section.parameters.push_back(&p); }
            if (!section.parameters.empty()) sections.push_back(std::move(section));
        }
    ParameterSection rest{sections.empty() ? "" : "More", {}};
    for (const auto& p : m.parameters)
        if (std::find(placed.begin(), placed.end(), p.symbol) == placed.end()) rest.parameters.push_back(&p);
    if (!rest.parameters.empty()) sections.push_back(std::move(rest));
    return sections;
}
// Parameter bars: two columns of six in view, as on Beebo's module pages.
constexpr int bar_width = 372, bar_height = 48;
int bar_span(lv_obj_t* bar) {
    const int w = lv_obj_get_width(bar);
    return w > 0 ? w : bar_width;
}
// How far a bar is filled, and what it says.
double bar_level(const Parameter& p) {
    if (is_euclidean_toggle(p)) return p.value >= 0.5 ? 1 : 0;
    if (p.kind == ParameterKind::Enumeration) return p.choices.empty() ? 0 : std::clamp((std::round(p.value) + 1) / static_cast<double>(p.choices.size()), 0.0, 1.0);
    return normalized_value(p);
}
std::string bar_text(const Parameter& p) {
    if (is_euclidean_toggle(p)) return p.value >= 0.5 ? "ON" : "OFF";
    return p.kind == ParameterKind::Enumeration ? upper(format_value(p)) : format_value(p);
}
// A bar's children: the fill, the name, the value.
void paint_bar(lv_obj_t* bar, const Parameter& p) {
    lv_obj_set_width(lv_obj_get_child(bar, 0), static_cast<int>(std::round(bar_level(p) * bar_span(bar))));
    const auto shown = bar_text(p);
    auto* value = lv_obj_get_child(bar, 2);
    if (shown != lv_label_get_text(value)) lv_label_set_text(value, shown.c_str());
}
// The encoder highlight: white, or the module's colour while encoder 1 turns the value.
void highlight(lv_obj_t* bar, bool focused, bool adjusting, std::uint32_t ink) {
    lv_obj_set_style_border_width(bar, focused ? (adjusting ? 3 : 2) : 1, 0);
    lv_obj_set_style_border_color(bar, color(!focused ? border : adjusting ? ink : text), 0);
}
// The next choice of a mode, wrapping; the other state of a switch.
double stepped_choice(const Parameter& p) {
    if (is_euclidean_toggle(p)) return p.value >= 0.5 ? 0 : 1;
    const auto count = static_cast<double>(std::max<std::size_t>(1, p.choices.size()));
    return std::fmod(std::round(p.value) + 1, count);
}
// Peepo's Footswitch module: a stomp as a control signal. Its Stomp control
// (0 for the first) says which stomp it hears; the interface sets Pressed.
constexpr const char* footswitch_uri = "urn:peepo:lv2:footswitch";
std::string joined_names(const std::vector<std::string>& names) {
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", " : "") + names[i];
    return out;
}
// The stomps that act on a module, by number: assigned to its bypass or a
// parameter, or heard by it when it is a Footswitch module.
std::vector<int> stomps_on(const Graph& g, const Module& m) {
    std::vector<int> numbers;
    const auto add = [&](int n) { if (std::find(numbers.begin(), numbers.end(), n) == numbers.end()) numbers.push_back(n); };
    for (const auto& a : g.assignments)
        if (a.module == m.id && is_stomp(a.control) && assignment_target(g, a)) add(std::stoi(a.control.substr(5)));
    if (m.uri == footswitch_uri)
        for (const auto& p : m.parameters) if (p.symbol == "stomp") add(static_cast<int>(p.value) + 1);
    std::sort(numbers.begin(), numbers.end());
    return numbers;
}
}
Application::Application(Engine& engine, PatchStore* store) : engine_(engine), graph_(engine.snapshot()), store_(store && store->usable() ? store : nullptr) {
    last_state_ = engine_.state();
    ever_ready_ = last_state_ == EngineState::Ready;
    refresh_stored();
    startup_pending_ = store_ && !store_->startup().empty();
    rebuild();
}
void Application::refresh_stored() {
    if (store_) stored_ = store_->list();
}
std::string Application::status_text() const {
    switch (engine_.state()) {
    case EngineState::Ready: return engine_.capabilities().simulation ? "SIMULATED" : "READY";
    case EngineState::Connecting: return ever_ready_ ? "RECONNECTING" : "STARTING";
    case EngineState::Disconnected: return "OFFLINE";
    default: return "ENGINE ERROR";
    }
}
std::string Application::engine_banner() const {
    const auto reason = engine_note_.empty() ? std::string() : " (" + engine_note_ + ")";
    switch (engine_.state()) {
    case EngineState::Ready: return {};
    case EngineState::Connecting:
        return ever_ready_ ? "Engine connection lost" + reason + ". Retrying; edits are paused and the last graph is shown."
                           : "Connecting to the engine" + reason + "...";
    case EngineState::Disconnected: return "Engine disconnected. Settings > Connect reconnects; the last graph is shown.";
    default: return "Engine error" + reason + ". Edits are paused.";
    }
}
Application::~Application() { engine_.watch_tuner(std::nullopt); engine_.watch_euclidean(std::nullopt); if (screen_) lv_obj_delete(screen_); }
void Application::message(std::string value) {
    message_ = std::move(value);
    if (footer_) lv_label_set_text(footer_, message_.c_str());
}
void Application::navigate(Page page) {
    // The browser keeps its filter between visits, so a chain of one kind is
    // quick to build, but always opens at the top with nothing picked.
    if (page == Page::Browser && page_ != Page::Browser) { browser_pick_.clear(); browser_scroll_ = 0; }
    // Another program, or a damaged disk, may have changed what is stored.
    if (page == Page::Patches && page_ != Page::Patches) refresh_stored();
    page_ = page; rebuild_ = true;
}
RequestId Application::submit(Command command) {
    if (engine_.state() != EngineState::Ready) {
        message(engine_.state() == EngineState::Connecting && ever_ready_ ? "Engine is reconnecting; edits are paused." : "Engine unavailable. Reconnect in Settings.");
        rebuild_ = true; return 0;
    }
    auto kind = std::holds_alternative<NewPatch>(command) || std::holds_alternative<LoadSession>(command) || std::holds_alternative<LoadGraph>(command) ||
        std::holds_alternative<LoadPatch>(command) ? PendingKind::Replace :
        std::holds_alternative<RenamePatch>(command) ? PendingKind::Rename : PendingKind::Edit;
    const auto id = engine_.submit(std::move(command));
    pending_[id] = kind;
    message(engine_.capabilities().simulation ? "Waiting for simulated engine..." : "Waiting for engine...");
    return id;
}
void Application::stream_parameter(InstanceId id, const std::string& symbol, double value) {
    // A moving control must be heard while it moves. Send as fast as the
    // engine acknowledges, and never queue more than the newest value.
    if (stream_ && (stream_->id != id || stream_->symbol != symbol)) {
        if (stream_->sent != stream_->latest) submit(SetParameter{stream_->id, stream_->symbol, stream_->latest});
        stream_.reset();
    }
    if (!stream_) stream_ = Stream{id, symbol};
    stream_->latest = value;
    if (!stream_->in_flight) send_stream();
}
void Application::send_stream() {
    if (stream_->sent == stream_->latest) return;
    stream_->sent = stream_->latest;
    stream_->in_flight = submit(SetParameter{stream_->id, stream_->symbol, stream_->latest});
    if (!stream_->in_flight) stream_.reset();
}
void Application::tick(unsigned ms) {
    clock_ms_ += ms;
    for (auto& e : engine_.advance(ms)) {
        const auto it = pending_.find(e.request);
        const auto kind = it == pending_.end() ? PendingKind::Edit : it->second;
        pending_.erase(e.request);
        if (stream_ && e.request && e.request == stream_->in_flight) { stream_->in_flight = 0; send_stream(); }
        if (e.snapshot) graph_ = *e.snapshot;
        if (e.request && tempo_sent_ && pending_.empty()) tempo_sent_.reset();
        if (e.saved_session) { saved_ = *e.saved_session; has_saved_ = true; }
        if (!e.request) {
            // The connection's own news: keep the reason for the banner.
            if (e.state == EngineState::Ready) {
                engine_note_.clear();
                if (last_state_ != EngineState::Ready) message(ever_ready_ ? "Engine is back; showing its graph." : "Engine connected.");
            }
            else if (!e.message.empty()) engine_note_ = e.message;
            if (e.state == EngineState::Ready) ever_ready_ = true;
            // A fresh engine after a restart: offer the startup patch again.
            if (e.state == EngineState::Ready && last_state_ != EngineState::Ready && store_ && !store_->startup().empty() &&
                graph_.patch_id.empty() && !graph_.dirty && std::all_of(graph_.modules.begin(), graph_.modules.end(), [](const Module& m) { return m.fixed; }))
                startup_pending_ = true;
            last_state_ = e.state;
        }
        if (e.accepted && e.request && kind == PendingKind::Replace) {
            pan_x_ = pan_y_ = 0; selected_cable_.reset(); patch_focus_ = 0; navigate(Page::Patch);
        } else if (e.accepted && e.request && kind == PendingKind::Rename) navigate(Page::Patches);
        if (selected_cable_ && std::find(graph_.connections.begin(), graph_.connections.end(), *selected_cable_) == graph_.connections.end())
            selected_cable_.reset();
        if (!e.accepted) message(e.message);
        else if (!e.message.empty()) message(e.message);
        else if (e.request) message("Edit accepted by engine.");
        if (kind == PendingKind::Add) {
            if (!e.accepted) message("Could not add " + adding_[e.request] + ": " + e.message);
            adding_.erase(e.request);
        } else if (kind == PendingKind::Save && staging_) {
            // The engine has written the bundle; publish it, or leave the previous revision alone.
            if (e.accepted) {
                const auto issue = store_->publish(*staging_);
                if (issue.empty()) message("Saved " + graph_.name + " (revision " + std::to_string(staging_->revision) + ").");
                else { store_->discard(*staging_); message("Not saved: " + issue); }
            } else store_->discard(*staging_);
            staging_.reset();
            refresh_stored();
        } else if (kind == PendingKind::SaveAs) {
            if (e.accepted) start_save(save_as_id_);
            save_as_id_.clear();
        } else if (kind == PendingKind::Replace && e.accepted && !load_note_.empty()) message(e.message.empty() ? load_note_ : e.message + " " + load_note_);
        if (kind == PendingKind::Replace) load_note_.clear();
        rebuild_ = true;
    }
    for (const auto& g : recognizer_.advance(ms)) gesture(g);
    if (startup_pending_ && engine_.state() == EngineState::Ready && pending_.empty()) {
        startup_pending_ = false;
        // Only when the engine plays nothing stored and nothing unsaved: a
        // restarted interface must never replace the patch being played.
        const auto id = store_->startup();
        if (!id.empty() && graph_.patch_id.empty() && !graph_.dirty) load_stored(id, false, false);
    }
    // Rebuilding during a held key can deliver its release to a new button.
    // Keep both pointer and keypad targets alive until their gesture ends.
    bool pressed = (pointer_input_ && lv_indev_get_state(pointer_input_) == LV_INDEV_STATE_PRESSED) ||
        (keypad_input_ && lv_indev_get_state(keypad_input_) == LV_INDEV_STATE_PRESSED);
    for (auto* indev = lv_indev_get_next(nullptr); indev && !pressed; indev = lv_indev_get_next(indev))
        if (const auto type = lv_indev_get_type(indev); (type == LV_INDEV_TYPE_POINTER || type == LV_INDEV_TYPE_KEYPAD) && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED)
            pressed = true;
    if (!pressed) {
        pressed_module_ = held_module_ = second_module_ = 0;
        pressed_indev_ = held_indev_ = second_indev_ = nullptr;
        second_press_ms_ = 0;
        hold_connected_ = false;
    }
    if (stream_ && !stream_->in_flight && !pressed) stream_.reset();
    if (rebuild_ && !dragging_ && !panning_ && !cable_source_ && (!pressed || hold_connected_)) rebuild();
    update_metrics();
    if ((network_age_ += ms) >= 2000) { network_age_ = 0; update_network(); }
    if (system_ && (system_age_ += ms) >= 2000) { system_age_ = 0; update_system(); }
    const auto* shown = page_ == Page::Parameters ? find_module(graph_, selected_) : nullptr;
    const auto layout = shown ? module_layout(*shown) : ModuleLayout::Generic;
    engine_.watch_tuner(shown && layout == ModuleLayout::Tuner ? std::optional<InstanceId>{shown->id} : std::nullopt);
    engine_.watch_euclidean(shown && layout == ModuleLayout::Euclidean ? std::optional<InstanceId>{shown->id} : std::nullopt);
    update_tuner();
    update_euclidean();
}
void Application::update_metrics() {
    if (!tempo_value_ || !cpu_value_) return;
    const auto readings = engine_.telemetry();
    const auto tempo = metric_text(readings.tempo_bpm, false), cpu = metric_text(readings.cpu_percent, true);
    if (tempo != lv_label_get_text(tempo_value_)) lv_label_set_text(tempo_value_, tempo.c_str());
    if (cpu != lv_label_get_text(cpu_value_)) lv_label_set_text(cpu_value_, cpu.c_str());
    const auto tempo_ink = color(tempo == "--" ? muted : accent);
    const auto cpu_ink = color(cpu == "--" ? muted : *readings.cpu_percent >= 85 ? warning : text);
    if (!lv_color_eq(lv_obj_get_style_text_color(tempo_value_, LV_PART_MAIN), tempo_ink))
        lv_obj_set_style_text_color(tempo_value_, tempo_ink, 0);
    if (!lv_color_eq(lv_obj_get_style_text_color(cpu_value_, LV_PART_MAIN), cpu_ink))
        lv_obj_set_style_text_color(cpu_value_, cpu_ink, 0);
}
void Application::update_network() {
    if (!network_value_ || !network_) return;
    const auto network = network_();
    if (network.text != lv_label_get_text(network_value_)) lv_label_set_text(network_value_, network.text.c_str());
    const auto ink = color(network.connected ? accent : warning);
    if (!lv_color_eq(lv_obj_get_style_text_color(network_value_, LV_PART_MAIN), ink)) lv_obj_set_style_text_color(network_value_, ink, 0);
}
void Application::update_system() {
    reading_ = system_();
    // Said once: a weak supply makes audio crackle and slows the CPU down.
    if (reading_.under_voltage == true && !low_voltage_seen_) {
        low_voltage_seen_ = true;
        if (page_ != Page::Halting) message("The power supply's voltage is too low; audio can crackle. Settings > Diagnostics.");
    }
    if (page_ == Page::Diagnostics) rebuild_ = true;
}
void Application::bind(lv_obj_t* obj, Action action, InstanceId id, std::string symbol, int number, lv_obj_t* value) {
    bindings_.push_back({this, action, id, std::move(symbol), number, value});
    lv_obj_add_event_cb(obj, event, LV_EVENT_ALL, &bindings_.back());
}
lv_obj_t* Application::button(lv_obj_t* parent, const std::string& title, int x, int y, int w,
                              Action action, InstanceId id, std::string symbol, int number) {
    auto* b = lv_button_create(parent); lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, 48);
    lv_obj_set_style_bg_color(b, color(surface), 0); lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, color(pressed_fill), LV_STATE_PRESSED);
    // Unavailable actions stay visible but read as such.
    lv_obj_set_style_opa(b, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_style_border_width(b, 1, 0); lv_obj_set_style_border_color(b, color(rim), 0);
    lv_obj_set_style_border_color(b, color(text), LV_STATE_FOCUS_KEY);
    lv_obj_set_style_radius(b, 8, 0);
    auto* l = label(b, upper(title), 0, 0, 0, text, &peepo_bold_14);
    lv_obj_set_style_text_letter_space(l, 1, 0); lv_obj_center(l);
    bind(b, action, id, std::move(symbol), number);
    if (auto* group = lv_group_get_default()) lv_group_add_obj(group, b);
    return b;
}
namespace {
// The one action a page is about: white, with black text.
lv_obj_t* primary(lv_obj_t* b) {
    lv_obj_set_style_bg_color(b, color(text), 0); lv_obj_set_style_bg_color(b, color(0xd4d4d4), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_outline_color(b, color(text), LV_STATE_FOCUS_KEY); lv_obj_set_style_outline_width(b, 2, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(b, 2, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), color(background), 0);
    return b;
}
// Header buttons: rounded pills, a little shorter than the band they sit in.
lv_obj_t* pill(lv_obj_t* b) {
    lv_obj_set_y(b, 16); lv_obj_set_height(b, 40);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    return b;
}
// Footer buttons: shorter still.
lv_obj_t* slim(lv_obj_t* b) {
    lv_obj_set_y(b, 6); lv_obj_set_height(b, 36);
    return b;
}
// The emphasis a chosen option gets: its border and text in the given colour.
void mark(lv_obj_t* b, std::uint32_t ink) {
    lv_obj_set_style_border_color(b, color(ink), 0); lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), color(ink), 0);
}
}
void Application::event(lv_event_t* e) {
    auto* b = static_cast<Binding*>(lv_event_get_user_data(e));
    b->app->handle(*b, e);
}
void Application::handle(Binding& b, lv_event_t* e) {
    if (auto* input = lv_indev_active()) {
        if (lv_indev_get_type(input) == LV_INDEV_TYPE_POINTER) pointer_input_ = input;
        else if (lv_indev_get_type(input) == LV_INDEV_TYPE_KEYPAD) keypad_input_ = input;
    }
    const auto code = lv_event_get_code(e);
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (b.action == Action::NameInput) {
        if (code == LV_EVENT_VALUE_CHANGED) {
            draft_name_ = lv_textarea_get_text(target);
            message(renaming_ ? "Rename keeps modules, values and cables." : "New patch starts with IN connected to OUT.");
        } else if (code == LV_EVENT_READY) commit_name();
        return;
    }
    if (b.action == Action::Keyboard) {
        // The default keyboard forwards READY to its textarea. Commit there
        // once, which also handles physical Enter and the on-screen return key.
        if (code == LV_EVENT_CANCEL) navigate(Page::Patches);
        return;
    }
    if (b.action == Action::SelectCable && code == LV_EVENT_HIT_TEST) {
        auto* hit = static_cast<lv_hit_test_info_t*>(lv_event_get_param(e));
        hit->res = false;
        if (b.number < 0 || static_cast<std::size_t>(b.number) >= cables_.size()) return;
        const auto& cable = cables_[b.number];
        const double px = hit->point->x, py = hit->point->y - 72;
        for (std::size_t i = 0; i + 1 < cable.points.size(); ++i) {
            const double ax = cable.points[i].x, ay = cable.points[i].y;
            const double dx = cable.points[i + 1].x - ax, dy = cable.points[i + 1].y - ay;
            const double length = dx * dx + dy * dy;
            const double t = length ? std::clamp(((px - ax) * dx + (py - ay) * dy) / length, 0.0, 1.0) : 0;
            const double ex = px - (ax + t * dx), ey = py - (ay + t * dy);
            if (ex * ex + ey * ey <= 144) { hit->res = true; break; }
        }
        return;
    }
    if (b.action == Action::Pan) {
        auto* indev = lv_indev_active(); if (!indev) return;
        lv_point_t point; lv_indev_get_point(indev, &point);
        if (code == LV_EVENT_PRESSED) {
            if (held_module_ || pressed_module_) return;
            press_ = point; start_x_ = pan_x_; start_y_ = pan_y_; panning_ = true;
        } else if (code == LV_EVENT_PRESSING && panning_) {
            pan_x_ = std::clamp(start_x_ + press_.x - point.x, 0, canvas_width - 800);
            pan_y_ = std::clamp(start_y_ + press_.y - point.y, 0, canvas_height - 360);
            update_viewport();
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (code == LV_EVENT_RELEASED && panning_) {
                pan_x_ = std::clamp(start_x_ + press_.x - point.x, 0, canvas_width - 800);
                pan_y_ = std::clamp(start_y_ + press_.y - point.y, 0, canvas_height - 360);
                update_viewport();
            }
            panning_ = false;
        } else if (code == LV_EVENT_CLICKED && std::abs(point.x - press_.x) + std::abs(point.y - press_.y) <= 8) {
            selected_cable_.reset(); rebuild_ = true;
        }
        return;
    }
    if (b.action == Action::Module || b.action == Action::Port) {
        auto* indev = lv_indev_active();
        if (!indev) return;
        lv_point_t point; lv_indev_get_point(indev, &point);
        const auto* m = find_module(graph_, b.id);
        if (!m) return;
        if (code == LV_EVENT_PRESSED) {
            if (b.action == Action::Module) {
                const bool first_held = held_module_ && held_module_ != b.id && (!held_indev_ || lv_indev_get_state(held_indev_) == LV_INDEV_STATE_PRESSED);
                const bool first_pressed = pressed_module_ && pressed_module_ != b.id && pressed_indev_ && pressed_indev_ != indev && lv_indev_get_state(pressed_indev_) == LV_INDEV_STATE_PRESSED;
                if (first_held || first_pressed) {
                    second_module_ = b.id; second_indev_ = indev; second_press_ = point; second_press_ms_ = lv_tick_get();
                    return;
                }
                pressed_module_ = b.id; pressed_indev_ = indev;
                held_module_ = second_module_ = 0; held_indev_ = second_indev_ = nullptr; second_press_ms_ = 0;
            }
            press_ = point; start_x_ = m->x; start_y_ = m->y; dragging_ = false; long_pressed_ = false;
            if (b.action == Action::Port && (b.number & 1)) {
                cable_source_ = b.id; cable_port_ = b.symbol; cable_cv_ = (b.number & 4) != 0;
                const auto from = jack(*m, b.symbol, Direction::Output, static_cast<unsigned>(b.number >> 3));
                message(cable_cv_ ? "Control cable from " + m->name + ". Drop it on the square connector under another module."
                                  : "Cable from " + m->name + " / " + b.symbol + ". Drop it on an input jack.");
                preview_ = lv_line_create(canvas_); lv_obj_remove_flag(preview_, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_line_color(preview_, color(cable_cv_ ? control_ink : accent), 0);
                lv_obj_set_style_line_width(preview_, 2, 0);
                if (cable_cv_) preview_points_[0] = {static_cast<lv_value_precise_t>(m->x + module_width / 2 - pan_x_), static_cast<lv_value_precise_t>(m->y - pan_y_)};
                else preview_points_[0] = {static_cast<lv_value_precise_t>(m->x + module_width - pan_x_), static_cast<lv_value_precise_t>(m->y + (from ? from->centre : module_height / 2) - pan_y_)};
                indicate_targets(true);
            }
        } else if (code == LV_EVENT_LONG_PRESSED && b.action == Action::Module && !dragging_) {
            const InstanceId first = held_module_ && held_module_ != b.id ? held_module_ : (pressed_module_ && pressed_module_ != b.id ? pressed_module_ : 0);
            auto* first_indev = first == held_module_ ? held_indev_ : pressed_indev_;
            if (first && (!first_indev || lv_indev_get_state(first_indev) == LV_INDEV_STATE_PRESSED)) {
                connect_modules(first, b.id);
                return;
            }
            long_pressed_ = true; held_module_ = b.id; held_indev_ = indev;
            if (!m->fixed && (!second_module_ || !second_indev_ || lv_indev_get_state(second_indev_) != LV_INDEV_STATE_PRESSED)) {
                selected_ = b.id; navigate(Page::ModuleActions);
            }
        } else if (code == LV_EVENT_PRESSING) {
            if (b.action == Action::Module && indev == second_indev_ && second_module_ == b.id && held_module_ != b.id) return;
            if (b.action == Action::Module && long_pressed_ && held_module_ == b.id && !dragging_) {
                InstanceId under = 0;
                if (point.x >= 0 && point.x < 800 && point.y >= 72 && point.y < 432) {
                    const int cx = point.x + pan_x_, cy = point.y - 72 + pan_y_;
                    for (const auto& item : graph_.modules)
                        if (item.id != b.id && cx >= item.x && cx < item.x + module_width && cy >= item.y && cy < item.y + module_height) {
                            under = item.id;
                            break;
                        }
                }
                if (!under) {
                    if (second_indev_ == indev) { second_module_ = 0; second_press_ms_ = 0; }
                } else if (under != second_module_ || second_indev_ != indev) {
                    second_module_ = under; second_indev_ = indev; second_press_ = point; second_press_ms_ = lv_tick_get();
                } else if (lv_tick_elaps(second_press_ms_) >= 400) {
                    connect_modules(b.id, under);
                    return;
                }
            } else if (b.action == Action::Module && !m->fixed && !long_pressed_) {
                const int dx = point.x - press_.x, dy = point.y - press_.y;
                if (std::abs(dx) + std::abs(dy) > 8) { dragging_ = true; pressed_module_ = held_module_ = 0; }
                if (dragging_) {
                    lv_obj_set_pos(target, std::clamp(start_x_ + dx, module_min_x, module_max_x) - pan_x_,
                        std::clamp(start_y_ + dy, module_min_y, module_max_y) - pan_y_);
                    update_cables();
                }
            } else if (cable_source_ && preview_) {
                sample_cable(preview_, preview_points_, preview_points_[0].x, preview_points_[0].y, point.x, point.y - 72, cable_cv_);
            }
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (b.action == Action::Module) {
                if (indev == second_indev_ && second_module_ == b.id && held_module_ != b.id) {
                    second_module_ = 0; second_indev_ = nullptr; second_press_ms_ = 0;
                    return;
                }
                if (b.id == held_module_) { held_module_ = 0; held_indev_ = nullptr; }
                if (b.id == pressed_module_) { pressed_module_ = 0; pressed_indev_ = nullptr; }
            }
            if (cable_source_ && cable_cv_) {
                // A module can have a dozen control inputs, far too many for its edge.
                // Drop on its control connector, then choose the ports from a list.
                InstanceId destination = 0;
                if (point.x >= 0 && point.x < 800 && point.y >= 72 && point.y < 432)
                    for (const auto& item : graph_.modules)
                        if (item.id != cable_source_ && !cv_ports(item, Direction::Input).empty() &&
                            std::abs(point.x + pan_x_ - item.x - module_width / 2) <= 24 && std::abs(point.y - 72 + pan_y_ - item.y - module_height) <= 24) destination = item.id;
                if (destination && code == LV_EVENT_RELEASED) {
                    const auto outputs = cv_ports(*m, Direction::Output);
                    cv_from_ = cable_source_; cv_to_ = destination; cv_page_ = 0; cv_only_ = true;
                    cv_output_ = outputs.size() == 1 ? outputs.front()->symbol : std::string();
                    navigate(Page::CvConnect);
                } else message("Drop the control cable on the square connector under a module.");
                if (preview_) lv_obj_delete(preview_);
                preview_ = nullptr; cable_source_ = 0; indicate_targets(false);
            } else if (cable_source_) {
                // The nearest input jack under the finger wins; a module can have several.
                InstanceId destination = 0; std::string input; int nearest = 0;
                if (point.x >= 0 && point.x < 800 && point.y >= 72 && point.y < 432)
                    for (const auto& item : graph_.modules) for (const auto& port : module_ports(item)) for (unsigned channel = 0; channel < port.channels; ++channel) {
                        const auto drop = jack(item, port.symbol, Direction::Input, channel);
                        if (!drop || std::abs(point.x + pan_x_ - item.x) > 24) continue;
                        const int distance = std::abs(point.y - 72 + pan_y_ - item.y - drop->centre);
                        if (distance > drop->height / 2 + 2 || (destination && distance >= nearest)) continue;
                        destination = item.id; input = port.symbol; nearest = distance;
                    }
                if (destination && code == LV_EVENT_RELEASED) submit(Connect{{cable_source_, destination, cable_port_, input}});
                else message("Drop the cable on an input jack.");
                if (preview_) lv_obj_delete(preview_);
                preview_ = nullptr; cable_source_ = 0; indicate_targets(false);
            } else if (dragging_) {
                // Release can contain a final pointer sample without a preceding
                // PRESSING event. Commit that point, rather than the last frame.
                if (code == LV_EVENT_RELEASED)
                    submit(MoveModule{b.id, snap(start_x_ + point.x - press_.x), snap(start_y_ + point.y - press_.y)});
                rebuild_ = true;
                dragging_ = false;
            }
        } else if (code == LV_EVENT_CLICKED && b.action == Action::Module && !m->fixed) {
            // A completed drag is followed by CLICKED; distinguish it using displacement.
            if (!long_pressed_ && !held_module_ && std::abs(point.x - press_.x) + std::abs(point.y - press_.y) <= 8) {
                open_module(b.id);
            }
        }
        return;
    }
    if (b.action == Action::Parameter) {
        const auto* m = find_module(graph_, b.id); if (!m) return;
        const auto it = std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == b.symbol; });
        if (it == m->parameters.end()) return;
        lv_point_t point{};
        if (auto* indev = lv_indev_active()) lv_indev_get_point(indev, &point);
        const bool slides = (it->kind == ParameterKind::Continuous || it->kind == ParameterKind::Integer) && !is_euclidean_toggle(*it);
        const int span = bar_span(target);
        // Relative, as an encoder is: the value moves by how far the finger
        // goes, the whole width for the whole range, so touching never jumps it.
        const auto dragged = [&] {
            auto p = *it;
            p.value = value_from_normalized(p, std::clamp(drag_origin_ + static_cast<double>(point.x - press_.x) / span, 0.0, 1.0));
            if (p.kind == ParameterKind::Integer) p.value = std::round(p.value);
            return p;
        };
        if (code == LV_EVENT_PRESSED) {
            press_ = point; bar_moving_ = bar_dragged_ = false;
            auto current = *it;
            if (stream_ && stream_->id == b.id && stream_->symbol == b.symbol) current.value = stream_->latest;
            drag_origin_ = normalized_value(current);
            // The touched bar takes the encoder highlight.
            for (std::size_t i = 0; i < bars_.size(); ++i)
                if (bars_[i].widget == target && i != parameter_focus_) {
                    if (parameter_focus_ < bars_.size()) highlight(bars_[parameter_focus_].widget, false, false, 0);
                    parameter_focus_ = i; adjusting_ = false;
                    highlight(target, true, false, bar_ink(*m, b.symbol));
                }
        } else if (code == LV_EVENT_PRESSING && slides) {
            const int dx = point.x - press_.x, dy = point.y - press_.y;
            if (!bar_moving_ && std::abs(dx) > 8 && std::abs(dx) > std::abs(dy)) {
                bar_moving_ = bar_dragged_ = true;
                // A sideways drag is the value's: the list must not scroll under it.
                if (parameter_list_) lv_obj_remove_flag(parameter_list_, LV_OBJ_FLAG_SCROLLABLE);
            }
            if (bar_moving_) { const auto p = dragged(); paint_bar(target, p); stream_parameter(b.id, b.symbol, p.value); update_euclidean(); }
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (bar_moving_) {
                // Release sends the final position.
                if (code == LV_EVENT_RELEASED) { const auto p = dragged(); paint_bar(target, p); stream_parameter(b.id, b.symbol, p.value); update_euclidean(); }
                if (parameter_list_) lv_obj_add_flag(parameter_list_, LV_OBJ_FLAG_SCROLLABLE);
                bar_moving_ = false;
            }
        } else if (code == LV_EVENT_CLICKED && !bar_dragged_ && !slides) submit(SetParameter{b.id, b.symbol, stepped_choice(*it)});
        return;
    }
    if (b.action == Action::TempoSlider) {
        // Whole beats per minute; the value follows the finger, the engine hears the release.
        if (code != LV_EVENT_VALUE_CHANGED && code != LV_EVENT_RELEASED) return;
        const auto bpm = static_cast<double>(lv_slider_get_value(target));
        if (b.value_label) lv_label_set_text(b.value_label, metric_text(bpm, false).c_str());
        if (code == LV_EVENT_RELEASED) set_tempo(bpm);
        return;
    }
    if (b.action == Action::RangeEnd) {
        // The From (0) and To (1) ends of an assignment's range, sent on release.
        const auto* a = pinned(editing_control_);
        const auto* p = a ? assignment_parameter(graph_, *a) : nullptr;
        if (!p) return;
        auto end = *p;
        end.value = value_from_normalized(*p, lv_slider_get_value(target) / 1000.0);
        if (p->kind != ParameterKind::Continuous) end.value = std::round(end.value);
        if (b.value_label) lv_label_set_text(b.value_label, format_value(end).c_str());
        if (code == LV_EVENT_RELEASED) {
            auto changed = *a;
            (b.number ? changed.maximum : changed.minimum) = end.value;
            if (changed.minimum > changed.maximum) { message("From must not pass To; use Invert to reverse the direction."); rebuild_ = true; return; }
            submit(Assign{changed});
        }
        return;
    }
    if (code != LV_EVENT_CLICKED) return;
    switch (b.action) {
    case Action::Navigate: navigate(static_cast<Page>(b.number)); break;
    case Action::Add: add_module(b.symbol); break;
    case Action::Duplicate:
        if (const auto slot = vacant_slot()) {
            const auto [x, y] = *slot;
            submit(DuplicateModule{b.id, x, y});
            reveal(x, y); navigate(Page::Patch);
        } else message("No free module slot remains on this canvas.");
        break;
    case Action::SelectCable:
        if (b.number >= 0 && static_cast<std::size_t>(b.number) < cables_.size()) {
            selected_cable_ = cables_[b.number].connection; rebuild_ = true;
            const auto* from = find_module(graph_, selected_cable_->from); const auto* to = find_module(graph_, selected_cable_->to);
            message(from && to ? from->name + " / " + selected_cable_->output + " > " + to->name + " / " + selected_cable_->input : "Cable selected.");
        }
        break;
    case Action::DeleteCable: if (selected_cable_) submit(Disconnect{*selected_cable_}); break;
    case Action::ClearCable: selected_cable_.reset(); rebuild_ = true; break;
    case Action::Home: pan_x_ = pan_y_ = 0; rebuild_ = true; break;
    case Action::OpenActions: selected_ = b.id; navigate(Page::ModuleActions); break;
    case Action::Edit: open_module(b.id); break;
    case Action::Bypass: {
        const auto* m = find_module(graph_, b.id); if (m) submit(SetBypass{b.id, !m->bypassed}); break;
    }
    case Action::Remove: submit(RemoveModule{b.id}); navigate(Page::Patch); break;
    case Action::SaveSession:
        if (pending()) { message("Wait for pending edits before saving."); break; }
        submit(SaveGraph{}); break;
    case Action::LoadSession:
        if (has_saved_) replace_patch(LoadSession{}, Page::Patches);
        else message("Save a session snapshot first.");
        break;
    case Action::ParallelDemo: replace_patch(LoadGraph{parallel_demo_graph()}, Page::Patches); break;
    case Action::TempoStep: set_tempo((tempo_sent_ ? *tempo_sent_ : graph_.tempo) + b.number); break;
    case Action::Tap: tap_tempo(); break;
    case Action::ChooseFile: selected_ = b.id; file_page_ = 0; navigate(Page::FilePicker); break;
    case Action::FilePage: file_page_ = static_cast<std::size_t>(b.number); rebuild_ = true; break;
    case Action::PickFile:
        if (submit(SetFile{b.id, b.symbol})) message("Loading " + file_name(b.symbol) + ". The plugin reads it in the background.");
        navigate(Page::Parameters);
        break;
    case Action::State:
        if (static_cast<EngineState>(b.number) == EngineState::Ready) engine_.connect();
        else if (static_cast<EngineState>(b.number) == EngineState::Disconnected) engine_.disconnect();
        else if (!engine_.simulate_state(EngineState::Error)) message("Unsupported capability: simulated engine error");
        break;
    case Action::Reject:
        message(engine_.simulate_rejection() ? "The next engine edit will be rejected." : "Unsupported capability: simulated rejection"); break;
    case Action::MissingFixture:
        if (engine_.simulate_missing_module()) navigate(Page::Patch);
        else message("Missing-module fixture requires a ready simulator with no pending edit.");
        break;
    case Action::NameEntry:
        if (pending()) { message("Wait for pending edits before naming a patch."); break; }
        // 0: new patch, 1: rename, 2: save under a new name as another stored patch.
        renaming_ = b.number == 1; saving_as_ = b.number == 2;
        draft_name_ = b.number ? graph_.name : "";
        message(saving_as_ ? "Saves a separate copy; the original stays as it was saved." :
                renaming_ ? "Rename keeps modules, values and cables." : "New patch starts with IN connected to OUT.");
        navigate(Page::NamePatch); break;
    case Action::SavePatch: save_patch(b.number == 1); break;
    case Action::LoadStored: load_stored(b.symbol, false, true); break;
    case Action::DeleteStored: {
        const auto patch = store_ ? store_->find(b.symbol) : std::nullopt;
        if (!patch) { refresh_stored(); rebuild_ = true; break; }
        const auto id = patch->id;
        ask("Delete saved patch?", patch->name, "Every saved revision of this patch is removed from disk.\nThe patch playing now is not changed.", "Delete", [this, id] {
            const auto issue = store_->remove(id);
            message(issue.empty() ? "Saved patch deleted." : issue);
            refresh_stored(); navigate(Page::Patches);
        });
        break;
    }
    case Action::StartupStored: {
        if (!store_) break;
        const bool already = store_->startup() == b.symbol;
        const auto issue = store_->set_startup(already ? std::string() : b.symbol);
        message(!issue.empty() ? issue : already ? "No startup patch: the engine keeps whatever it plays." : "This patch loads when the instrument starts.");
        refresh_stored(); rebuild_ = true; break;
    }
    case Action::PatchPage: patch_page_ = static_cast<std::size_t>(b.number); rebuild_ = true; break;
    case Action::AssignControl:
        editing_control_ = b.symbol; picking_module_ = 0; picker_page_ = 0;
        message("Choose what " + control_label(b.symbol) + " controls in this patch.");
        navigate(Page::AssignPicker); break;
    case Action::EditControl: editing_control_ = b.symbol; navigate(Page::AssignmentDetail); break;
    case Action::ClearControl:
        if (pinned(b.symbol)) submit(Unassign{b.symbol});
        else if (store_ && store_->global_controls().contains(b.symbol)) {
            const auto issue = store_->set_global_control(b.symbol, {});
            message(issue.empty() ? control_label(b.symbol) + " cleared." : issue);
        }
        navigate(Page::Controls); break;
    case Action::PickModule: picking_module_ = b.id; picker_page_ = 0; rebuild_ = true; break;
    case Action::PickerPage: picker_page_ = static_cast<std::size_t>(b.number); rebuild_ = true; break;
    case Action::PickTarget: {
        if (b.symbol == "@next-patch" || b.symbol == "@previous-patch" || b.symbol == "@tap-tempo") {
            const auto issue = store_ ? store_->set_global_control(editing_control_, b.symbol.substr(1)) : std::string("Patch storage is not available");
            message(!issue.empty() ? issue : b.symbol == "@tap-tempo" ? control_label(editing_control_) + " taps the tempo in every patch that does not use it otherwise." :
                    control_label(editing_control_) + " changes patch in every patch that does not use it otherwise.");
            navigate(Page::Controls); break;
        }
        const auto* m = find_module(graph_, picking_module_);
        if (!m) { picking_module_ = 0; rebuild_ = true; break; }
        Assignment a;
        a.control = editing_control_; a.module = m->id;
        if (b.symbol == "@bypass") a.action = ControlAction::Bypass;
        else {
            const auto it = std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == b.symbol; });
            if (it == m->parameters.end()) break;
            a.parameter = it->symbol; a.minimum = it->minimum; a.maximum = it->maximum;
        }
        // A bypass joins the modules the stomp already switches; Clear empties it.
        submit(Assign{a, a.action == ControlAction::Bypass});
        navigate(Page::Controls); break;
    }
    case Action::InvertControl:
        if (const auto* a = pinned(editing_control_)) { auto changed = *a; changed.inverted = !changed.inverted; submit(Assign{changed}); }
        break;
    case Action::SwitchModeChoice:
        if (const auto* a = pinned(editing_control_)) { auto changed = *a; changed.mode = b.number ? SwitchMode::Momentary : SwitchMode::Toggle; submit(Assign{changed}); }
        break;
    case Action::ConfirmAction: if (confirmation_.act) { auto act = std::move(confirmation_.act); confirmation_.act = nullptr; act(); } break;
    case Action::CancelAction: confirmation_.act = nullptr; navigate(confirmation_.back); break;
    case Action::PowerOff: power_action(false); break;
    case Action::Restart: power_action(true); break;
    case Action::Revert:
        // Back to the last saved revision, through the same guard as any load.
        if (graph_.patch_id.empty() || !graph_.dirty) { message("Nothing to revert: the patch is as it was saved."); break; }
        load_stored(graph_.patch_id, false, true);
        break;
    case Action::CommitName: commit_name(); break;
    case Action::Confirm:
        if (replacement_ && !pending()) submit(*replacement_);
        else message("Wait for pending edits before replacing the patch.");
        break;
    case Action::CancelReplace: replacement_.reset(); navigate(confirm_origin_); break;
    case Action::BrowserGroup:
        if (browser_group_ != b.symbol) { browser_group_ = b.symbol; browser_category_.clear(); browser_pick_.clear(); browser_scroll_ = 0; rebuild_ = true; }
        break;
    case Action::BrowserCategory:
        if (browser_category_ != b.symbol) { browser_category_ = b.symbol; browser_pick_.clear(); browser_scroll_ = 0; rebuild_ = true; }
        break;
    case Action::BrowserPick:
        // The first tap picks a card; tapping it again, or Add, adds it.
        if (b.symbol.empty() || b.symbol == browser_pick_) { if (!browser_pick_.empty()) add_module(browser_pick_); }
        else browser_pick(b.symbol, false);
        break;
    case Action::CvPage: cv_page_ = static_cast<std::size_t>(b.number); rebuild_ = true; break;
    case Action::CvChoice:
        if (cv_output_.empty()) {
            const auto* from = find_module(graph_, cv_from_);
            const auto* to = find_module(graph_, cv_to_);
            const auto inputs = from && to ? connectable_ports(*from, *to, b.symbol, cv_only_) : std::vector<const Port*>{};
            if (inputs.size() == 1) {
                submit(Connect{{cv_from_, cv_to_, b.symbol, inputs.front()->symbol}});
                navigate(Page::Patch);
            } else {
                cv_output_ = b.symbol; cv_page_ = 0; rebuild_ = true;
            }
        } else { submit(Connect{{cv_from_, cv_to_, cv_output_, b.symbol}}); navigate(Page::Patch); }
        break;
    default: break;
    }
}
void Application::add_module(const std::string& uri) {
    const auto slot = vacant_slot();
    if (!slot) { message("No free module slot remains on this canvas."); return; }
    const auto [x, y] = *slot;
    if (const auto request = submit(AddModule{uri, x, y})) {
        const auto* d = definition(uri);
        pending_[request] = PendingKind::Add; adding_[request] = d ? d->name : uri;
    }
    reveal(x, y); navigate(Page::Patch);
}
void Application::commit_name() {
    if (pending()) { message("Wait for pending edits before naming a patch."); return; }
    if (auto issue = validate_patch_name(draft_name_); !issue.empty()) { message(issue); return; }
    if (saving_as_) {
        // A new identity; the name changes first, so the copy is saved under it.
        const auto id = store_ ? store_->new_id() : std::string();
        if (id.empty()) { message("Patch storage is not available."); return; }
        navigate(Page::Patches);
        if (draft_name_ == graph_.name) { start_save(id); return; }
        if (const auto request = submit(RenamePatch{draft_name_})) { pending_[request] = PendingKind::SaveAs; save_as_id_ = id; }
    } else if (renaming_) submit(RenamePatch{draft_name_});
    else replace_patch(NewPatch{draft_name_}, Page::NamePatch);
}
void Application::save_patch(bool as_new) {
    if (!store_) return;
    if (pending()) { message("Wait for pending edits before saving."); return; }
    if (as_new) {
        renaming_ = false; saving_as_ = true; draft_name_ = graph_.name;
        message("Saves a separate copy; the original stays as it was saved.");
        navigate(Page::NamePatch);
        return;
    }
    // Save writes a new revision of the stored patch this graph came from.
    start_save(graph_.patch_id.empty() ? store_->new_id() : graph_.patch_id);
}
void Application::start_save(const std::string& id) {
    std::string issue;
    auto staging = store_->stage(id, issue);
    if (!staging) { message(issue); return; }
    const auto request = submit(SaveGraph{SaveScope::Persistent, engine_.capabilities().plugin_state, staging->location, id});
    if (!request) { store_->discard(*staging); return; }
    pending_[request] = PendingKind::Save;
    staging_ = std::move(*staging);
    message("Saving " + graph_.name + "...");
}
void Application::load_stored(const std::string& id, bool allow_missing, bool guarded) {
    const auto patch = store_ ? store_->find(id) : std::nullopt;
    if (!patch) { message("That patch is no longer stored."); refresh_stored(); rebuild_ = true; return; }
    if (!patch->revision) { message(patch->name + ": " + patch->issue); return; }
    const auto blocks = saved_blocks(patch->bundle);
    std::vector<std::string> absent;
    for (const auto& block : blocks)
        if (const auto* known = definition(block.plugin); !known || known->availability != Availability::Available) absent.push_back(block.name);
    std::string names;
    for (const auto& name : absent) names += (names.empty() ? "" : ", ") + name;
    if (!absent.empty() && !allow_missing) {
        ask("Modules unavailable", patch->name, "Not available here: " + names + ".\nLoad the rest without plugin state? Saving afterwards leaves these modules out.", "Load the rest",
            [this, id, guarded] { navigate(Page::Patches); load_stored(id, true, guarded); });
        return;
    }
    Command command = LoadPatch{patch->location, blocks, patch->id};
    load_note_ = patch->issue;
    if (!patch->missing_files.empty()) {
        std::string files;
        for (const auto& file : patch->missing_files) files += (files.empty() ? "" : ", ") + file;
        load_note_ += (load_note_.empty() ? "" : " ") + std::string("Missing files: ") + files + ".";
    }
    if (!absent.empty()) {
        // The engine loads none of a bundle with a missing plugin. Rebuild the
        // rest from its description instead: routing, values, names, layout.
        auto rest = bundle_graph(patch->bundle);
        std::erase_if(rest.modules, [](const Module& m) { const auto* d = definition(m.uri); return !m.fixed && (!d || d->availability != Availability::Available); });
        std::erase_if(rest.connections, [&](const Connection& c) { return !find_module(rest, c.from) || !find_module(rest, c.to); });
        rest.patch_id = patch->id;
        rest.dirty = true;
        command = LoadGraph{std::move(rest)};
        load_note_ = "Left out: " + names + ". Plugin state was not restored; saving keeps the patch without them.";
    }
    if (guarded) replace_patch(std::move(command), Page::Patches);
    else submit(std::move(command));
}
void Application::power_action(bool restart) {
    if (!(restart ? restart_ : power_off_)) return;
    if (pending()) { message(restart ? "Wait for pending edits before restarting." : "Wait for pending edits before shutting down."); return; }
    const std::string what = restart ? "Restart" : "Shut down";
    ask(what, restart ? "Restart Peepo?" : "Shut down Peepo?", std::string(graph_.dirty ? "This patch has unsaved edits; they will be lost.\n" : "") +
        (restart ? "Saved patches are safe. Peepo starts again by itself, with its startup patch." : "Saved patches are safe. Unplug when the screen goes dark."), what, [this, restart] {
        // An edit or patch change made with a control while this was asked must finish first.
        if (pending()) { message(restart ? "Wait for pending edits before restarting." : "Wait for pending edits before shutting down."); navigate(Page::Power); return; }
        if (const auto issue = (restart ? restart_ : power_off_)(); !issue.empty()) { message(issue); navigate(Page::Power); return; }
        restarting_ = restart;
        message(restart ? "Restarting." : "Shutting down. Unplug when the screen goes dark.");
        navigate(Page::Halting);
    });
    confirmation_.back = Page::Power;
}
void Application::ask(std::string title, std::string heading, std::string body, std::string button, std::function<void()> act) {
    confirmation_ = {std::move(title), std::move(heading), std::move(body), std::move(button), std::move(act)};
    navigate(Page::ConfirmAction);
}
void Application::replace_patch(Command command, Page origin) {
    if (pending()) { message("Wait for pending edits before replacing the patch."); return; }
    if (graph_.dirty) {
        replacement_ = std::move(command); confirm_origin_ = origin;
        message("Choose Replace patch to discard unsaved edits, or Cancel."); navigate(Page::ConfirmReplace);
    }
    else submit(std::move(command));
}
void Application::rebuild() {
    rebuild_ = false;
    if (!screen_) {
        screen_ = lv_obj_create(nullptr);
        lv_obj_remove_style_all(screen_); lv_obj_set_size(screen_, 800, 480);
        lv_obj_set_style_bg_color(screen_, color(background), 0); lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
        lv_screen_load(screen_);
    } else {
        if (browser_list_) browser_scroll_ = lv_obj_get_scroll_y(browser_list_);
        if (parameter_list_ && page_ == Page::Parameters) parameter_scroll_ = lv_obj_get_scroll_y(parameter_list_);
        lv_obj_clean(screen_);
    }
    browser_list_ = browser_add_ = parameter_list_ = nullptr; browser_cards_.clear();
    bars_.clear();
    bindings_.clear(); module_widgets_.clear(); input_jacks_.clear(); cv_inputs_.clear(); dots_.clear(); cables_.clear(); canvas_ = nullptr; footer_ = nullptr;
    tempo_value_ = cpu_value_ = network_value_ = nullptr;
    tuner_note_ = tuner_detail_ = tuner_status_ = tuner_needle_ = nullptr;
    euclidean_status_ = nullptr;
    euclidean_rings_ = {}; euclidean_halos_ = {}; euclidean_dots_ = {}; euclidean_step_labels_ = {}; euclidean_tiles_ = {};
    const bool patch = page_ == Page::Patch, ready = engine_.state() == EngineState::Ready;
    // Every page: a header naming where you are, the page, and a footer with
    // the way back, what is happening, and the readings.
    auto* header = band(screen_, 0, 72);
    auto* content = band(screen_, 72, 360);
    auto* foot = band(screen_, 432, 48);
    lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(foot, 1, 0); lv_obj_set_style_border_color(foot, color(0x1f1f1f), 0);
    std::string section = "PATCH", title = graph_.name + (graph_.dirty ? " *" : "");
    switch (page_) {
    case Page::Patch: break;
    case Page::Browser: section = "BROWSER"; title.clear(); break;
    case Page::Parameters: case Page::ModuleActions: {
        section = page_ == Page::Parameters ? "MODULE" : "ACTIONS";
        const auto* m = find_module(graph_, selected_);
        title = m ? m->name : "Module";
        break;
    }
    case Page::Patches: section = "PATCHES"; title = store_ ? "Patches" : "Session patches"; break;
    case Page::Settings: section = "SETTINGS"; title = engine_.capabilities().simulation ? "Simulator settings" : "Engine settings"; break;
    case Page::NamePatch: section = "PATCHES"; title = saving_as_ ? "Save as a new patch" : renaming_ ? "Rename patch" : "New patch"; break;
    case Page::ConfirmReplace: section = "PATCHES"; title = "Replace current patch?"; break;
    case Page::ConfirmAction: section = "CONFIRM"; title = confirmation_.title; break;
    case Page::CvConnect: {
        section = "CABLE";
        const auto* from = find_module(graph_, cv_from_); const auto* to = find_module(graph_, cv_to_);
        const auto ports = from && to ? connectable_ports(*from, *to, cv_output_, cv_only_) : std::vector<const Port*>{};
        title = cv_only_ || std::all_of(ports.begin(), ports.end(), [](const Port* p) { return p->type == PortType::Cv; }) ? "Control cable" : "Connect cable";
        break;
    }
    case Page::Controls: section = "CONTROLS"; title = "Controls"; break;
    case Page::AssignPicker:
        section = "CONTROLS"; title = "Assign " + control_label(editing_control_);
        if (picking_module_) if (const auto* m = find_module(graph_, picking_module_)) title = control_label(editing_control_) + ": " + m->name;
        break;
    case Page::AssignmentDetail: section = "CONTROLS"; title = control_label(editing_control_); break;
    case Page::Diagnostics: section = "SETTINGS"; title = "Diagnostics"; break;
    case Page::Tempo: section = "TEMPO"; title = "Tempo"; break;
    case Page::FilePicker: {
        section = "PARAMETERS";
        const auto* m = find_module(graph_, selected_);
        const auto* d = m ? definition(m->uri) : nullptr;
        title = d && !d->file.label.empty() ? d->file.label + " for " + m->name : "Choose a file";
        break;
    }
    case Page::Power: section = "SETTINGS"; title = "Power"; break;
    case Page::Halting: section = "POWER"; title = restarting_ ? "Restarting" : "Shutting down"; break;
    }
    // The dot is the engine: green when it answers.
    auto* dot = panel(header, 24, title.empty() ? 32 : 17, 8, 8, ready ? accent : warning);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    auto* crumb = label(header, "PEEPO // " + section, 40, title.empty() ? 27 : 12, 0, faint, &peepo_mono_14);
    lv_obj_set_style_text_letter_space(crumb, 2, 0);
    if (!title.empty()) {
        auto* heading = label(header, upper(title), 24, 32, patch ? 412 : page_ == Page::Settings ? 330 : page_ == Page::Patches ? 480 : page_ == Page::Parameters ? 440 : 600, text, &peepo_bold_24);
        lv_label_set_long_mode(heading, LV_LABEL_LONG_DOT); lv_obj_set_height(heading, 30);
        if (patch) {
            // Tapping the patch name brings the canvas back to its start.
            lv_obj_add_flag(heading, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_ext_click_area(heading, 12);
            bind(heading, Action::Home);
            if (auto* group = lv_group_get_default()) lv_group_add_obj(group, heading);
        }
    }
    if (page_ == Page::Browser) { browser(header, content, foot); lv_obj_update_layout(screen_); return; }
    if (patch) {
        auto* add = primary(pill(button(header, "+ Add", 448, 12, 96, Action::Navigate, 0, {}, static_cast<int>(Page::Browser))));
        // Nothing can be added until the engine answers again.
        if (!ready) lv_obj_add_state(add, LV_STATE_DISABLED);
        pill(button(header, "Patches", 556, 12, 104, Action::Navigate, 0, {}, static_cast<int>(Page::Patches)));
        pill(button(header, "Settings", 672, 12, 104, Action::Navigate, 0, {}, static_cast<int>(Page::Settings)));
    } else if (page_ == Page::NamePatch) {
        primary(pill(button(header, saving_as_ ? "Save" : renaming_ ? "Rename" : "Create", 640, 12, 136, Action::CommitName)));
    } else if (page_ == Page::Settings || (page_ == Page::Patches && store_)) {
        // The pages Settings leads to, or what Patches can do to the patch playing,
        // as pills as wide as their names.
        auto* pills = lv_obj_create(header); lv_obj_remove_style_all(pills);
        lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(pills, 8, 0);
        lv_obj_remove_flag(pills, LV_OBJ_FLAG_CLICKABLE);
        auto link = [&](const std::string& name, Action action, Page target) {
            auto* b = pill(button(pills, name, 0, 0, 0, action, 0, {}, static_cast<int>(target)));
            lv_obj_set_width(b, LV_SIZE_CONTENT); lv_obj_set_style_pad_hor(b, 18, 0);
            return b;
        };
        if (page_ == Page::Patches) {
            auto* revert = link("Revert", Action::Revert, Page::Patches);
            if (graph_.patch_id.empty() || !graph_.dirty) lv_obj_add_state(revert, LV_STATE_DISABLED);
        } else {
            link("Controls", Action::Navigate, Page::Controls);
            link("Diagnostics", Action::Navigate, Page::Diagnostics);
            if (power_off_ || restart_) link("Power", Action::Navigate, Page::Power);
        }
        lv_obj_align(pills, LV_ALIGN_RIGHT_MID, -24, 0);
    } else if (const auto* m = page_ == Page::Parameters ? find_module(graph_, selected_) : nullptr; m && module_available(*m)) {
        // On or bypassed, in the module's colour, and its other actions.
        auto* pills = lv_obj_create(header); lv_obj_remove_style_all(pills);
        lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(pills, 8, 0);
        lv_obj_remove_flag(pills, LV_OBJ_FLAG_CLICKABLE);
        auto* power = pill(button(pills, m->bypassed ? "Bypassed" : "On", 0, 0, 0, Action::Bypass, m->id));
        lv_obj_set_width(power, LV_SIZE_CONTENT); lv_obj_set_style_pad_hor(power, 22, 0);
        if (m->bypassed) lv_obj_set_style_text_color(lv_obj_get_child(power, 0), color(muted), 0);
        else { lv_obj_set_style_bg_color(power, color(module_fill(*m)), 0); lv_obj_set_style_border_width(power, 0, 0); }
        auto* actions = pill(button(pills, "Actions", 0, 0, 0, Action::OpenActions, m->id));
        lv_obj_set_width(actions, LV_SIZE_CONTENT); lv_obj_set_style_pad_hor(actions, 18, 0);
        lv_obj_align(pills, LV_ALIGN_RIGHT_MID, -24, 0);
    } else if (page_ != Page::Halting) {
        auto* mode = label(header, engine_.capabilities().simulation ? "SIMULATED" : "ENGINE", 640, 28, 136, faint, &peepo_mono_14);
        lv_obj_set_style_text_align(mode, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_set_style_text_letter_space(mode, 2, 0);
    }
    switch (page_) {
    case Page::Patch: patch_view(content); break;
    case Page::Browser: break;
    case Page::Parameters: parameters(content); break;
    case Page::Patches: patches(content); break;
    case Page::Settings: settings(content); break;
    case Page::ModuleActions: module_actions(content); break;
    case Page::NamePatch: name_patch(content); break;
    case Page::ConfirmReplace: confirm_replace(content); break;
    case Page::CvConnect: cv_connect(content); break;
    case Page::ConfirmAction: confirm_action(content); break;
    case Page::Controls: controls(content); break;
    case Page::AssignPicker: assign_picker(content); break;
    case Page::AssignmentDetail: assignment_detail(content); break;
    case Page::Diagnostics: diagnostics(content); break;
    case Page::FilePicker: file_picker(content); break;
    case Page::Tempo: tempo(content); break;
    case Page::Power: power(content); break;
    case Page::Halting: halting(content); break;
    }
    std::string missing;
    if (patch) for (const auto& m : graph_.modules) if (!module_available(m)) missing += (missing.empty() ? "" : ", ") + m.name;
    if (patch && (!ready || !missing.empty())) {
        // Over the canvas, but never in the way: touches pass through to the patch below.
        auto* banner = panel(content, 24, 8, 752, 40, 0x1f130a);
        lv_obj_set_style_border_color(banner, color(warning), 0);
        lv_obj_remove_flag(banner, LV_OBJ_FLAG_CLICKABLE);
        // The engine first: nothing can be repaired while it is away.
        auto* note = label(banner, !ready ? engine_banner() : "Not available here: " + missing + ". Its cables are silent; hold it to delete it.",
                           14, 10, 724, warning, &lv_font_montserrat_14);
        lv_label_set_long_mode(note, LV_LABEL_LONG_DOT); lv_obj_set_height(note, 18);
    }
    // Nothing is left to go back to once the machine is going down.
    if (!patch && page_ != Page::Halting) {
        slim(button(foot, "< Back", 14, 0, 112, Action::Navigate, 0, {}, static_cast<int>(back_page())));
    }
    const int message_x = patch || page_ == Page::Halting ? 24 : 142;
    footer_ = label(foot, message_, message_x, 7, patch && selected_cable_ ? 240 : 592 - message_x, faint, &peepo_mono_14);
    lv_obj_set_height(footer_, 34); lv_label_set_long_mode(footer_, LV_LABEL_LONG_DOT);
    if (patch && selected_cable_) {
        slim(button(foot, "Deselect", 280, 0, 128, Action::ClearCable));
        slim(button(foot, "Delete cable", 424, 0, 168, Action::DeleteCable));
    }
    auto* metrics = panel(foot, 608, 4, 178, 40, background);
    lv_obj_set_style_border_width(metrics, 0, 0);
    lv_obj_remove_flag(metrics, LV_OBJ_FLAG_CLICKABLE);
    tempo_value_ = label(metrics, "--", 8, 0, 76, accent, &peepo_bold_18);
    cpu_value_ = label(metrics, "--", 92, 0, 80, text, &peepo_bold_18);
    for (auto* value : {tempo_value_, cpu_value_}) {
        lv_obj_set_height(value, 22); lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    }
    // The tempo reading opens its page, wherever it is shown.
    if (page_ != Page::Tempo && page_ != Page::Halting && page_ != Page::NamePatch) {
        lv_obj_add_flag(tempo_value_, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_ext_click_area(tempo_value_, 14);
        bind(tempo_value_, Action::Navigate, 0, {}, static_cast<int>(Page::Tempo));
    }
    for (auto* caption : {label(metrics, "BPM", 8, 22, 76, faint, &peepo_mono_14),
                          label(metrics, engine_.capabilities().simulation ? "CPU SIM" : "CPU", 92, 22, 80, faint, &peepo_mono_14)})
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_RIGHT, 0);
    update_metrics();
    lv_obj_update_layout(screen_);
}
void Application::patch_view(lv_obj_t* parent) {
    canvas_ = parent;
    lv_obj_add_flag(parent, LV_OBJ_FLAG_PRESS_LOCK);
    bind(parent, Action::Pan);
    for (int y = 12; y < 350; y += 24) for (int x = 24; x < 790; x += 24) {
        auto* dot = panel(parent, x, y, 2, 2, 0x1f1f1f); lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        dots_.push_back(dot);
    }
    for (const auto& edge : graph_.connections) {
        const bool selected = selected_cable_ && *selected_cable_ == edge;
        const auto routing = expand_connection(graph_, edge);
        // Draw every channel: stereo to stereo is two lines, mono to stereo
        // fans out to both connectors, stereo to mono lands on one.
        unsigned out_channels = 1, in_channels = 1; bool cv = false;
        if (const auto* source = find_module(graph_, edge.from)) for (const auto& p : module_ports(*source))
            if (p.symbol == edge.output && p.direction == Direction::Output) { out_channels = p.channels; cv = p.type == PortType::Cv; }
        if (const auto* sink = find_module(graph_, edge.to)) for (const auto& p : module_ports(*sink))
            if (p.symbol == edge.input && p.direction == Direction::Input) in_channels = p.channels;
        for (unsigned channel = 0; channel < std::max(out_channels, in_channels); ++channel) {
        auto* line = lv_line_create(parent);
        lv_obj_set_style_line_color(line, color(selected || !routing.error.empty() ? warning : cv ? control_ink : cable_ink), 0);
        lv_obj_set_style_line_width(line, selected ? 4 : 2, 0);
        lv_obj_add_flag(line, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(line, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_set_ext_click_area(line, 12);
        bind(line, Action::SelectCable, 0, {}, static_cast<int>(cables_.size()));
        cables_.push_back({line, edge, {}, routing, out_channels == 1 ? 0 : channel, in_channels == 1 ? 0 : channel, cv});
        }
    }
    for (const auto& m : graph_.modules) {
        const auto ink = module_ink(m);
        // A browser card in miniature: dark, outlined in the group colour, a white name.
        auto* card = panel(parent, m.x - pan_x_, m.y - pan_y_, module_width, module_height);
        lv_obj_set_style_border_color(card, color(ink), 0); lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_opa(card, static_cast<lv_opa_t>(m.bypassed ? LV_OPA_40 : LV_OPA_COVER), 0);
        lv_obj_set_style_bg_color(card, color(m.bypassed ? background : surface), 0);
        lv_obj_set_style_radius(card, 6, 0);
        // Encoder 1's highlight.
        if (m.id == patch_focus_) {
            lv_obj_set_style_outline_width(card, 2, 0); lv_obj_set_style_outline_color(card, color(text), 0);
            lv_obj_set_style_outline_pad(card, 4, 0);
        }
        // Unseen on black, but it widens where touches reach the jacks that
        // stick out past the edge: LVGL looks that far beyond the square.
        lv_obj_set_style_shadow_width(card, 14, 0); lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
        lv_obj_set_style_shadow_color(card, color(background), 0);
        auto* name = label(card, upper(m.name), 0, 0, module_width - 8, m.bypassed ? faint : text, &peepo_bold_14);
        lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_update_layout(name);
        if (lv_obj_get_height(name) > 36) {
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_height(name, 36);
        }
        lv_obj_align(name, LV_ALIGN_CENTER, 0, 0);
        const auto& ports = module_ports(m);
        // The stomps that act on it, numbered in a white dot on its top right
        // corner, clear of the name and of the connectors.
        if (const auto stomps = stomps_on(graph_, m); !stomps.empty() && !m.fixed) {
            std::string digits;
            for (const auto n : stomps) digits += (digits.empty() ? "" : " ") + std::to_string(n);
            const int width = 20 + 14 * (static_cast<int>(stomps.size()) - 1);
            auto* badge = panel(card, module_width + 6 - width, -12, width, 20, m.bypassed ? faint : text);
            lv_obj_set_style_radius(badge, 10, 0); lv_obj_set_style_border_width(badge, 0, 0);
            lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);
            auto* number = label(badge, digits, 0, 1, width, background, &peepo_mono_14);
            lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
        }
        lv_obj_add_flag(card, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_PRESS_LOCK);
        bind(card, Action::Module, m.id);
        module_widgets_[m.id] = card;
        for (const auto& port : ports) {
            if (port.type != PortType::Audio) continue;
            const bool output = port.direction == Direction::Output;
            for (unsigned channel = 0; channel < port.channels; ++channel) {
            auto* hit = lv_obj_create(card); lv_obj_remove_style_all(hit);
            const auto place = *jack(m, port.symbol, port.direction, channel);
            lv_obj_set_size(hit, 44, place.height); lv_obj_set_pos(hit, output ? module_width - 22 : -22, place.centre - place.height / 2);
            lv_obj_remove_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(hit, LV_OBJ_FLAG_PRESS_LOCK);
            auto* dot = panel(hit, 15, place.height / 2 - 7, 14, 14, background);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(dot, 3, 0); lv_obj_set_style_border_color(dot, color(ink), 0);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            // The connector's channel rides above the direction bit.
            bind(hit, Action::Port, m.id, port.symbol, (output ? 1 : 0) | static_cast<int>(channel << 3));
            if (!output) input_jacks_.emplace(std::pair{m.id, port.symbol}, dot);
            }
        }
        // Control connectors: one square socket for all control outputs on top,
        // one for all control inputs underneath.
        for (const bool output : {false, true}) {
            if (cv_ports(m, output ? Direction::Output : Direction::Input).empty()) continue;
            auto* hit = lv_obj_create(card); lv_obj_remove_style_all(hit);
            lv_obj_set_size(hit, 44, 44); lv_obj_set_pos(hit, module_width / 2 - 22, output ? -22 : module_height - 22);
            lv_obj_remove_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(hit, LV_OBJ_FLAG_PRESS_LOCK);
            auto* dot = panel(hit, 15, 15, 14, 14, background);
            lv_obj_set_style_radius(dot, 3, 0);
            lv_obj_set_style_border_width(dot, 3, 0); lv_obj_set_style_border_color(dot, color(control_ink), 0);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            bind(hit, Action::Port, m.id, {}, (output ? 1 : 0) | 4);
            if (!output) cv_inputs_[m.id] = dot;
        }
    }
    update_viewport();
}
void Application::update_viewport() {
    for (const auto& m : graph_.modules) {
        const auto it = module_widgets_.find(m.id);
        if (it != module_widgets_.end()) lv_obj_set_pos(it->second, m.x - pan_x_, m.y - pan_y_);
    }
    std::size_t i = 0;
    for (int y = 12; y < 350; y += 24) for (int x = 24; x < 790; x += 24)
        lv_obj_set_pos(dots_[i++], x - pan_x_ % 24, y - pan_y_ % 24);
    update_cables();
}
void Application::indicate_targets(bool active) {
    for (const auto& [id, dot] : cv_inputs_) {
        lv_obj_set_style_outline_color(dot, color(control_ink), 0);
        lv_obj_set_style_outline_width(dot, active && cable_cv_ && id != cable_source_ ? 4 : 0, 0);
        lv_obj_set_style_outline_opa(dot, LV_OPA_COVER, 0);
    }
    for (const auto& [key, jack] : input_jacks_) {
        const auto& [id, symbol] = key;
        const auto* m = find_module(graph_, id); if (!m) continue;
        const bool valid = active && !cable_cv_ && validate_connection(graph_, {cable_source_, id, cable_port_, symbol}).empty();
        lv_obj_set_style_border_color(jack, color(active ? (valid ? accent : border) : module_ink(*m)), 0);
        lv_obj_set_style_outline_color(jack, color(accent), 0);
        lv_obj_set_style_outline_width(jack, valid ? 4 : 0, 0);
        lv_obj_set_style_outline_opa(jack, LV_OPA_COVER, 0);
    }
}
std::optional<std::pair<int, int>> Application::vacant_slot() const {
    // Prefer the current viewport, then search the larger canvas.
    for (int visible = 1; visible >= 0; --visible)
        for (int y = module_min_y; y <= module_max_y; y += 120)
            for (int x = module_min_x; x <= module_max_x; x += 144) {
                if (visible && (x < pan_x_ + 24 || x + module_width > pan_x_ + 776 || y < pan_y_ + 12 || y + module_height > pan_y_ + 360)) continue;
                const bool occupied = std::any_of(graph_.modules.begin(), graph_.modules.end(), [&](const Module& m) {
                    return std::abs(m.x - x) < 120 && std::abs(m.y - y) < 110;
                });
                if (!occupied) return std::pair{x, y};
            }
    return {};
}
void Application::reveal(int x, int y) {
    if (x < pan_x_ + 24 || x + module_width > pan_x_ + 776) pan_x_ = std::clamp(x - 24, 0, canvas_width - 800);
    if (y < pan_y_ + 12 || y + module_height > pan_y_ + 360) pan_y_ = std::clamp(y - 12, 0, canvas_height - 360);
}
void Application::update_cables() {
    // LVGL positions are lazy; resolve the cards before reading coordinates.
    if (canvas_) lv_obj_update_layout(canvas_);
    for (auto& c : cables_) {
        auto a = module_widgets_.find(c.connection.from), b = module_widgets_.find(c.connection.to);
        if (a == module_widgets_.end() || b == module_widgets_.end()) continue;
        if (c.cv) {
            // Top of the source to the underside of the destination.
            sample_cable(c.line, c.points, lv_obj_get_x(a->second) + module_width / 2, lv_obj_get_y(a->second), lv_obj_get_x(b->second) + module_width / 2, lv_obj_get_y(b->second) + module_height, true);
            continue;
        }
        const auto* source = find_module(graph_, c.connection.from); const auto* sink = find_module(graph_, c.connection.to);
        const auto out = source ? jack(*source, c.connection.output, Direction::Output, c.out_channel) : std::nullopt;
        const auto in = sink ? jack(*sink, c.connection.input, Direction::Input, c.in_channel) : std::nullopt;
        sample_cable(c.line, c.points, lv_obj_get_x(a->second) + module_width, lv_obj_get_y(a->second) + (out ? out->centre : module_height / 2),
                     lv_obj_get_x(b->second), lv_obj_get_y(b->second) + (in ? in->centre : module_height / 2), false);
    }
}
void Application::browser(lv_obj_t* header, lv_obj_t* content, lv_obj_t* foot) {
    const auto& groups = module_groups();
    std::vector<std::vector<const ModuleDefinition*>> members(groups.size());
    for (const auto& d : catalogue()) if (d.category != "Endpoint" && !d.hidden) members[module_group(d.category)].push_back(&d);
    // Usable modules first, each part alphabetical.
    for (auto& entries : members)
        std::stable_sort(entries.begin(), entries.end(), [](const ModuleDefinition* a, const ModuleDefinition* b) {
            const bool ua = a->availability == Availability::Available, ub = b->availability == Availability::Available;
            return ua != ub ? ua : a->name < b->name;
        });
    // A group the catalogue leaves empty has no pill; a filter on one shows all.
    browser_groups_ = {std::string()};
    for (std::size_t i = 0; i < groups.size(); ++i) if (!members[i].empty()) browser_groups_.push_back(groups[i].name);
    if (std::find(browser_groups_.begin(), browser_groups_.end(), browser_group_) == browser_groups_.end()) browser_group_.clear();
    auto* group = lv_group_get_default();

    // Header: the group pills, as wide as their names, to the right.
    auto* pills = lv_obj_create(header); lv_obj_remove_style_all(pills);
    lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(pills, 8, 0);
    lv_obj_remove_flag(pills, LV_OBJ_FLAG_CLICKABLE);
    for (const auto& name : browser_groups_) {
        const auto* g = name.empty() ? nullptr : &*std::find_if(groups.begin(), groups.end(), [&](const ModuleGroup& x) { return name == x.name; });
        const bool open = name == browser_group_;
        auto* pill = lv_button_create(pills); lv_obj_remove_style_all(pill);
        lv_obj_set_size(pill, LV_SIZE_CONTENT, 36); lv_obj_set_style_pad_hor(pill, 16, 0);
        lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(pill, color(open ? text : g ? g->fill : border), 0); lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_opa(pill, LV_OPA_80, LV_STATE_PRESSED);
        lv_obj_set_style_outline_color(pill, color(text), LV_STATE_FOCUS_KEY); lv_obj_set_style_outline_width(pill, 2, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_pad(pill, 2, LV_STATE_FOCUS_KEY);
        auto* l = label(pill, name.empty() ? "ALL" : name, 0, 0, 0, open ? (g ? g->fill : 0x2563eb) : text, &peepo_bold_14);
        lv_obj_set_style_text_letter_space(l, 1, 0); lv_obj_center(l);
        bind(pill, Action::BrowserGroup, 0, name);
        if (group) lv_group_add_obj(group, pill);
    }
    lv_obj_align(pills, LV_ALIGN_RIGHT_MID, -14, 0);

    // Content: two columns of roomy cards to scroll.
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_pad_bottom(content, 12, 0);
    browser_list_ = content;
    browser_order_.clear();
    // An open group with several finer categories offers them as a row of
    // chips above its cards, in the order the group lists them.
    std::vector<std::string> categories;
    if (!browser_group_.empty()) {
        const auto gi = static_cast<std::size_t>(std::find_if(groups.begin(), groups.end(), [&](const ModuleGroup& x) { return browser_group_ == x.name; }) - groups.begin());
        for (const auto& name : groups[gi].categories)
            if (std::any_of(members[gi].begin(), members[gi].end(), [&](const ModuleDefinition* d) { return d->category == name; })) categories.push_back(name);
        for (const auto* d : members[gi])
            if (std::find(categories.begin(), categories.end(), d->category) == categories.end()) categories.push_back(d->category);
    }
    if (categories.size() < 2 || std::find(categories.begin(), categories.end(), browser_category_) == categories.end()) browser_category_.clear();
    int top = 8;
    if (categories.size() > 1) {
        const auto& g = *std::find_if(groups.begin(), groups.end(), [&](const ModuleGroup& x) { return browser_group_ == x.name; });
        auto* chips = lv_obj_create(content); lv_obj_remove_style_all(chips);
        lv_obj_set_pos(chips, 14, 8); lv_obj_set_size(chips, 772, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW_WRAP); lv_obj_set_style_pad_column(chips, 8, 0); lv_obj_set_style_pad_row(chips, 8, 0);
        lv_obj_remove_flag(chips, LV_OBJ_FLAG_CLICKABLE);
        categories.insert(categories.begin(), std::string());
        for (const auto& name : categories) {
            const bool open = name == browser_category_;
            auto* chip = lv_button_create(chips); lv_obj_remove_style_all(chip);
            lv_obj_set_size(chip, LV_SIZE_CONTENT, 40); lv_obj_set_style_pad_hor(chip, 16, 0);
            lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(chip, color(open ? g.bright : surface), 0); lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_opa(chip, LV_OPA_80, LV_STATE_PRESSED);
            lv_obj_set_style_border_width(chip, open ? 0 : 1, 0); lv_obj_set_style_border_color(chip, color(rim), 0);
            lv_obj_set_style_outline_color(chip, color(text), LV_STATE_FOCUS_KEY); lv_obj_set_style_outline_width(chip, 2, LV_STATE_FOCUS_KEY);
            auto* l = label(chip, name.empty() ? std::string("ALL") : upper(name), 0, 0, 0, open ? background : g.bright, &peepo_bold_14);
            lv_obj_set_style_text_letter_space(l, 1, 0); lv_obj_center(l);
            bind(chip, Action::BrowserCategory, 0, name);
            if (group) lv_group_add_obj(group, chip);
        }
        lv_obj_update_layout(chips);
        top = 8 + lv_obj_get_height(chips) + 12;
    }
    int shown = 0;
    for (std::size_t gi = 0; gi < groups.size(); ++gi) {
        if (!browser_group_.empty() && browser_group_ != groups[gi].name) continue;
        const auto& g = groups[gi];
        for (const auto* d : members[gi]) {
            if (!browser_category_.empty() && d->category != browser_category_) continue;
            const bool usable = d->availability == Availability::Available;
            auto* card = lv_button_create(content); lv_obj_remove_style_all(card);
            lv_obj_set_pos(card, shown % 2 ? 406 : 14, top + shown / 2 * 172); lv_obj_set_size(card, 380, 160);
            lv_obj_set_style_bg_color(card, color(surface), 0); lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(card, color(pressed_fill), LV_STATE_PRESSED);
            lv_obj_set_style_radius(card, 12, 0);
            lv_obj_set_style_border_width(card, 1, 0); lv_obj_set_style_border_color(card, color(border), 0);
            lv_obj_set_style_border_color(card, color(faint), LV_STATE_FOCUS_KEY);
            lv_obj_set_style_border_width(card, 2, LV_STATE_CHECKED); lv_obj_set_style_border_color(card, color(text), LV_STATE_CHECKED);
            lv_obj_set_style_border_width(card, 2, LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
            lv_obj_set_style_border_color(card, color(text), LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
            ++shown;
            // The finer category, as a tag in the group's colour.
            auto* tag = lv_obj_create(card); lv_obj_remove_style_all(tag);
            lv_obj_set_size(tag, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(tag, color(g.bright), 0); lv_obj_set_style_bg_opa(tag, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(tag, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_pad_hor(tag, 10, 0); lv_obj_set_style_pad_ver(tag, 3, 0);
            lv_obj_remove_flag(tag, LV_OBJ_FLAG_CLICKABLE);
            auto* category = label(tag, upper(d->category), 0, 0, 0, background, &peepo_bold_14);
            lv_obj_set_style_text_letter_space(category, 1, 0);
            lv_obj_align(tag, LV_ALIGN_TOP_RIGHT, -16, 18);
            lv_obj_update_layout(tag);
            auto* title = label(card, upper(d->name), 18, 18, 380 - 18 - 16 - lv_obj_get_width(tag) - 12, text, &peepo_bold_20);
            lv_label_set_long_mode(title, LV_LABEL_LONG_DOT); lv_obj_set_height(title, 26);
            // An entry that cannot be added says why instead of what it does.
            const auto about = usable ? d->description : "Needs setup: " + (d->availability_reason.empty() ? d->description : d->availability_reason);
            auto* description = label(card, about, 18, 54, 344, muted, &lv_font_montserrat_18);
            lv_label_set_long_mode(description, LV_LABEL_LONG_DOT); lv_obj_set_height(description, 92);
            if (usable) {
                bind(card, Action::BrowserPick, 0, d->uri);
                if (group) lv_group_add_obj(group, card);
                browser_cards_[d->uri] = card;
                browser_order_.push_back(d->uri);
            } else {
                // Not clickable, so a drag starting here still scrolls the list.
                lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_opa(card, LV_OPA_40, 0);
            }
        }
    }

    // Footer: back, what is in view, and add.
    slim(button(foot, "< Back", 14, 0, 112, Action::Navigate, 0, {}, static_cast<int>(Page::Patch)));
    const auto in_view = (browser_group_.empty() ? std::string("ALL") : browser_group_) + (browser_category_.empty() ? std::string() : " / " + upper(browser_category_));
    auto* count = label(foot, in_view + " // " + std::to_string(shown) + (shown == 1 ? " MODULE" : " MODULES"),
                        142, 15, 460, faint, &peepo_mono_14);
    lv_obj_set_style_text_letter_space(count, 2, 0); lv_obj_set_style_text_align(count, LV_TEXT_ALIGN_CENTER, 0);
    browser_add_ = primary(slim(button(foot, "Add module +", 618, 0, 168, Action::BrowserPick)));

    lv_obj_update_layout(content);
    lv_obj_scroll_to_y(content, browser_scroll_, LV_ANIM_OFF);
    // A pick the filter no longer shows is dropped.
    browser_pick(browser_cards_.count(browser_pick_) ? browser_pick_ : std::string(), false);
}
void Application::browser_pick(const std::string& uri, bool reveal) {
    if (const auto old = browser_cards_.find(browser_pick_); old != browser_cards_.end()) lv_obj_remove_state(old->second, LV_STATE_CHECKED);
    browser_pick_ = uri;
    if (const auto card = browser_cards_.find(uri); card != browser_cards_.end()) {
        lv_obj_add_state(card->second, LV_STATE_CHECKED);
        if (reveal) lv_obj_scroll_to_view(card->second, LV_ANIM_ON);
    }
    if (browser_add_) lv_obj_set_state(browser_add_, LV_STATE_DISABLED, browser_pick_.empty());
}
void Application::parameters(lv_obj_t* parent) {
    const auto* m = find_module(graph_, selected_);
    if (!m) { label(parent, "Module no longer exists", 24, 24); return; }
    if (!module_available(*m)) {
        button(parent, "Delete module", 24, 12, 200, Action::Remove, m->id);
        label(parent, "Module unavailable", 24, 92, 0, warning, &peepo_bold_24);
        label(parent, "Saved values and cables are retained for repair.\nParameters, bypass and new connections are disabled.\nYou can move or delete the placeholder.", 24, 136, 752, muted);
        auto* uri = label(parent, m->uri, 24, 244, 752, muted, &lv_font_montserrat_14);
        lv_label_set_long_mode(uri, LV_LABEL_LONG_DOT); return;
    }
    // Every control on one page, as bars in two columns; a long module scrolls.
    // A tuner shows its meter above them; Euclidean shows its four concentric rings beside its four track columns.
    const auto layout = module_layout(*m);
    if (layout == ModuleLayout::Euclidean) {
        euclidean_view(parent, *m);
        return;
    }
    const bool tuner = layout == ModuleLayout::Tuner;
    const int top = tuner ? 220 : 0;
    if (tuner) tuner_meter(parent);
    auto* list = lv_obj_create(parent); lv_obj_remove_style_all(list);
    lv_obj_set_pos(list, 0, top); lv_obj_set_size(list, 800, 360 - top);
    lv_obj_set_style_pad_hor(list, 20, 0); lv_obj_set_style_pad_top(list, tuner ? 4 : 16, 0); lv_obj_set_style_pad_bottom(list, 16, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(list, 16, 0); lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER); lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_CLICKABLE);
    // A bypassed module's controls still work, but read as not playing.
    if (m->bypassed) lv_obj_set_style_opa(list, LV_OPA_50, 0);
    parameter_list_ = list;
    const auto ink = module_ink(*m);
    if (const auto* d = definition(m->uri); d && !d->file.property.empty()) file_bar(list, *m);
    for (const auto& section : parameter_sections(*m)) {
        if (!section.name.empty()) {
            auto* heading = label(list, upper(section.name), 0, 0, 760, ink, &peepo_mono_14);
            lv_obj_set_style_text_letter_space(heading, 2, 0);
        }
        for (const auto* p : section.parameters) parameter_bar(list, *m, *p);
    }
    if (bars_.empty()) { label(list, "This module has no controls.", 0, 0, 760, muted); return; }
    if (!focus_symbol_.empty()) {
        for (std::size_t i = 0; i < bars_.size(); ++i) if (bars_[i].symbol == focus_symbol_) parameter_focus_ = i;
        focus_symbol_.clear();
    }
    if (parameter_focus_ >= bars_.size()) { parameter_focus_ = bars_.size() - 1; adjusting_ = false; }
    highlight(bars_[parameter_focus_].widget, true, adjusting_, bar_ink(*m, bars_[parameter_focus_].symbol));
    lv_obj_update_layout(list);
    lv_obj_scroll_to_y(list, parameter_scroll_, LV_ANIM_OFF);
}
void Application::tuner_meter(lv_obj_t* parent) {
    auto* meter = panel(parent, 24, 12, 752, 200);
    tuner_note_ = label(meter, "--", 0, 8, 752, text, &peepo_bold_48);
    lv_obj_set_style_text_align(tuner_note_, LV_TEXT_ALIGN_CENTER, 0);
    tuner_detail_ = label(meter, "-- Hz", 0, 64, 752, muted);
    lv_obj_set_style_text_align(tuner_detail_, LV_TEXT_ALIGN_CENTER, 0);
    tuner_status_ = label(meter, "Play one string", 0, 88, 752, muted);
    lv_obj_set_style_text_align(tuner_status_, LV_TEXT_ALIGN_CENTER, 0);
    // +/- 5 cents is the green target; the scale spans a whole semitone.
    auto* target = panel(meter, 346, 118, 60, 50, 0x14331f);
    lv_obj_set_style_border_width(target, 0, 0);
    for (int i = 0; i <= 10; ++i) {
        auto* mark = panel(meter, 76 + i * 60 - 1, i == 5 ? 119 : 131, 2, i == 5 ? 48 : 24, i == 5 ? accent : muted);
        lv_obj_set_style_border_width(mark, 0, 0);
    }
    label(meter, "-50  FLAT", 42, 174, 150, muted, &lv_font_montserrat_14);
    auto* centre = label(meter, "0", 346, 174, 60, accent, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(centre, LV_TEXT_ALIGN_CENTER, 0);
    auto* sharp = label(meter, "SHARP  +50", 560, 174, 150, muted, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(sharp, LV_TEXT_ALIGN_RIGHT, 0);
    tuner_needle_ = panel(meter, 372, 118, 8, 50, accent);
    lv_obj_set_style_border_width(tuner_needle_, 0, 0);
    lv_obj_remove_flag(tuner_needle_, LV_OBJ_FLAG_CLICKABLE);
    update_tuner();
}
void Application::update_tuner() {
    if (!tuner_note_) return;
    const auto* m = find_module(graph_, selected_);
    if (!m) return;
    const bool ready = engine_.state() == EngineState::Ready && !m->bypassed && !engine_.capabilities().simulation;
    const auto reading = tuning_from_frequency(ready ? engine_.tuner_frequency(m->id) : std::nullopt, parameter(*m, "tuning").value);
    std::string detail = "Connect IN to this tuner; play one string.";
    std::string status = engine_.capabilities().simulation ? "No audio in simulator" : !ready ? (m->bypassed ? "Tuner bypassed" : "Engine offline") : "Waiting for a note";
    if (reading) {
        std::ostringstream value;
        value << std::fixed << std::setprecision(1) << reading->frequency << " Hz   |   " << std::showpos << reading->cents << " cents";
        detail = value.str();
        status = std::abs(reading->cents) <= 5 ? "In tune" : reading->cents < 0 ? "Flat - tighten string" : "Sharp - loosen string";
        lv_obj_set_x(tuner_needle_, 372 + static_cast<int>(std::round(std::clamp(reading->cents, -50.0, 50.0) * 6)));
        lv_obj_remove_flag(tuner_needle_, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(tuner_needle_, LV_OBJ_FLAG_HIDDEN);
    const auto ink = reading ? (std::abs(reading->cents) <= 5 ? accent : warning) : muted;
    lv_obj_set_style_text_color(tuner_note_, color(ink), 0);
    lv_obj_set_style_text_color(tuner_status_, color(ink), 0);
    lv_obj_set_style_bg_color(tuner_needle_, color(ink), 0);
    const std::string note = reading ? reading->note : "--";
    if (note != lv_label_get_text(tuner_note_)) lv_label_set_text(tuner_note_, note.c_str());
    if (detail != lv_label_get_text(tuner_detail_)) lv_label_set_text(tuner_detail_, detail.c_str());
    if (status != lv_label_get_text(tuner_status_)) lv_label_set_text(tuner_status_, status.c_str());
}
void Application::euclidean_view(lv_obj_t* parent, const Module& m) {
    auto* viz = panel(parent, 20, 10, 340, 340);
    if (m.bypassed) lv_obj_set_style_opa(viz, LV_OPA_50, 0);
    auto* heading = label(viz, "RHYTHM", 16, 12, 120, faint, &peepo_mono_14);
    lv_obj_set_style_text_letter_space(heading, 2, 0);
    euclidean_status_ = label(viz, "", 136, 12, 188, muted, &peepo_mono_14);
    lv_obj_set_style_text_align(euclidean_status_, LV_TEXT_ALIGN_RIGHT, 0);

    // 12 o'clock downbeat tick mark and central hub inside the concentric rings.
    auto* downbeat = panel(viz, 169, 32, 2, 8, muted);
    lv_obj_set_style_border_width(downbeat, 0, 0);
    lv_obj_remove_flag(downbeat, LV_OBJ_FLAG_CLICKABLE);
    auto* hub = panel(viz, 170 - 12, 170 - 12, 24, 24, 0x141414);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_CLICKABLE);
    auto* center_dot = panel(viz, 170 - 2, 170 - 2, 4, 4, faint);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(center_dot, 0, 0);
    lv_obj_remove_flag(center_dot, LV_OBJ_FLAG_CLICKABLE);

    for (std::size_t t = 0; t < euclidean_tracks; ++t) {
        const auto& style = euclidean_styles[t];
        auto* ring = panel(viz, 170 - style.radius, 170 - style.radius, style.radius * 2, style.radius * 2, surface);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, color(style.ring_dim), 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        euclidean_rings_[t] = ring;

        auto* halo = panel(viz, 0, 0, 22, 22, style.bright);
        lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(halo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(halo, LV_OBJ_FLAG_CLICKABLE);
        euclidean_halos_[t] = halo;

        for (int s = 0; s < euclidean_max_steps; ++s) {
            auto* dot = panel(viz, 0, 0, 10, 10, surface);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            euclidean_dots_[t][static_cast<std::size_t>(s)] = dot;
        }

        auto* step_lbl = label(viz, "", 14 + static_cast<int>(t) * 80, 314, 76, style.bright, &peepo_mono_14);
        lv_obj_set_style_text_align(step_lbl, LV_TEXT_ALIGN_CENTER, 0);
        euclidean_step_labels_[t] = step_lbl;
    }

    for (std::size_t t = 0; t < euclidean_tracks; ++t) {
        const auto& style = euclidean_styles[t];
        const auto n = std::to_string(t + 1);
        auto* col = panel(parent, 372 + static_cast<int>(t) * 104, 10, 96, 340);
        if (m.bypassed) lv_obj_set_style_opa(col, LV_OPA_50, 0);
        auto make_tile = [&](const std::string& symbol, const std::string& title, int y, int h, bool is_header) {
            const auto& p = parameter(m, symbol);
            auto* bar = panel(col, 6, y, 84, h);
            lv_obj_set_style_radius(bar, 10, 0); lv_obj_set_style_clip_corner(bar, true, 0);
            lv_obj_set_style_border_post(bar, true, 0);
            lv_obj_add_flag(bar, LV_OBJ_FLAG_PRESS_LOCK);
            // Set explicit width before paint_bar reads it.
            lv_obj_update_layout(bar);
            auto* fill = panel(bar, 0, 0, 0, h, style.fill);
            lv_obj_set_style_radius(fill, 0, 0); lv_obj_set_style_border_width(fill, 0, 0);
            lv_obj_remove_flag(fill, LV_OBJ_FLAG_CLICKABLE);
            auto* name = label(bar, title, 8, is_header ? 10 : 8, 68, is_header ? style.bright : muted, &peepo_bold_14);
            lv_obj_set_style_text_letter_space(name, 1, 0);
            auto* value = label(bar, "", 8, is_header ? 32 : 36, 68, text, is_header ? &peepo_bold_18 : &peepo_bold_24);
            lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
            std::string pin;
            for (const auto& a : graph_.assignments)
                if (a.module == m.id && a.action == ControlAction::Parameter && a.parameter == p.symbol && assignment_target(graph_, a))
                    pin += (pin.empty() ? "" : " ") + std::string(is_encoder(a.control) ? "E" : "S") + a.control.substr(is_encoder(a.control) ? 3 : 5);
            if (!pin.empty()) {
                auto* badge = label(bar, pin, 8, is_header ? 10 : 8, 68, style.bright, &peepo_mono_14);
                lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_RIGHT, 0);
            }
            paint_bar(bar, p);
            bind(bar, Action::Parameter, m.id, p.symbol);
            bars_.push_back({m.id, p.symbol, bar});
            return bar;
        };
        make_tile("is_enabled" + n, "LOOP " + n, 6, 64, true);
        euclidean_tiles_[t][0] = make_tile("steps" + n, "STEPS", 78, 80, false);
        euclidean_tiles_[t][1] = make_tile("beats" + n, "BEATS", 166, 80, false);
        euclidean_tiles_[t][2] = make_tile("shift" + n, "SHIFT", 254, 80, false);
    }

    if (!focus_symbol_.empty()) {
        for (std::size_t i = 0; i < bars_.size(); ++i) if (bars_[i].symbol == focus_symbol_) parameter_focus_ = i;
        focus_symbol_.clear();
    }
    if (parameter_focus_ >= bars_.size()) { parameter_focus_ = bars_.size() - 1; adjusting_ = false; }
    highlight(bars_[parameter_focus_].widget, true, adjusting_, bar_ink(m, bars_[parameter_focus_].symbol));
    update_euclidean();
}
void Application::update_euclidean() {
    if (!euclidean_status_) return;
    const auto* m = find_module(graph_, selected_);
    if (!m) return;
    const bool ready = engine_.state() == EngineState::Ready && !m->bypassed && !engine_.capabilities().simulation;
    const auto live_steps = ready ? engine_.euclidean_steps(m->id) : EuclideanSteps{};
    bool any_live = false;
    for (const auto& s : live_steps) if (s.has_value()) any_live = true;
    const std::string status = engine_.capabilities().simulation ? "SIMULATED" :
        engine_.state() != EngineState::Ready ? "OFFLINE" :
        m->bypassed ? "BYPASSED" :
        any_live ? "LIVE" : "WAITING FOR TRIG";
    const auto status_ink = color(any_live ? accent : (m->bypassed || engine_.state() != EngineState::Ready) ? warning : muted);
    if (status != lv_label_get_text(euclidean_status_)) lv_label_set_text(euclidean_status_, status.c_str());
    if (!lv_color_eq(lv_obj_get_style_text_color(euclidean_status_, LV_PART_MAIN), status_ink))
        lv_obj_set_style_text_color(euclidean_status_, status_ink, 0);

    auto current_val = [&](const std::string& symbol) -> int {
        if (stream_ && stream_->id == m->id && stream_->symbol == symbol) return static_cast<int>(std::round(stream_->latest));
        return static_cast<int>(std::round(parameter(*m, symbol).value));
    };
    constexpr double pi = 3.14159265358979323846;
    for (std::size_t t = 0; t < euclidean_tracks; ++t) {
        const auto n = std::to_string(t + 1);
        const auto& style = euclidean_styles[t];
        const bool enabled = current_val("is_enabled" + n) > 0;
        const int steps = std::clamp(current_val("steps" + n), 0, euclidean_max_steps);
        const int beats = std::clamp(current_val("beats" + n), 0, euclidean_max_steps);
        const int shift = std::clamp(current_val("shift" + n), 0, euclidean_max_steps);
        const auto pattern = euclidean_pattern(steps, beats, shift);
        const auto active_step = (enabled && steps > 0 && live_steps[t] && *live_steps[t] >= 0)
            ? std::optional<int>{*live_steps[t] % steps} : std::nullopt;

        for (auto* tile : euclidean_tiles_[t])
            if (tile) lv_obj_set_style_opa(tile, static_cast<lv_opa_t>(enabled ? LV_OPA_COVER : LV_OPA_50), 0);

        if (auto* ring = euclidean_rings_[t]) {
            lv_obj_set_style_border_color(ring, color(enabled && steps > 0 ? style.ring_dim : 0x1c1c1c), 0);
            lv_obj_set_style_opa(ring, static_cast<lv_opa_t>(enabled ? LV_OPA_COVER : LV_OPA_40), 0);
        }

        auto* halo = euclidean_halos_[t];
        if (halo) lv_obj_add_flag(halo, LV_OBJ_FLAG_HIDDEN);

        for (int s = 0; s < euclidean_max_steps; ++s) {
            auto* dot = euclidean_dots_[t][static_cast<std::size_t>(s)];
            if (!dot) continue;
            if (!enabled || s >= steps) {
                lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
            const double angle = -pi / 2.0 + (2.0 * pi * static_cast<double>(s)) / static_cast<double>(steps);
            const int cx = 170 + static_cast<int>(std::round(static_cast<double>(style.radius) * std::cos(angle)));
            const int cy = 170 + static_cast<int>(std::round(static_cast<double>(style.radius) * std::sin(angle)));
            const bool beat = pattern[static_cast<std::size_t>(s)];
            const bool active = active_step && *active_step == s;
            const int size = active ? (beat ? (steps <= 16 ? 14 : 12) : (steps <= 16 ? 10 : 8))
                                    : beat ? (steps <= 20 ? 12 : 10) : (steps <= 24 ? 6 : 4);
            lv_obj_set_pos(dot, cx - size / 2, cy - size / 2);
            lv_obj_set_size(dot, size, size);
            if (active) {
                lv_obj_set_style_bg_color(dot, color(style.bright), 0);
                lv_obj_set_style_border_width(dot, beat ? 2 : 0, 0);
                lv_obj_set_style_border_color(dot, color(text), 0);
                if (halo) {
                    const int h_size = beat ? (steps <= 16 ? 24 : 20) : (steps <= 16 ? 18 : 14);
                    lv_obj_set_pos(halo, cx - h_size / 2, cy - h_size / 2);
                    lv_obj_set_size(halo, h_size, h_size);
                    lv_obj_set_style_bg_color(halo, color(style.bright), 0);
                    lv_obj_set_style_bg_opa(halo, static_cast<lv_opa_t>(beat ? LV_OPA_30 : LV_OPA_20), 0);
                    lv_obj_set_style_border_width(halo, beat ? 2 : 1, 0);
                    lv_obj_set_style_border_color(halo, color(style.bright), 0);
                    lv_obj_remove_flag(halo, LV_OBJ_FLAG_HIDDEN);
                }
            } else if (beat) {
                lv_obj_set_style_bg_color(dot, color(surface), 0);
                lv_obj_set_style_border_width(dot, 2, 0);
                lv_obj_set_style_border_color(dot, color(style.bright), 0);
            } else {
                lv_obj_set_style_bg_color(dot, color(0x3f3f46), 0);
                lv_obj_set_style_border_width(dot, 0, 0);
            }
        }

        if (auto* step_lbl = euclidean_step_labels_[t]) {
            std::string summary = "L" + n + " ";
            if (!enabled) summary += "OFF";
            else if (active_step) summary += std::to_string(*active_step + 1) + "/" + std::to_string(steps);
            else summary += std::to_string(std::min(beats, steps)) + ":" + std::to_string(steps);
            if (summary != lv_label_get_text(step_lbl)) lv_label_set_text(step_lbl, summary.c_str());
            lv_obj_set_style_text_color(step_lbl, color(enabled ? style.bright : faint), 0);
        }
    }
}
void Application::parameter_bar(lv_obj_t* parent, const Module& m, const Parameter& p) {
    auto* bar = panel(parent, 0, 0, bar_width, bar_height);
    lv_obj_set_style_radius(bar, 10, 0); lv_obj_set_style_clip_corner(bar, true, 0);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_PRESS_LOCK);
    auto* fill = panel(bar, 0, 0, 0, bar_height, module_fill(m));
    lv_obj_set_style_radius(fill, 0, 0); lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_CLICKABLE);
    // Under the name: the same time in milliseconds at the module's own tempo,
    // and the controls pinned to it.
    std::string caption;
    if (const auto* d = definition(m.uri); p.unit == beats_unit && d && !d->tempo_symbol.empty())
        for (const auto& t : m.parameters)
            if (t.symbol == d->tempo_symbol && t.value > 0) {
                std::ostringstream ms;
                ms << std::fixed << std::setprecision(0) << 60000 / t.value * p.value << " ms at " << t.value << " BPM";
                caption = ms.str();
            }
    for (const auto& a : graph_.assignments)
        if (a.module == m.id && a.action == ControlAction::Parameter && a.parameter == p.symbol && assignment_target(graph_, a))
            caption += (caption.empty() ? "" : "  ") + std::string(is_encoder(a.control) ? "E" : "S") + a.control.substr(is_encoder(a.control) ? 3 : 5);
    auto* name = label(bar, upper(p.name), 16, caption.empty() ? 15 : 6, 200, text, &peepo_bold_14);
    lv_obj_set_style_text_letter_space(name, 1, 0);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT); lv_obj_set_height(name, 18);
    auto* value = label(bar, "", 196, 13, 160, text, &peepo_bold_18);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT); lv_obj_set_height(value, 22);
    if (!caption.empty()) label(bar, caption, 16, 25, 200, muted, &peepo_mono_14);
    paint_bar(bar, p);
    bind(bar, Action::Parameter, m.id, p.symbol);
    bars_.push_back({m.id, p.symbol, bar});
}
void Application::file_bar(lv_obj_t* parent, const Module& m) {
    const auto& file = definition(m.uri)->file;
    auto* bar = panel(parent, 0, 0, bar_width, bar_height);
    lv_obj_set_style_radius(bar, 10, 0);
    // Children as on a parameter bar: an empty fill, the name, the value.
    lv_obj_remove_flag(panel(bar, 0, 0, 0, bar_height), LV_OBJ_FLAG_CLICKABLE);
    auto* name = label(bar, upper(file.label), 16, 15, 120, text, &peepo_bold_14);
    lv_obj_set_style_text_letter_space(name, 1, 0);
    // The file's name only: where it lives says nothing on a small screen.
    auto* chosen = label(bar, m.file.empty() ? "None chosen" : file_name(m.file), 136, 13, 220, m.file.empty() ? faint : text, &peepo_bold_18);
    lv_obj_set_style_text_align(chosen, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(chosen, LV_LABEL_LONG_DOT); lv_obj_set_height(chosen, 22);
    if (files_ && engine_.state() == EngineState::Ready) bind(bar, Action::ChooseFile, m.id);
    else { lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_opa(bar, LV_OPA_40, 0); }
    bars_.push_back({m.id, {}, bar, true});
}
void Application::focus_parameter(std::size_t index) {
    if (bars_.empty()) return;
    index = std::min(index, bars_.size() - 1);
    const auto* m = find_module(graph_, bars_[index].id);
    if (!m) return;
    if (parameter_focus_ < bars_.size()) highlight(bars_[parameter_focus_].widget, false, false, 0);
    parameter_focus_ = index; adjusting_ = false;
    highlight(bars_[index].widget, true, false, bar_ink(*m, bars_[index].symbol));
    lv_obj_scroll_to_view(bars_[index].widget, LV_ANIM_OFF);
    if (bars_[index].file) { message(definition(m->uri)->file.label + ": push to choose a file."); return; }
    const auto& p = parameter(*m, bars_[index].symbol);
    message(p.name + " " + format_value(p));
}
void Application::activate_parameter() {
    if (parameter_focus_ >= bars_.size()) return;
    const auto& bar = bars_[parameter_focus_];
    const auto* m = find_module(graph_, bar.id);
    if (!m) return;
    if (bar.file) {
        if (files_ && engine_.state() == EngineState::Ready) { selected_ = m->id; file_page_ = 0; navigate(Page::FilePicker); }
        else message("Files are not available here.");
        return;
    }
    const auto& p = parameter(*m, bar.symbol);
    if (p.kind == ParameterKind::Toggle || p.kind == ParameterKind::Enumeration || is_euclidean_toggle(p)) { submit(SetParameter{m->id, p.symbol, stepped_choice(p)}); return; }
    // With one encoder, its push switches between choosing a bar and turning it.
    adjusting_ = !adjusting_;
    highlight(bar.widget, true, adjusting_, bar_ink(*m, bar.symbol));
    message(adjusting_ ? "Encoder 1 turns " + p.name + "; push it again to move on." : "Encoder 1 picks a control again.");
}
void Application::open_module(InstanceId id) {
    // The same module again keeps its place; another starts at the top.
    if (id != selected_) { parameter_focus_ = 0; parameter_scroll_ = 0; }
    selected_ = id; patch_focus_ = id; adjusting_ = false;
    message("Drag a bar to set it. E1 picks a bar, E2 turns it.");
    navigate(Page::Parameters);
}
Page Application::back_page() const {
    // Back returns to where the page was opened from.
    switch (page_) {
    case Page::NamePatch: return Page::Patches;
    case Page::Controls: case Page::Diagnostics: case Page::Power: return Page::Settings;
    case Page::AssignPicker: case Page::AssignmentDetail: return Page::Controls;
    case Page::FilePicker: return Page::Parameters;
    default: return Page::Patch;
    }
}
void Application::turn_parameter(InstanceId id, const std::string& symbol, const Gesture& g, const Assignment* pin) {
    const auto* m = find_module(graph_, id);
    const Parameter* p = nullptr;
    if (m) for (const auto& candidate : m->parameters) if (candidate.symbol == symbol) p = &candidate;
    if (!p) return;
    // Continue from a value still on its way to the engine, not the last acknowledged one.
    const bool streaming = stream_ && stream_->id == id && stream_->symbol == symbol;
    const auto current = streaming ? stream_->latest : p->value;
    // A time in beats turns from one note value to the next; fine turning is free.
    const auto next = p->unit == beats_unit && !g.fine
        ? step_note(current, pin && pin->inverted ? -g.steps : g.steps, pin ? pin->minimum : p->minimum, pin ? pin->maximum : p->maximum)
        : turned_value(*p, current, g.steps, g.fine, g.interval_ms, pin);
    auto shown = *p; shown.value = next;
    message(control_label(g.control) + (g.fine ? " (fine): " : ": ") + m->name + " / " + p->name + " " + format_value(shown) + (next == current ? " (limit)" : ""));
    if (next == current) return;
    // The bar follows at once; the engine's answer confirms it.
    for (const auto& bar : bars_) if (bar.id == id && bar.symbol == symbol) paint_bar(bar.widget, shown);
    if ((p->kind == ParameterKind::Continuous || p->kind == ParameterKind::Integer) && !is_euclidean_toggle(*p)) stream_parameter(id, symbol, next);
    else submit(SetParameter{id, symbol, next});
    update_euclidean();
}
void Application::focus_module(int steps) {
    // In signal order: left to right, then top to bottom.
    std::vector<const Module*> order;
    for (const auto& m : graph_.modules) if (!m.fixed) order.push_back(&m);
    if (order.empty()) { message("No modules yet. Add one with + Add."); return; }
    std::stable_sort(order.begin(), order.end(), [](const Module* a, const Module* b) { return a->x != b->x ? a->x < b->x : a->y < b->y; });
    const auto last = static_cast<int>(order.size()) - 1;
    const auto it = std::find_if(order.begin(), order.end(), [&](const Module* m) { return m->id == patch_focus_; });
    const int at = it == order.end() ? (steps > 0 ? -1 : last + 1) : static_cast<int>(it - order.begin());
    const auto* m = order[static_cast<std::size_t>(std::clamp(at + steps, 0, last))];
    patch_focus_ = m->id;
    reveal(m->x, m->y);
    message(m->name + ": push encoder 1 to open it.");
    rebuild_ = true;
}
void Application::set_tempo(double bpm) {
    // Whole beats per minute: what a module's BPM control needs, and what the screen shows.
    bpm = std::round(std::clamp(bpm, minimum_tempo, maximum_tempo));
    if (bpm == (tempo_sent_ ? *tempo_sent_ : graph_.tempo)) return;
    if (submit(SetTempo{bpm})) tempo_sent_ = bpm;
}
void Application::tap_tempo() {
    const auto bpm = tapper_.tap(clock_ms_);
    if (bpm) { set_tempo(*bpm); message("Tap tempo: " + metric_text(*bpm, false) + " BPM."); }
    else message(tapper_.taps() < 2 ? "Tap tempo: keep tapping." : "Tap tempo: too fast or too slow; keep tapping.");
    if (page_ == Page::Tempo) rebuild_ = true;
}
void Application::tempo(lv_obj_t* parent) {
    const auto shown = tempo_sent_ ? *tempo_sent_ : graph_.tempo;
    auto* card = panel(parent, 24, 12, 752, 150);
    label(card, "PATCH TEMPO", 24, 18, 300, faint, &peepo_mono_14);
    auto* value = label(card, metric_text(shown, false), 24, 44, 260, accent, &peepo_bold_48);
    label(card, "BPM", 24, 104, 100, muted, &peepo_bold_14);
    for (int i = 0; i < 4; ++i) {
        static const int steps[] = {-5, -1, 1, 5};
        auto* step = button(card, (steps[i] > 0 ? "+" : "") + std::to_string(steps[i]), 300 + i * 108, 24, 96, Action::TempoStep, 0, {}, steps[i]);
        lv_obj_set_height(step, 44);
    }
    auto* slider = lv_slider_create(card); lv_obj_set_pos(slider, 310, 100); lv_obj_set_size(slider, 410, 12);
    lv_slider_set_range(slider, static_cast<int>(minimum_tempo), static_cast<int>(maximum_tempo));
    lv_slider_set_value(slider, static_cast<int>(std::round(shown)), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, color(border), LV_PART_MAIN); lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, color(accent), LV_PART_INDICATOR); lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, color(text), LV_PART_KNOB); lv_obj_set_style_pad_all(slider, 10, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB); lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_ext_click_area(slider, 20);
    bind(slider, Action::TempoSlider, 0, {}, 0, value);
    if (auto* group = lv_group_get_default()) lv_group_add_obj(group, slider);
    // A big pad to tap on the beat, as a footswitch set to Tap tempo does.
    auto* tap = primary(button(parent, "Tap", 24, 178, 360, Action::Tap));
    lv_obj_set_height(tap, 150);
    std::string following;
    unsigned count = 0;
    for (const auto& m : graph_.modules)
        if (const auto* d = definition(m.uri); d && !d->tempo_symbol.empty() && module_available(m)) { following += (count++ ? ", " : "") + m.name; }
    label(parent, count ? "Follow the tempo" : "Nothing follows it yet", 408, 186, 368, text, &peepo_bold_20);
    auto* who = label(parent, count ? following : "Modules with a tempo, such as Chorus, Flanger or the sequencers, take it when they are added.",
                      408, 222, 368, muted, &lv_font_montserrat_14);
    lv_label_set_long_mode(who, LV_LABEL_LONG_DOT); lv_obj_set_height(who, 60);
    label(parent, "Saved with the patch. Encoder 1 turns it; a footswitch can tap it.", 408, 292, 368, faint, &lv_font_montserrat_14);
}
void Application::file_picker(lv_obj_t* parent) {
    const auto* m = find_module(graph_, selected_);
    const auto* d = m ? definition(m->uri) : nullptr;
    if (!d || d->file.property.empty() || !files_) { label(parent, "Nothing to choose here.", 24, 24, 752, muted); return; }
    std::string types;
    for (const auto& e : d->file.extensions) types += (types.empty() ? "." : ", .") + e;
    const auto found = files_->list(d->file.folder, d->file.extensions);
    if (found.empty()) {
        label(parent, "No " + types + " files yet", 24, 20, 752, text, &peepo_bold_24);
        auto* where = label(parent, "Copy them into " + (files_->root() / d->file.folder).generic_string() + "\n(for example over SSH), then open this page again.", 24, 68, 752, muted);
        lv_label_set_long_mode(where, LV_LABEL_LONG_WRAP);
        return;
    }
    constexpr std::size_t per_page = 8;
    const auto pages = (found.size() + per_page - 1) / per_page;
    file_page_ = std::min(file_page_, pages - 1);
    auto* prompt = label(parent, std::to_string(found.size()) + (found.size() == 1 ? " file" : " files") + " in " + d->file.folder + "/", 24, 18, 560, muted, &lv_font_montserrat_14);
    lv_label_set_long_mode(prompt, LV_LABEL_LONG_DOT); lv_obj_set_height(prompt, 18);
    if (pages > 1) {
        auto* previous = button(parent, "<", 600, 4, 48, Action::FilePage, 0, {}, static_cast<int>(file_page_ ? file_page_ - 1 : 0));
        if (!file_page_) lv_obj_add_state(previous, LV_STATE_DISABLED);
        label(parent, std::to_string(file_page_ + 1) + " / " + std::to_string(pages), 664, 18, 56, muted, &lv_font_montserrat_14);
        auto* next = button(parent, ">", 728, 4, 48, Action::FilePage, 0, {}, static_cast<int>(std::min(file_page_ + 1, pages - 1)));
        if (file_page_ + 1 >= pages) lv_obj_add_state(next, LV_STATE_DISABLED);
    }
    const auto ink = module_ink(*m);
    for (std::size_t i = file_page_ * per_page; i < std::min(found.size(), file_page_ * per_page + per_page); ++i) {
        const auto slot = i - file_page_ * per_page;
        auto* choice = button(parent, found[i].name, 24 + static_cast<int>(slot % 2) * 384, 64 + static_cast<int>(slot / 2) * 68, 368, Action::PickFile, m->id, found[i].engine_path);
        // File names keep their case, and long ones end in dots.
        if (auto* title = lv_obj_get_child(choice, 0)) {
            lv_label_set_text(title, found[i].name.c_str());
            lv_label_set_long_mode(title, LV_LABEL_LONG_DOT); lv_obj_set_width(title, 340); lv_obj_center(title);
        }
        if (found[i].engine_path == m->file) mark(choice, ink);
    }
}
void Application::confirm_action(lv_obj_t* parent) {
    auto* heading = label(parent, confirmation_.heading, 24, 44, 752, accent, &peepo_bold_24);
    lv_label_set_long_mode(heading, LV_LABEL_LONG_DOT); lv_obj_set_height(heading, 32);
    label(parent, confirmation_.body, 24, 104, 752, muted);
    button(parent, "Cancel", 24, 268, 200, Action::CancelAction);
    primary(button(parent, confirmation_.button, 552, 268, 224, Action::ConfirmAction));
}
void Application::stored_patches(lv_obj_t* parent) {
    constexpr std::size_t rows = 3;
    label(parent, "Saved patches", 24, 16, 0, text, &peepo_bold_24);
    const auto pages = std::max<std::size_t>(1, (stored_.size() + rows - 1) / rows);
    patch_page_ = std::min(patch_page_, pages - 1);
    if (pages > 1) {
        auto* previous = button(parent, "<", 600, 4, 48, Action::PatchPage, 0, {}, static_cast<int>(patch_page_ ? patch_page_ - 1 : 0));
        if (!patch_page_) lv_obj_add_state(previous, LV_STATE_DISABLED);
        label(parent, std::to_string(patch_page_ + 1) + " / " + std::to_string(pages), 664, 18, 56, muted, &lv_font_montserrat_14);
        auto* next = button(parent, ">", 728, 4, 48, Action::PatchPage, 0, {}, static_cast<int>(std::min(patch_page_ + 1, pages - 1)));
        if (patch_page_ + 1 >= pages) lv_obj_add_state(next, LV_STATE_DISABLED);
    }
    if (stored_.empty()) label(parent, "Nothing saved yet. Save keeps this patch on disk, with its\nplugin state, and survives restarting the app and the engine.", 24, 72, 752, muted);
    for (std::size_t i = patch_page_ * rows; i < std::min(stored_.size(), patch_page_ * rows + rows); ++i) {
        const auto& patch = stored_[i];
        auto* row = panel(parent, 24, 60 + static_cast<int>(i - patch_page_ * rows) * 64, 752, 56);
        const bool playing = patch.id == graph_.patch_id;
        if (playing) lv_obj_set_style_border_color(row, color(accent), 0);
        auto* name = label(row, patch.name, 16, 6, 424, text);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT); lv_obj_set_height(name, 22);
        std::string detail = patch.revision ? "Revision " + std::to_string(patch.revision) + " / " + std::to_string(patch.bundle.blocks.size()) + " modules" : "Unreadable";
        if (patch.startup) detail += " / loads at startup";
        if (playing) detail += graph_.dirty ? " / playing, unsaved edits" : " / playing";
        if (!patch.missing_files.empty()) detail += " / " + std::to_string(patch.missing_files.size()) + " missing files";
        if (!patch.issue.empty()) detail += " / " + patch.issue;
        auto* info = label(row, detail, 16, 30, 424, patch.issue.empty() && patch.missing_files.empty() ? muted : warning, &lv_font_montserrat_14);
        lv_label_set_long_mode(info, LV_LABEL_LONG_DOT); lv_obj_set_height(info, 18);
        auto* load = button(row, "Load", 448, 3, 96, Action::LoadStored, 0, patch.id);
        if (!patch.revision) lv_obj_add_state(load, LV_STATE_DISABLED);
        auto* startup = button(row, "Startup", 552, 3, 104, Action::StartupStored, 0, patch.id);
        if (patch.startup) { lv_obj_set_style_border_color(startup, color(accent), 0); lv_obj_set_style_text_color(lv_obj_get_child(startup, 0), color(accent), 0); }
        button(row, "Delete", 664, 3, 80, Action::DeleteStored, 0, patch.id);
    }
    const std::string playing = graph_.patch_id.empty() ? "never saved" : graph_.dirty ? "unsaved edits since its last save" : "saved";
    auto* now = label(parent, "Playing: " + graph_.name + " / " + playing, 24, 252, 752, muted, &lv_font_montserrat_14);
    lv_label_set_long_mode(now, LV_LABEL_LONG_DOT); lv_obj_set_height(now, 18);
    primary(button(parent, "Save", 24, 280, 140, Action::SavePatch, 0, {}, 0));
    button(parent, "Save as", 176, 280, 140, Action::SavePatch, 0, {}, 1);
    button(parent, "New patch", 328, 280, 140, Action::NameEntry, 0, {}, 0);
    button(parent, "Rename", 480, 280, 140, Action::NameEntry, 0, {}, 1);
    if (engine_.capabilities().simulation) button(parent, "Parallel demo", 632, 280, 144, Action::ParallelDemo);
}
void Application::patches(lv_obj_t* parent) {
    if (store_) { stored_patches(parent); return; }
    label(parent, "Keep a snapshot while exploring", 24, 24, 0, text, &peepo_bold_24);
    label(parent, "Session storage is in memory. Closing the app clears it.\nDisk persistence and plugin state arrive in Phase 5.", 24, 68, 710, muted);
    auto* row = panel(parent, 24, 140, 752, 100);
    auto* name = label(row, has_saved_ ? saved_.name : "No session snapshot yet", 20, 20, 560, text, &peepo_bold_24);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_height(name, 30);
    label(row, has_saved_ ? std::to_string(saved_.modules.size()) + " modules / session only" : "Save the current graph to try restoring it", 20, 60, 440, muted, &lv_font_montserrat_14);
    button(row, "Load", 616, 26, 112, Action::LoadSession);
    button(parent, "Save snapshot", 24, 268, 200, Action::SaveSession);
    button(parent, "New patch", 240, 268, 168, Action::NameEntry, 0, {}, 0);
    button(parent, "Rename", 424, 268, 168, Action::NameEntry, 0, {}, 1);
    button(parent, "Parallel demo", 608, 268, 168, Action::ParallelDemo);
}
void Application::name_patch(lv_obj_t* parent) {
    auto* input = lv_textarea_create(parent); lv_obj_set_pos(input, 24, 20); lv_obj_set_size(input, 752, 56);
    lv_textarea_set_one_line(input, true);
    // Preserve a backend name longer than the limit for newly typed text.
    lv_textarea_set_text(input, draft_name_.c_str()); lv_textarea_set_max_length(input, 48);
    lv_textarea_set_placeholder_text(input, "Patch name");
    lv_obj_set_style_text_font(input, &peepo_bold_24, 0); lv_obj_set_style_text_color(input, color(text), 0);
    lv_obj_set_style_bg_color(input, color(surface), 0); lv_obj_set_style_bg_opa(input, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(input, color(rim), 0); lv_obj_set_style_border_width(input, 1, 0);
    lv_obj_set_style_radius(input, 8, 0);
    lv_obj_set_style_pad_all(input, 12, 0);
    bind(input, Action::NameInput);
    if (auto* group = lv_group_get_default()) lv_group_add_obj(group, input);
    label(parent, renaming_ && lv_textarea_get_cursor_pos(input) > 48 ?
          "Existing long name preserved. Shorten to 48 characters to add text." :
          "Up to 48 characters. Cancel leaves your current patch intact.", 24, 88, 752, muted, &lv_font_montserrat_14);
    auto* keyboard = lv_keyboard_create(parent); lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(keyboard, 0, 112); lv_obj_set_size(keyboard, 800, 248);
    lv_keyboard_set_textarea(keyboard, input); lv_keyboard_set_popovers(keyboard, false);
    lv_obj_set_style_bg_color(keyboard, color(background), 0); lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(keyboard, 6, 0); lv_obj_set_style_pad_gap(keyboard, 6, 0);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_18, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, color(surface), LV_PART_ITEMS); lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS); lv_obj_set_style_border_color(keyboard, color(border), LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, color(text), LV_PART_ITEMS);
    // Shift, symbols, delete and the like: a step darker, in grey.
    lv_obj_set_style_bg_color(keyboard, color(background), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(keyboard, color(muted), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(keyboard, color(pressed_fill), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_PRESSED);
    bind(keyboard, Action::Keyboard);
    lv_obj_add_state(input, LV_STATE_FOCUSED);
    if (auto* group = lv_obj_get_group(input)) { lv_group_focus_obj(input); lv_group_set_editing(group, true); }
}
void Application::cv_connect(lv_obj_t* parent) {
    const auto* from = find_module(graph_, cv_from_); const auto* to = find_module(graph_, cv_to_);
    if (!from || !to) { label(parent, "Module no longer exists", 24, 24); return; }
    const bool choosing_output = cv_output_.empty();
    const auto ports = connectable_ports(*from, *to, cv_output_, cv_only_);
    auto* prompt = label(parent, choosing_output ? "Which output of " + from->name + "?"
                                                 : from->name + " / " + cv_output_ + "  >  which input of " + to->name + "?", 24, 14, 560, text, &peepo_bold_20);
    lv_label_set_long_mode(prompt, LV_LABEL_LONG_DOT); lv_obj_set_height(prompt, 26);
    const auto count = std::max<std::size_t>(1, (ports.size() + 7) / 8);
    cv_page_ = std::min(cv_page_, count - 1);
    if (count > 1) {
        auto* previous = button(parent, "<", 600, 4, 48, Action::CvPage, 0, {}, static_cast<int>(cv_page_ ? cv_page_ - 1 : 0));
        if (!cv_page_) lv_obj_add_state(previous, LV_STATE_DISABLED);
        label(parent, std::to_string(cv_page_ + 1) + " / " + std::to_string(count), 664, 18, 56, muted, &lv_font_montserrat_14);
        auto* next = button(parent, ">", 728, 4, 48, Action::CvPage, 0, {}, static_cast<int>(std::min(cv_page_ + 1, count - 1)));
        if (cv_page_ + 1 >= count) lv_obj_add_state(next, LV_STATE_DISABLED);
    }
    for (std::size_t i = cv_page_ * 8; i < std::min(ports.size(), cv_page_ * 8 + 8); ++i) {
        const auto slot = i - cv_page_ * 8;
        auto* choice = button(parent, ports[i]->symbol, 24 + static_cast<int>(slot % 2) * 384, 64 + static_cast<int>(slot / 2) * 68, 368, Action::CvChoice, 0, ports[i]->symbol);
        lv_obj_set_style_border_color(choice, color(ports[i]->type == PortType::Cv ? control_ink : cable_ink), 0);
    }
}
void Application::connect_modules(InstanceId from_id, InstanceId to_id) {
    const auto* from = find_module(graph_, from_id);
    const auto* to = find_module(graph_, to_id);
    pressed_module_ = held_module_ = second_module_ = 0;
    pressed_indev_ = held_indev_ = second_indev_ = nullptr;
    second_press_ms_ = 0;
    long_pressed_ = true;
    dragging_ = false;
    hold_connected_ = true;
    for (auto* indev = lv_indev_get_next(nullptr); indev; indev = lv_indev_get_next(indev))
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED)
            lv_indev_wait_release(indev);
    if (!from || !to || from_id == to_id) { navigate(Page::Patch); return; }
    const auto outputs = connectable_ports(*from, *to, {}, false);
    if (outputs.empty()) {
        message(from->name + " has no output that can connect to " + to->name + ".");
        navigate(Page::Patch);
        return;
    }
    const auto inputs = outputs.size() == 1 ? connectable_ports(*from, *to, outputs.front()->symbol, false) : std::vector<const Port*>{};
    if (outputs.size() == 1 && inputs.size() == 1) {
        submit(Connect{{from_id, to_id, outputs.front()->symbol, inputs.front()->symbol}});
        navigate(Page::Patch);
        return;
    }
    cv_from_ = from_id; cv_to_ = to_id; cv_page_ = 0; cv_only_ = false;
    cv_output_ = outputs.size() == 1 ? outputs.front()->symbol : std::string();
    navigate(Page::CvConnect);
}
void Application::confirm_replace(lv_obj_t* parent) {
    label(parent, "Current edits have not been saved", 24, 44, 752, text, &peepo_bold_24);
    auto* name = label(parent, graph_.name, 24, 104, 752, accent, &peepo_bold_24); lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_height(name, 32);
    label(parent, std::string("Replace this patch and discard its current unsaved edits?\nCancel to return and save ") + (store_ ? "it first." : "a session snapshot first."), 24, 168, 752, muted);
    button(parent, "Cancel", 24, 268, 200, Action::CancelReplace);
    primary(button(parent, "Replace patch", 552, 268, 224, Action::Confirm));
}
void Application::module_actions(lv_obj_t* parent) {
    const auto* m = find_module(graph_, selected_);
    if (!m || m->fixed) { label(parent, "Module actions unavailable", 24, 24); return; }
    label(parent, "Choose an action for this module", 24, 24, 0, text, &peepo_bold_24);
    button(parent, "Parameters", 24, 84, 352, Action::Edit, m->id);
    auto* bypass = button(parent, m->bypassed ? "Enable module" : "Software bypass", 408, 84, 368, Action::Bypass, m->id);
    auto* duplicate = button(parent, "Duplicate", 24, 156, 352, Action::Duplicate, m->id);
    if (!module_available(*m)) { lv_obj_add_state(bypass, LV_STATE_DISABLED); lv_obj_add_state(duplicate, LV_STATE_DISABLED); }
    button(parent, "Delete module", 408, 156, 368, Action::Remove, m->id);
    if (!module_available(*m)) {
        const auto* d = definition(m->uri);
        const auto why = d && !d->availability_reason.empty() ? d->availability_reason : std::string("its plugin is not installed here");
        auto* note = label(parent, "Not available: " + why + ".\nIts cables carry no sound. Delete module removes it and its cables;\nthe saved patch keeps it until you save again.", 24, 232, 752, warning);
        lv_obj_set_height(note, 96); lv_label_set_long_mode(note, LV_LABEL_LONG_DOT);
        return;
    }
    // While this page is open a stomp press assigns rather than plays (quick_assign).
    std::vector<std::string> switching;
    for (const auto& id : layout_.ids())
        for (const auto* a : bypass_assignments(graph_, id)) if (a->module == m->id) switching.push_back(control_label(id));
    label(parent, "Press a stomp to switch this module with it", 24, 228, 752, accent, &peepo_bold_20);
    label(parent, (switching.empty() ? std::string("No stomp switches it yet.") : joined_names(switching) + " switches it; press it again to stop.") +
                  " A stomp can switch several modules. Each one flips, so one on and one bypassed swap places.", 24, 262, 752, muted, &lv_font_montserrat_14);
    label(parent, "Duplicate keeps parameter values and software bypass; cables and control assignments stay with the original. "
                  "Settings > Controls assigns parameters too.", 24, 316, 752, faint, &lv_font_montserrat_14);
}
void Application::settings(lv_obj_t* parent) {
    if (!engine_.capabilities().simulation) {
        label(parent, "Engine connection", 24, 20, 0, text, &peepo_bold_24);
        auto* state = label(parent, status_text() + (engine_note_.empty() ? std::string() : ": " + engine_note_), 24, 65, 752, engine_.state() == EngineState::Ready ? accent : warning);
        lv_label_set_long_mode(state, LV_LABEL_LONG_DOT); lv_obj_set_height(state, 24);
        label(parent, "Disconnecting stops only this interface; the engine keeps playing.\nA lost engine is retried every second.", 24, 96, 752, muted, &lv_font_montserrat_14);
        button(parent, "Connect", 24, 142, 160, Action::State, 0, {}, static_cast<int>(EngineState::Ready));
        button(parent, "Disconnect", 200, 142, 160, Action::State, 0, {}, static_cast<int>(EngineState::Disconnected));
        label(parent, store_ ? "Patches are stored in " + store_->root().generic_string() : "Patch storage is not available: session snapshots only.", 24, 216, 752, muted, &lv_font_montserrat_14);
        if (network_) {
            label(parent, "Network", 24, 244, 0, text, &peepo_bold_18);
            network_value_ = label(parent, "", 24, 270, 752, muted, &lv_font_montserrat_14);
            update_network();
        }
        label(parent, "Temperature, memory, the power supply and CPU load: Diagnostics.", 24, 316, 752, muted, &lv_font_montserrat_14);
        return;
    }
    label(parent, "Windows simulator / 800 x 480", 24, 20, 0, text, &peepo_bold_24);
    label(parent, "No audio processing. No hardware connection required.\nUse these states to exercise the interface.", 24, 65, 710, muted);
    button(parent, "Ready", 24, 142, 160, Action::State, 0, {}, static_cast<int>(EngineState::Ready));
    button(parent, "Disconnect", 200, 142, 160, Action::State, 0, {}, static_cast<int>(EngineState::Disconnected));
    button(parent, "Engine error", 376, 142, 180, Action::State, 0, {}, static_cast<int>(EngineState::Error));
    button(parent, "Reject next edit", 24, 216, 220, Action::Reject);
    button(parent, "Missing module", 264, 216, 220, Action::MissingFixture);
    label(parent, "Keys: Q/A W/S turn encoders 1 and 2, keys 1 and 2 push them, Z X C are the stomps.", 24, 276, 752, muted, &lv_font_montserrat_14);
}
namespace {
std::string decimal(double value, int places) {
    std::ostringstream out; out << std::fixed << std::setprecision(places) << value;
    return out.str();
}
std::string gigabytes(std::uint64_t bytes) { return decimal(bytes / 1073741824.0, 1) + " GB"; }
std::string duration(double seconds) {
    const auto minutes = static_cast<long long>(seconds / 60), hours = minutes / 60, days = hours / 24;
    if (days) return std::to_string(days) + " d " + std::to_string(hours % 24) + " h";
    if (hours) return std::to_string(hours) + " h " + std::to_string(minutes % 60) + " min";
    return std::to_string(minutes) + " min";
}
}
void Application::diagnostics(lv_obj_t* parent) {
    // Six cards: a caption, the reading, and what it means. Nothing is
    // estimated: a reading this machine does not give is said to be missing.
    struct Card { const char* caption; std::string value, detail; std::uint32_t ink; };
    const auto& r = reading_;
    const bool measured = static_cast<bool>(system_);
    std::vector<Card> cards;
    const auto cpu = engine_.telemetry().cpu_percent;
    cards.push_back({"ENGINE", status_text(), metric_text(cpu, true) == "--" ? "The engine reports no CPU load." :
                     "CPU " + metric_text(cpu, true) + ": the engine's mean run load" + (engine_.capabilities().simulation ? ", simulated." : "."),
                     engine_.state() == EngineState::Ready ? accent : warning});
    if (!r.under_voltage) cards.push_back({"POWER", "--", "The supply voltage is not measured here.", muted});
    else if (*r.under_voltage) cards.push_back({"POWER", "TOO LOW", "The supply is too weak now: audio can crackle and the CPU slows down.", warning});
    else if (low_voltage_seen_) cards.push_back({"POWER", "WAS LOW", "It dropped too low since this screen started. Use a 5 V, 5 A supply.", warning});
    else cards.push_back({"POWER", "OK", "Not too low since this screen started.", accent});
    if (r.temperature_c) cards.push_back({"TEMPERATURE", decimal(*r.temperature_c, 1) + " C",
                                          *r.temperature_c >= 80 ? "Hot: the CPU slows down near 85 C." : "The CPU slows down near 85 C.", *r.temperature_c >= 80 ? warning : text});
    else cards.push_back({"TEMPERATURE", "--", "Not measured here.", muted});
    if (r.memory_total && r.memory_available) {
        const bool short_of = *r.memory_available * 10 < *r.memory_total;
        cards.push_back({"MEMORY", gigabytes(*r.memory_available) + " free", "Of " + gigabytes(*r.memory_total) + ", shared by the engine, its plugins and this screen.", short_of ? warning : text});
    } else cards.push_back({"MEMORY", "--", "Not measured here.", muted});
    std::error_code error;
    const auto space = store_ ? std::filesystem::space(store_->root(), error) : std::filesystem::space_info{};
    if (store_ && !error && space.capacity) {
        const bool short_of = space.available * 20 < space.capacity;
        cards.push_back({"STORAGE", gigabytes(space.available) + " free", "Of " + gigabytes(space.capacity) + " where patches are saved.", short_of ? warning : text});
    } else cards.push_back({"STORAGE", "--", "No patch storage: session snapshots only.", muted});
    if (r.uptime_s) cards.push_back({"UPTIME", duration(*r.uptime_s), "Since the system started.", text});
    else cards.push_back({"UPTIME", "--", "Not measured here.", muted});
    for (std::size_t i = 0; i < cards.size(); ++i) {
        auto* card = panel(parent, 24 + static_cast<int>(i % 3) * 256, 12 + static_cast<int>(i / 3) * 162, 240, 150);
        auto* caption = label(card, cards[i].caption, 16, 14, 208, faint, &peepo_mono_14);
        lv_obj_set_style_text_letter_space(caption, 2, 0);
        auto* value = label(card, cards[i].value, 16, 38, 208, cards[i].ink, &peepo_bold_24);
        lv_label_set_long_mode(value, LV_LABEL_LONG_DOT); lv_obj_set_height(value, 30);
        auto* detail = label(card, cards[i].detail, 16, 78, 208, muted, &lv_font_montserrat_14);
        lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT); lv_obj_set_height(detail, 60);
    }
    label(parent, measured ? "Read every 2 s. Audio dropouts (xruns) are not counted here yet." : "This computer's readings are not shown; the instrument shows its own.",
          24, 334, 752, faint, &lv_font_montserrat_14);
}
void Application::power(lv_obj_t* parent) {
    struct Choice { const char* title; const char* detail; Action action; bool offered; };
    const Choice choices[] = {
        {"Shut down", "Saved patches are safe. Unplug the instrument when the screen goes dark.", Action::PowerOff, static_cast<bool>(power_off_)},
        {"Restart", "Starts the whole instrument again. It comes back by itself, playing its startup patch.", Action::Restart, static_cast<bool>(restart_)}};
    int y = 12;
    for (const auto& choice : choices) {
        if (!choice.offered) continue;
        auto* card = panel(parent, 24, y, 752, 120);
        label(card, upper(choice.title), 20, 18, 480, text, &peepo_bold_20);
        auto* detail = label(card, choice.detail, 20, 54, 520, muted, &lv_font_montserrat_14);
        lv_obj_set_height(detail, 50);
        auto* b = button(card, choice.title, 572, 35, 160, choice.action);
        if (choice.action == Action::PowerOff) primary(b);
        y += 136;
    }
    if (graph_.dirty) {
        auto* note = label(parent, "This patch has unsaved edits. Save it in Patches first to keep them.", 24, y + 8, 752, warning, &lv_font_montserrat_14);
        lv_label_set_long_mode(note, LV_LABEL_LONG_DOT); lv_obj_set_height(note, 20);
    }
}
void Application::halting(lv_obj_t* parent) {
    label(parent, restarting_ ? "Peepo is restarting" : "Peepo is shutting down", 24, 96, 752, text, &peepo_bold_24);
    label(parent, restarting_ ? "It comes back by itself, playing its startup patch." : "Unplug the instrument when the screen goes dark.", 24, 144, 752, muted);
}

// Physical controls. Events arrive from whatever stands in for the hardware;
// the recognizer turns them into gestures, applied here on the LVGL thread.
void Application::control(const ControlEvent& event) { recognizer_.input(event); }
const Assignment* Application::pinned(const std::string& control) const {
    const auto it = std::find_if(graph_.assignments.begin(), graph_.assignments.end(), [&](const Assignment& a) { return a.control == control; });
    return it == graph_.assignments.end() ? nullptr : &*it;
}
void Application::gesture(const Gesture& g) {
    // Typing a name: the keys standing in for controls are text there.
    // Going down: nothing is changed any more.
    if (page_ == Page::NamePatch || page_ == Page::Halting) return;
    active_control_ = g.control;
    if (page_ == Page::Controls) rebuild_ = true;
    if (is_encoder(g.control)) encoder(g);
    else if (is_stomp(g.control)) stomp(g);
}
void Application::encoder(const Gesture& g) {
    const auto name = control_label(g.control);
    const bool turn = g.kind == Gesture::Kind::Turn, tap = g.kind == Gesture::Kind::Tap;
    // A pinned encoder controls its target on every page: a tap shows it, a hold unpins it.
    if (const auto* pin = pinned(g.control)) {
        const auto* m = assignment_parameter(graph_, *pin) ? find_module(graph_, pin->module) : nullptr;
        if (g.kind == Gesture::Kind::Hold) { submit(Unassign{g.control}); message(name + " unpinned: it moves through the page again."); }
        else if (!m) message(name + " is pinned to a module that is gone. Settings > Controls repairs it.");
        else if (turn) turn_parameter(m->id, pin->parameter, g, pin);
        else if (tap) { focus_symbol_ = pin->parameter; open_module(m->id); }
        return;
    }
    // Two encoders run everything. Encoder 1 moves through the page and
    // pushes what it is on; encoder 2 turns the value it is on. Holding
    // encoder 1 goes back. A single encoder does both: its push on a value
    // switches between moving and turning.
    const bool first = g.control == "enc1";
    if (g.kind == Gesture::Kind::Hold) {
        if (first && page_ != Page::Patch) navigate(back_page());
        return;
    }
    switch (page_) {
    case Page::Parameters:
        if (bars_.empty()) return;
        if (turn && first && !adjusting_) focus_parameter(static_cast<std::size_t>(std::max(0, static_cast<int>(parameter_focus_) + g.steps)));
        else if (turn && bars_[parameter_focus_].file) message("Push to choose a file.");
        else if (turn) turn_parameter(bars_[parameter_focus_].id, bars_[parameter_focus_].symbol, g, nullptr);
        else if (tap) activate_parameter();
        return;
    case Page::Patch:
        if (first) {
            if (turn) focus_module(g.steps);
            else if (tap && find_module(graph_, patch_focus_)) open_module(patch_focus_);
            else if (tap) message("Turn encoder 1 to pick a module.");
            return;
        }
        break;
    case Page::Browser:
        if (tap) { if (!browser_pick_.empty()) add_module(browser_pick_); return; }
        if (!turn) return;
        if (!first) {
            // Encoder 2 picks the group.
            const auto count = static_cast<int>(browser_groups_.size());
            const auto at = static_cast<int>(std::find(browser_groups_.begin(), browser_groups_.end(), browser_group_) - browser_groups_.begin());
            browser_group_ = browser_groups_[static_cast<std::size_t>(((at + g.steps) % count + count) % count)];
            browser_category_.clear(); browser_pick_.clear(); browser_scroll_ = 0; rebuild_ = true;
        } else if (!browser_order_.empty()) {
            const auto last = static_cast<int>(browser_order_.size()) - 1;
            const auto it = std::find(browser_order_.begin(), browser_order_.end(), browser_pick_);
            const int at = it == browser_order_.end() ? (g.steps > 0 ? -1 : last + 1) : static_cast<int>(it - browser_order_.begin());
            browser_pick(browser_order_[static_cast<std::size_t>(std::clamp(at + g.steps, 0, last))], true);
        }
        return;
    case Page::Tempo:
        // Either encoder sets the tempo, a beat per minute a detent; a push taps it.
        if (turn) set_tempo((tempo_sent_ ? *tempo_sent_ : graph_.tempo) + g.steps);
        else if (tap) tap_tempo();
        return;
    default: break;
    }
    // Everywhere else, and encoder 2 on the patch: step through the page's
    // buttons, and push the one highlighted.
    auto* group = lv_group_get_default();
    if (!group) return;
    if (turn) {
        if (auto* old = lv_group_get_focused(group)) lv_obj_remove_state(old, LV_STATE_FOCUS_KEY);
        for (int i = 0; i < std::abs(g.steps); ++i) (g.steps > 0 ? lv_group_focus_next : lv_group_focus_prev)(group);
        if (auto* now = lv_group_get_focused(group)) { lv_obj_add_state(now, LV_STATE_FOCUS_KEY); lv_obj_scroll_to_view_recursive(now, LV_ANIM_OFF); }
    } else if (tap) {
        auto* focused = lv_group_get_focused(group);
        if (focused && !lv_obj_has_state(focused, LV_STATE_DISABLED)) lv_obj_send_event(focused, LV_EVENT_CLICKED, nullptr);
    }
}
void Application::stomp(const Gesture& g) {
    const auto name = control_label(g.control);
    const bool pressed = g.kind == Gesture::Kind::Press;
    // A module's actions are open, as after holding it on the patch: the stomp is being assigned.
    if (page_ == Page::ModuleActions) { if (pressed) quick_assign(g.control); return; }
    // Footswitch modules hear the stomp as well as whatever it is assigned.
    std::vector<std::string> said;
    const bool listened = drive_footswitches(g.control, pressed, said);
    if (const auto bypasses = bypass_assignments(graph_, g.control); !bypasses.empty()) {
        // Each module flips, so two set opposite ways swap places.
        if (pressed)
            for (const auto* a : bypasses)
                if (const auto* m = assignment_target(graph_, *a)) { submit(SetBypass{m->id, !m->bypassed}); said.push_back(m->name + (m->bypassed ? " on" : " bypassed")); }
        if (!said.empty()) message(name + ": " + joined_names(said));
        else if (pressed) message(name + " switches modules that are gone. Settings > Controls repairs it.");
        return;
    }
    if (const auto* pin = pinned(g.control)) {
        const auto* m = assignment_target(graph_, *pin);
        if (!m) { if (pressed) message(name + " is pinned to a module that is gone. Settings > Controls repairs it."); return; }
        // Switch actions go straight to the engine: never coalesced with turns.
        const auto* p = assignment_parameter(graph_, *pin);
        const auto value = switched_value(*p, p->value, *pin, pressed);
        if (value && *value != p->value) {
            auto shown = *p; shown.value = *value;
            submit(SetParameter{m->id, p->symbol, *value});
            said.insert(said.begin(), m->name + " / " + p->name + " " + format_value(shown));
        }
        if (!said.empty()) message(name + ": " + joined_names(said));
        return;
    }
    // A Footswitch module listening makes the stomp this patch's: it does not
    // also take its every-patch action.
    if (listened) { if (!said.empty()) message(name + ": " + joined_names(said)); return; }
    if (!pressed) return;
    const auto global = store_ ? store_->global_controls() : std::map<std::string, std::string>{};
    if (const auto it = global.find(g.control); it != global.end()) {
        if (it->second == "tap-tempo") tap_tempo();
        else step_patch(it->second == "next-patch" ? 1 : -1);
        return;
    }
    message(name + " is not assigned. Settings > Controls.");
}
void Application::quick_assign(const std::string& control) {
    const auto name = control_label(control);
    const auto* m = find_module(graph_, selected_);
    if (!m || m->fixed || !module_available(*m)) { message(name + " can switch only a module that is installed."); return; }
    const auto held = bypass_assignments(graph_, control);
    if (std::any_of(held.begin(), held.end(), [&](const Assignment* a) { return a->module == m->id; })) {
        submit(Unassign{control, m->id});
        message(name + " no longer switches " + m->name + ".");
        return;
    }
    std::vector<std::string> others;
    for (const auto* a : held) if (const auto* other = assignment_target(graph_, *a)) others.push_back(other->name);
    const bool replaces = held.empty() && pinned(control);
    Assignment a;
    a.control = control; a.module = m->id; a.action = ControlAction::Bypass;
    submit(Assign{a, true});
    message(name + " now switches " + m->name + (others.empty() ? "" : " with " + joined_names(others)) +
            (replaces ? ", instead of what it controlled" : "") + ". Press it again here to undo.");
}
bool Application::drive_footswitches(const std::string& control, bool pressed, std::vector<std::string>& said) {
    bool listened = false;
    for (const auto& m : graph_.modules) {
        if (m.uri != footswitch_uri || !module_available(m)) continue;
        const Parameter *which = nullptr, *down = nullptr, *latching = nullptr;
        for (const auto& p : m.parameters) {
            if (p.symbol == "stomp") which = &p;
            else if (p.symbol == "pressed") down = &p;
            else if (p.symbol == "latching") latching = &p;
        }
        if (!which || !down || control != "stomp" + std::to_string(static_cast<int>(which->value) + 1)) continue;
        listened = true;
        double value = pressed ? 1 : 0;
        // Latching flips on each press; momentary follows the switch. Every
        // momentary change is sent, so a quick tap is never lost behind an echo.
        if (latching && latching->value >= 0.5) { if (!pressed) continue; value = down->value >= 0.5 ? 0 : 1; }
        submit(SetParameter{m.id, down->symbol, value});
        said.push_back(m.name + (value >= 0.5 ? " on" : " off"));
    }
    return listened;
}
bool Application::orphaned(const std::string& control) const {
    bool any = false;
    for (const auto& a : graph_.assignments)
        if (a.control == control) { any = true; if (assignment_target(graph_, a)) return false; }
    return any;
}
void Application::step_patch(int direction) {
    refresh_stored();
    std::vector<const StoredPatch*> loadable;
    for (const auto& patch : stored_) if (patch.revision) loadable.push_back(&patch);
    if (loadable.empty()) { message("No saved patches to step through."); return; }
    const auto here = std::find_if(loadable.begin(), loadable.end(), [&](const StoredPatch* p) { return p->id == graph_.patch_id; });
    std::size_t next = 0;
    if (here == loadable.end()) next = direction > 0 ? 0 : loadable.size() - 1;
    else next = (static_cast<std::size_t>(here - loadable.begin()) + loadable.size() + direction) % loadable.size();
    // Unsaved edits are guarded as on screen: the footswitch never discards them silently.
    load_stored(loadable[next]->id, false, true);
}
std::string Application::assignment_text(const std::string& control) const {
    if (const auto held = bypass_assignments(graph_, control); !held.empty()) {
        std::vector<std::string> names;
        for (const auto* a : held) if (const auto* m = assignment_target(graph_, *a)) names.push_back(m->name);
        if (names.empty()) return "Orphaned: its modules are gone or changed. Edit to clear it.";
        return joined_names(names) + " / bypass" + (names.size() < held.size() ? " (one is gone)" : "");
    }
    if (const auto* a = pinned(control)) {
        const auto* m = assignment_target(graph_, *a);
        if (!m) return "Orphaned: its module is gone or changed. Edit to clear it.";
        const auto* p = assignment_parameter(graph_, *a);
        auto low = *p, high = *p; low.value = a->minimum; high.value = a->maximum;
        std::string summary = m->name + " / " + p->name;
        if (a->minimum != p->minimum || a->maximum != p->maximum) summary += ", " + format_value(low) + " to " + format_value(high);
        if (a->inverted) summary += ", inverted";
        if (is_stomp(control)) summary += a->mode == SwitchMode::Momentary ? ", momentary" : ", toggle";
        return summary;
    }
    if (store_) if (const auto global = store_->global_controls(); global.contains(control))
        return global.at(control) == "next-patch" ? "Next patch (every patch)" : global.at(control) == "tap-tempo" ? "Tap tempo (every patch)" : "Previous patch (every patch)";
    if (control == "enc1") return "Moves through the page; hold to go back";
    return is_encoder(control) ? "Turns the highlighted control" : "Not assigned";
}
void Application::controls(lv_obj_t* parent) {
    const auto ids = layout_.ids();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto& id = ids[i];
        auto* row = panel(parent, 24, 8 + static_cast<int>(i) * 50, 752, 48);
        // Turning or pressing a control shows which row it is.
        if (id == active_control_) lv_obj_set_style_border_color(row, color(accent), 0);
        label(row, control_label(id), 14, 13, 120, id == active_control_ ? accent : text);
        const auto* pin = pinned(id);
        const bool orphan = orphaned(id);
        auto* what = label(row, assignment_text(id), 140, 14, 396, orphan ? warning : pin ? text : muted, &lv_font_montserrat_14);
        lv_label_set_long_mode(what, LV_LABEL_LONG_DOT); lv_obj_set_height(what, 18);
        button(row, "Assign", 544, 0, 100, Action::AssignControl, 0, id);
        auto* edit = button(row, "Edit", 652, 0, 100, Action::EditControl, 0, id);
        if (!pin && !(store_ && store_->global_controls().contains(id))) lv_obj_add_state(edit, LV_STATE_DISABLED);
    }
}
void Application::assign_picker(lv_obj_t* parent) {
    struct Choice { std::string title; Action action; InstanceId id; std::string symbol; };
    std::vector<Choice> choices;
    const bool stomp = is_stomp(editing_control_);
    if (!picking_module_) {
        if (stomp && store_) {
            choices.push_back({"Next patch", Action::PickTarget, 0, "@next-patch"});
            choices.push_back({"Previous patch", Action::PickTarget, 0, "@previous-patch"});
            choices.push_back({"Tap tempo", Action::PickTarget, 0, "@tap-tempo"});
        }
        for (const auto& m : graph_.modules)
            if (!m.fixed && module_available(m) && (stomp || !m.parameters.empty())) choices.push_back({m.name, Action::PickModule, m.id, {}});
    } else if (const auto* m = find_module(graph_, picking_module_)) {
        if (stomp) choices.push_back({"Bypass", Action::PickTarget, 0, "@bypass"});
        for (const auto& p : m->parameters) choices.push_back({p.name, Action::PickTarget, 0, p.symbol});
    }
    auto* prompt = label(parent, picking_module_ ? "Which control?" : stomp ? "A module, or a patch change for every patch" : "Which module?", 24, 18, 560, text, &peepo_bold_20);
    lv_label_set_long_mode(prompt, LV_LABEL_LONG_DOT); lv_obj_set_height(prompt, 26);
    if (choices.empty()) { label(parent, "Nothing in this patch can be controlled yet.", 24, 72, 752, muted); return; }
    const auto pages = std::max<std::size_t>(1, (choices.size() + 7) / 8);
    picker_page_ = std::min(picker_page_, pages - 1);
    if (pages > 1) {
        auto* previous = button(parent, "<", 600, 4, 48, Action::PickerPage, 0, {}, static_cast<int>(picker_page_ ? picker_page_ - 1 : 0));
        if (!picker_page_) lv_obj_add_state(previous, LV_STATE_DISABLED);
        label(parent, std::to_string(picker_page_ + 1) + " / " + std::to_string(pages), 664, 18, 56, muted, &lv_font_montserrat_14);
        auto* next = button(parent, ">", 728, 4, 48, Action::PickerPage, 0, {}, static_cast<int>(std::min(picker_page_ + 1, pages - 1)));
        if (picker_page_ + 1 >= pages) lv_obj_add_state(next, LV_STATE_DISABLED);
    }
    for (std::size_t i = picker_page_ * 8; i < std::min(choices.size(), picker_page_ * 8 + 8); ++i) {
        const auto slot = i - picker_page_ * 8;
        auto* choice = button(parent, choices[i].title, 24 + static_cast<int>(slot % 2) * 384, 64 + static_cast<int>(slot / 2) * 68, 368,
                              choices[i].action, choices[i].id, choices[i].symbol);
        if (auto* title = lv_obj_get_child(choice, 0)) { lv_label_set_long_mode(title, LV_LABEL_LONG_DOT); lv_obj_set_width(title, 340); lv_obj_center(title); }
    }
}
void Application::assignment_detail(lv_obj_t* parent) {
    const auto& id = editing_control_;
    auto* heading = label(parent, assignment_text(id), 24, 16, 752, accent, &peepo_bold_20);
    lv_label_set_long_mode(heading, LV_LABEL_LONG_DOT); lv_obj_set_height(heading, 26);
    button(parent, "Clear", 24, 280, 160, Action::ClearControl, 0, id);
    primary(button(parent, "Done", 616, 280, 160, Action::Navigate, 0, {}, static_cast<int>(Page::Controls)));
    const auto* a = pinned(id);
    if (!a) { label(parent, "This control acts the same in every patch.", 24, 64, 752, muted); return; }
    const auto* p = assignment_parameter(graph_, *a);
    if (orphaned(id)) {
        label(parent, "Its module was removed, or now holds a different plugin, so it is never applied.\nClear it, then assign the control again.", 24, 64, 752, muted);
        return;
    }
    if (!p) {
        label(parent, "Each press switches the software bypass of each module: one that plays is\nbypassed, one bypassed plays, so two set opposite ways swap places.\n"
                      "To add or remove a module, hold it on the patch and press this stomp.", 24, 64, 752, muted);
        return;
    }
    auto* invert = button(parent, a->inverted ? "Inverted" : "Invert", 24, 60, 160, Action::InvertControl);
    if (a->inverted) lv_obj_set_style_border_color(invert, color(accent), 0);
    if (is_stomp(id)) {
        auto* toggle = button(parent, "Toggle", 200, 60, 160, Action::SwitchModeChoice, 0, {}, 0);
        auto* momentary = button(parent, "Momentary", 376, 60, 160, Action::SwitchModeChoice, 0, {}, 1);
        lv_obj_set_style_border_color(a->mode == SwitchMode::Momentary ? momentary : toggle, color(accent), 0);
    }
    label(parent, is_stomp(id) ? (a->mode == SwitchMode::Momentary ? "Held: To. Released: From." : "Each press: From, then To.")
                               : "Clockwise moves from From towards To; push and turn for fine steps.", 24, 120, 752, muted, &lv_font_montserrat_14);
    if (p->kind == ParameterKind::Toggle || p->kind == ParameterKind::Enumeration) return;
    // The two ends of the range, in the parameter's own (linear or logarithmic) scale.
    for (int end = 0; end < 2; ++end) {
        const int y = 148 + end * 64;
        auto shown = *p; shown.value = end ? a->maximum : a->minimum;
        label(parent, end ? "To" : "From", 24, y + 8, 80, text);
        auto* value = label(parent, format_value(shown), 600, y + 8, 176, accent);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        auto* slider = lv_slider_create(parent); lv_obj_set_pos(slider, 120, y + 14); lv_obj_set_size(slider, 460, 12);
        lv_slider_set_range(slider, 0, 1000); lv_slider_set_value(slider, static_cast<int>(normalized_value(shown) * 1000), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(slider, color(border), LV_PART_MAIN); lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider, color(accent), LV_PART_INDICATOR); lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, color(text), LV_PART_KNOB); lv_obj_set_style_pad_all(slider, 10, LV_PART_KNOB);
        lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB); lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_ext_click_area(slider, 20);
        bind(slider, Action::RangeEnd, 0, {}, end, value);
    }
}
}
