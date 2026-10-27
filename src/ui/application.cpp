// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "ui/application.hpp"
#include "core/flow_wave.hpp"
#include "core/poly_voices.hpp"
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
// MIDI jacks (diamonds) and cables: a colour no module group uses.
constexpr std::uint32_t midi_ink = 0x2dd4bf;
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
// Whether text shows whole in a box this wide and tall: no word wider than
// the box, so none is broken, and every wrapped line inside.
bool text_fits(const std::string& value, const lv_font_t* font, int width, int height) {
    lv_point_t size;
    std::istringstream words(value);
    for (std::string word; words >> word;) {
        lv_text_get_size(&size, word.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > width) return false;
    }
    lv_text_get_size(&size, value.c_str(), font, 0, 0, width, LV_TEXT_FLAG_NONE);
    return size.y <= height;
}
int snap(int value) { return static_cast<int>(std::round(value / 24.0)) * 24; }
std::string metric_text(std::optional<double> reading, bool cpu) {
    if (!reading || !std::isfinite(*reading) || (cpu ? *reading < 0 || *reading > 100 : *reading <= 0)) return "--";
    std::ostringstream value;
    value << std::fixed << std::setprecision(std::abs(*reading - std::round(*reading)) < 0.05 ? 0 : 1) << *reading;
    if (cpu) value << '%';
    return value.str();
}
enum class ModuleLayout { Generic, Tuner, Euclidean, Looper, Pedal };
// Modules with a page of their own on two tabs: the main controls first, two
// to a row like every bar, and the stomps; the rest on the second tab under
// the overlay's groups. Flow Envelope draws its wave above its main bars.
struct PedalBar { const char* symbol; const char* name; bool segmented; };
struct PedalLayout { std::vector<PedalBar> main; std::vector<const char*> stomps; bool wave = false; };
const PedalLayout* pedal_layout(const std::string& uri) {
    static const PedalLayout aura{
        {{"time", nullptr, false}, {"wet_modify", "Modify", false},
         {"length", nullptr, false}, {"loop_modify", "Modify", false},
         {"mix", nullptr, false}, {"clock", nullptr, false},
         {"wet_mode", nullptr, true}, {"loop_mode", nullptr, true},
         {"routing", nullptr, true}},
        {"wet_on", "freeze", "loop_on", "overdub"}};
    static const PedalLayout discipline{
        {{"level", nullptr, false}, {"repeats", nullptr, false},
         {"size", nullptr, false}, {"modify", nullptr, false},
         {"spread", nullptr, false}, {"scan", nullptr, false},
         {"modifier", nullptr, false}, {"routing", nullptr, true}},
        {"loop", "scan_hold", "clear"}};
    static const PedalLayout flow{
        {{"shape_param", nullptr, false}, {"slope_param", nullptr, false},
         {"smoothness_param", nullptr, false}, {"shift_param", nullptr, false},
         {"frequency_param", nullptr, false}, {"range_param", nullptr, false},
         {"ramp_param", nullptr, false}, {"mode_param", nullptr, false}},
        {}, true};
    if (uri == "https://github.com/ABustos2606/aura#aura") return &aura;
    if (flow_uri(uri)) return &flow;
    if (uri == "https://github.com/ABustos2606/discipline#discipline") return &discipline;
    return nullptr;
}
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
    if (looper_uri(m.uri)) return ModuleLayout::Looper;
    if (pedal_layout(m.uri)) return ModuleLayout::Pedal;
    return ModuleLayout::Generic;
}
// Vertical placement of one connector on its edge of the card. Every audio
// channel has its own connector, so a stereo port shows two (left above
// right). A single connector stays centred; several share the edge in equal
// touch bands, and a card with more than two on a side grows to keep them
// apart.
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
            if (out.type != PortType::Cv && (cv_only || (out.type != PortType::Audio && out.type != PortType::Midi))) continue;
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
// Audio and MIDI connectors share the side edges, inputs left, outputs right,
// in the plugin's own order (audio first, as the catalogue lists them).
bool side_port(const Port& p) { return p.type == PortType::Audio || p.type == PortType::Midi; }
const Port* port_of(const Module& m, const std::string& symbol, Direction direction) {
    for (const auto& p : module_ports(m)) if (p.symbol == symbol && p.direction == direction) return &p;
    return nullptr;
}
struct Jack { int centre, height; };
std::optional<Jack> jack(const Module& m, const std::string& symbol, Direction direction, unsigned channel = 0) {
    int count = 0, index = -1;
    for (const auto& p : module_ports(m)) {
        if (!side_port(p) || p.direction != direction) continue;
        if (p.symbol == symbol && channel < p.channels) index = count + static_cast<int>(channel);
        count += static_cast<int>(p.channels);
    }
    if (index < 0) return {};
    const int height = module_height_of(m);
    return Jack{(2 * index + 1) * height / (2 * count), std::min(44, height / count)};
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
    // The width it was given: a new bar is painted before it is laid out.
    const int w = lv_obj_get_style_width(bar, LV_PART_MAIN);
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
// The module page's smaller bars, with room around them to scroll.
constexpr int compact_width = 300, compact_height = 40;
// Flow Envelope's wave box, over both columns of bars; the bars start below it.
constexpr int flow_left = 64, flow_width = 2 * compact_width + 40, flow_height = 118, flow_box_height = flow_height - 10, flow_inset = 14;
// A segmented bar shows every choice of a mode, as the Mood's three-way toggles.
bool segmented_bar(lv_obj_t* bar) { return lv_obj_has_flag(bar, LV_OBJ_FLAG_USER_1); }
int segment_at(lv_obj_t* bar, const Parameter& p, int x) {
    lv_area_t area; lv_obj_get_coords(bar, &area);
    const int count = std::max<int>(1, static_cast<int>(p.choices.size()));
    return std::clamp((x - area.x1) * count / std::max(1, bar_span(bar)), 0, count - 1);
}
// A bar's children: the fill, the name, the value; a segmented bar then has
// one label per choice, the chosen one lit.
void paint_bar(lv_obj_t* bar, const Parameter& p) {
    if (segmented_bar(bar)) {
        const int count = std::max<int>(1, static_cast<int>(p.choices.size()));
        const int at = std::clamp(static_cast<int>(std::round(p.value)), 0, count - 1);
        auto* fill = lv_obj_get_child(bar, 0);
        lv_obj_set_x(fill, at * bar_span(bar) / count);
        lv_obj_set_width(fill, (at + 1) * bar_span(bar) / count - at * bar_span(bar) / count);
        for (int i = 0; i < count; ++i)
            if (auto* choice = lv_obj_get_child(bar, 3 + i)) lv_obj_set_style_text_color(choice, color(i == at ? text : faint), 0);
    } else lv_obj_set_width(lv_obj_get_child(bar, 0), static_cast<int>(std::round(bar_level(p) * bar_span(bar))));
    const auto shown = bar_text(p);
    auto* value = lv_obj_get_child(bar, 2);
    if (shown != lv_label_get_text(value)) lv_label_set_text(value, shown.c_str());
}
// The rail left of the module page's list, as on Beebo: a line, and a ball
// where the list is scrolled to.
constexpr int rail_width = 56, rail_ball = 14, rail_inset = 14;
void place_rail_ball(lv_obj_t* rail, lv_obj_t* list) {
    const int scrolled = lv_obj_get_scroll_y(list), rest = lv_obj_get_scroll_bottom(list);
    const int travel = lv_obj_get_height(rail) - 2 * rail_inset - rail_ball;
    const double share = scrolled + rest > 0 ? std::clamp(static_cast<double>(scrolled) / (scrolled + rest), 0.0, 1.0) : 0;
    lv_obj_set_y(lv_obj_get_child(rail, 1), rail_inset + static_cast<int>(std::round(share * travel)));
}
void rail_followed(lv_event_t* e) {
    place_rail_ball(static_cast<lv_obj_t*>(lv_event_get_user_data(e)), lv_event_get_target_obj(e));
}
// Touching the rail moves the list to that point; a drag along it scrolls.
void rail_dragged(lv_event_t* e) {
    const auto code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;
    auto* rail = lv_event_get_target_obj(e);
    auto* list = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    lv_point_t point{};
    if (auto* indev = lv_indev_active()) lv_indev_get_point(indev, &point);
    lv_area_t area; lv_obj_get_coords(rail, &area);
    const int travel = std::max(1, lv_obj_get_height(rail) - 2 * rail_inset - rail_ball);
    const double share = std::clamp(static_cast<double>(point.y - area.y1 - rail_inset - rail_ball / 2) / travel, 0.0, 1.0);
    const int range = lv_obj_get_scroll_y(list) + lv_obj_get_scroll_bottom(list);
    lv_obj_scroll_to_y(list, static_cast<int>(std::round(share * range)), LV_ANIM_OFF);
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
// The stomps that act on a module, by letter: assigned to its bypass or a
// parameter, or heard by it when it is a Footswitch module.
std::vector<std::string> stomps_on(const Graph& g, const Module& m) {
    std::vector<std::string> letters;
    const auto add = [&](const std::string& id) {
        if (const auto l = stomp_letter(id); !l.empty() && std::find(letters.begin(), letters.end(), l) == letters.end()) letters.push_back(l);
    };
    for (const auto& a : g.assignments)
        if (a.module == m.id && is_stomp(a.control) && assignment_target(g, a)) add(a.control);
    if (m.uri == footswitch_uri)
        for (const auto& p : m.parameters) if (p.symbol == "stomp") add("stomp" + std::to_string(static_cast<int>(p.value) + 1));
    std::sort(letters.begin(), letters.end());
    return letters;
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
Application::~Application() { release_looper(); engine_.watch_looper(std::nullopt); engine_.watch_tuner(std::nullopt); engine_.watch_euclidean(std::nullopt); if (screen_) lv_obj_delete(screen_); }
void Application::message(std::string value) {
    message_ = std::move(value);
    message_ms_ = clock_ms_;
    if (footer_) lv_label_set_text(footer_, message_.c_str());
}
void Application::navigate(Page page) {
    // The browser keeps its filter between visits, so a chain of one kind is
    // quick to build, but always opens at the top with nothing picked.
    if (page == Page::Browser && page_ != Page::Browser) { browser_pick_.clear(); browser_scroll_ = 0; }
    // Another program, or a damaged disk, may have changed what is stored.
    if (page == Page::Patches && page_ != Page::Patches) refresh_stored();
    // From the patch every list opens at the top.
    if (page_ == Page::Patch) list_scroll_.clear();
    // Leaving the Scenes page ends picking a scene.
    if (page != Page::Scenes) scene_pick_ = ScenePick::None;
    if (page == Page::Scenes && page_ != Page::Scenes && page_ != Page::NamePatch && page_ != Page::ConfirmAction) message("Tap a scene to play it.");
    // Leaving MIDI learn before anything was learned ends the engine's wait.
    if (page_ == Page::MidiLearn && page != Page::MidiLearn && (learn_waiting_ || learn_request_)) {
        learn_waiting_ = false; learn_request_ = 0;
        submit(LearnMidi{learn_id_, learn_symbol_, true});
    }
    if (page == Page::Midi && midi_) { midi_settings_ = midi_->settings(); midi_status_ = midi_->status(); }
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
void Application::stream_parameter(InstanceId id, const std::string& symbol, double value, bool scene) {
    // A moving control must be heard while it moves. Send as fast as the
    // engine acknowledges, and never queue more than the newest value.
    if (stream_ && (stream_->id != id || stream_->symbol != symbol || stream_->scene != scene)) {
        if (stream_->sent != stream_->latest) submit(SetParameter{stream_->id, stream_->symbol, stream_->latest, stream_->scene});
        stream_.reset();
    }
    if (!stream_) stream_ = Stream{id, symbol};
    stream_->scene = scene;
    stream_->latest = value;
    if (!stream_->in_flight) send_stream();
}
void Application::send_stream() {
    if (stream_->sent == stream_->latest) return;
    stream_->sent = stream_->latest;
    stream_->in_flight = submit(SetParameter{stream_->id, stream_->symbol, stream_->latest, stream_->scene});
    if (!stream_->in_flight) stream_.reset();
}
// When the cables on a MIDI to CV poly's voices change, its Voices follows
// them, so no note goes to a voice nothing plays. A patch as loaded, or a
// Voices set by hand, stays until its cables next change.
void Application::follow_poly_voices(bool replaced) {
    if (replaced) poly_voices_.clear();
    std::map<InstanceId, int> seen;
    for (const auto& m : graph_.modules) {
        if (m.uri != midi_cv_poly_uri) continue;
        const int cabled = cabled_voices(graph_, m);
        seen[m.id] = cabled;
        const auto before = poly_voices_.find(m.id);
        if (before == poly_voices_.end() || before->second == cabled || cabled == 0) continue;
        const auto voices = std::find_if(m.parameters.begin(), m.parameters.end(), [](const Parameter& p) { return p.symbol == "voices"; });
        if (voices != m.parameters.end() && std::lround(voices->value) != cabled) submit(SetParameter{m.id, "voices", static_cast<double>(cabled)});
    }
    poly_voices_ = std::move(seen);
}
void Application::tick(unsigned ms) {
    clock_ms_ += ms;
    bool changed = false, replaced = false;
    for (auto& e : engine_.advance(ms)) {
        const auto it = pending_.find(e.request);
        const auto kind = it == pending_.end() ? PendingKind::Edit : it->second;
        if (e.snapshot) changed = true;
        if (kind == PendingKind::Replace && e.accepted) replaced = true;
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
        if (const auto note = accepted_notes_.find(e.request); note != accepted_notes_.end()) {
            if (e.accepted) message(note->second());
            accepted_notes_.erase(note);
        }
        if (kind == PendingKind::Add) {
            if (!e.accepted) message("Could not add " + adding_[e.request] + ": " + e.message);
            adding_.erase(e.request);
        } else if (kind == PendingKind::Save && staging_) {
            // The engine has written the bundle; publish it, or leave the previous revision alone.
            if (e.accepted) {
                const auto issue = store_->publish(*staging_);
                if (issue.empty()) message("Saved " + graph_.name + " (revision " + std::to_string(staging_->revision) + ").");
                else {
                    // The engine has marked the graph saved; it is not.
                    store_->discard(*staging_);
                    if (const auto request = submit(RestoreUnsaved{save_restore_id_})) accepted_notes_[request] = [issue] { return "Not saved: " + issue; };
                    message("Not saved: " + issue);
                }
            } else store_->discard(*staging_);
            staging_.reset();
            refresh_stored();
        } else if (kind == PendingKind::SaveAs) {
            if (e.accepted) start_save(save_as_id_);
            save_as_id_.clear();
        } else if (kind == PendingKind::Replace && e.accepted && !load_note_.empty()) message(e.message.empty() ? load_note_ : e.message + " " + load_note_);
        if (kind == PendingKind::Replace) load_note_.clear();
        // MIDI learn waits once the engine has dropped the old binding and listens.
        if (e.request && e.request == learn_request_) {
            learn_request_ = 0;
            learn_waiting_ = e.accepted && page_ == Page::MidiLearn;
            if (learn_waiting_) message("Move a control on a MIDI device.");
        }
        // Another client's edits, or a learned MIDI controller turning, can
        // arrive dozens of times a second: those redraw at most ten times a second.
        if (e.request) rebuild_ = true;
        else news_ = true;
    }
    if (changed) follow_poly_voices(replaced);
    if (news_ && clock_ms_ - rebuilt_ms_ >= 100) { news_ = false; rebuild_ = true; }
    if (learn_waiting_) {
        const auto* m = find_module(graph_, learn_id_);
        const auto it = m ? std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == learn_symbol_; }) : std::vector<Parameter>::const_iterator{};
        if (!m || it == m->parameters.end()) { learn_waiting_ = false; message("The module is gone; nothing was learned."); navigate(Page::Patch); }
        else if (it->midi) {
            learn_waiting_ = false;
            message(it->name + " now follows " + midi_binding_text(*it->midi) + ".");
            navigate(Page::Parameters);
        }
    }
    // What peepo-midi hears: devices come and go, incoming clock changes.
    if (midi_ && (midi_age_ += ms) >= 1000) {
        midi_age_ = 0;
        if (auto heard = midi_->status(); !(heard == midi_status_)) {
            midi_status_ = std::move(heard);
            if (page_ == Page::Midi || page_ == Page::Tempo) rebuild_ = true;
        }
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
    engine_.watch_looper(shown && layout == ModuleLayout::Looper ? std::optional<InstanceId>{shown->id} : std::nullopt);
    update_tuner();
    update_euclidean();
    update_flow();
    update_looper();
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
    bindings_.push_back({this, action, id, std::move(symbol), number, value, obj});
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
// What a lit bar on a page that is not a module's fills with.
constexpr std::uint32_t setting_fill = 0x166534;
// A section of a page's list: its name in small capitals across the list.
lv_obj_t* list_heading(lv_obj_t* list, const std::string& name, std::uint32_t ink = accent) {
    auto* h = label(list, upper(name), 0, 0, 0, ink, &peepo_mono_14);
    lv_obj_set_width(h, LV_PCT(100)); lv_obj_set_style_text_letter_space(h, 2, 0);
    lv_label_set_long_mode(h, LV_LABEL_LONG_DOT); lv_obj_set_height(h, 18);
    return h;
}
// A line of explanation across the list.
lv_obj_t* list_note(lv_obj_t* list, const std::string& value, std::uint32_t ink = muted) {
    auto* n = label(list, value, 0, 0, 0, ink, &lv_font_montserrat_14);
    lv_obj_set_width(n, LV_PCT(100)); lv_label_set_long_mode(n, LV_LABEL_LONG_WRAP);
    return n;
}
// A value bar's fill and value, from its binding.
void paint_slide(lv_obj_t* bar, double at, double low, double high, const std::function<std::string(double)>& shown) {
    const double share = high > low ? std::clamp((at - low) / (high - low), 0.0, 1.0) : 0;
    lv_obj_set_width(lv_obj_get_child(bar, 0), static_cast<int>(std::round(share * bar_span(bar))));
    const auto value = shown ? shown(at) : std::string();
    auto* l = lv_obj_get_child(bar, 2);
    if (value != lv_label_get_text(l)) lv_label_set_text(l, value.c_str());
}
}
lv_obj_t* Application::page_list(lv_obj_t* parent) {
    auto* list = scroll_list(parent, 0);
    page_list_ = list; page_list_page_ = page_;
    return list;
}
lv_obj_t* Application::list_bar(lv_obj_t* list, const std::string& name, const BarLook& look, Action action, InstanceId id, std::string symbol, int number) {
    // A module's bar, compact: its name on the left, what it is now on the right.
    lv_obj_t* bar = nullptr;
    if (look.still) bar = panel(list, 0, 0, compact_width, compact_height);
    else {
        bar = button(list, "", 0, 0, compact_width, action, id, std::move(symbol), number);
        lv_obj_set_height(bar, compact_height);
        lv_obj_delete(lv_obj_get_child(bar, 0));
        lv_obj_set_style_border_color(bar, color(border), 0);
        lv_obj_set_style_border_color(bar, color(text), LV_STATE_FOCUS_KEY);
    }
    lv_obj_set_style_radius(bar, 10, 0); lv_obj_set_style_clip_corner(bar, true, 0);
    if (look.edge) lv_obj_set_style_border_color(bar, color(look.edge), 0);
    auto* fill = panel(bar, 0, 0, look.lit ? compact_width : 0, compact_height, look.fill ? look.fill : setting_fill);
    lv_obj_set_style_radius(fill, 0, 0); lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_CLICKABLE);
    // The value's room, roughly by its length; the name has the rest.
    const int value_room = look.value.empty() ? 0 : std::min(static_cast<int>(look.value.size()) * 13 + 8, 180);
    const int name_width = compact_width - 24 - value_room;
    auto* title = label(bar, look.keep_case ? name : upper(name), 12, look.caption.empty() ? 11 : 2, name_width, text, &peepo_bold_14);
    lv_obj_set_style_text_letter_space(title, 1, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT); lv_obj_set_height(title, 18);
    auto* value = label(bar, look.value, compact_width - 12 - std::max(value_room, 8), 9, std::max(value_room, 8), look.value_ink ? look.value_ink : text, &peepo_bold_18);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT); lv_obj_set_height(value, 22);
    if (!look.caption.empty()) {
        auto* caption = label(bar, look.caption, 12, 21, name_width, look.caption_ink ? look.caption_ink : muted, &peepo_mono_14);
        lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT); lv_obj_set_height(caption, 16);
    }
    return bar;
}
void Application::choice_bar(lv_obj_t* list, const std::vector<std::string>& choices, int chosen, Action action, InstanceId id, std::string symbol, int width,
                             std::uint32_t fill) {
    // Like a module's three-way mode: every choice shows, the chosen one lit.
    const int wide = width ? width : compact_width;
    auto* bar = panel(list, 0, 0, wide, compact_height);
    lv_obj_set_style_radius(bar, 10, 0); lv_obj_set_style_clip_corner(bar, true, 0);
    const int count = std::max(1, static_cast<int>(choices.size()));
    for (int i = 0; i < count; ++i) {
        const int x = i * wide / count, w = (i + 1) * wide / count - x;
        auto* b = button(bar, choices[static_cast<std::size_t>(i)], x, 0, w, action, id, symbol, i);
        lv_obj_set_height(b, compact_height); lv_obj_set_style_radius(b, 0, 0); lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_border_width(b, 2, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_bg_color(b, color(i == chosen ? (fill ? fill : setting_fill) : surface), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(b, 0), color(i == chosen ? text : faint), 0);
        if (i) {
            auto* line = panel(bar, x, 8, 1, compact_height - 16, border);
            lv_obj_set_style_border_width(line, 0, 0); lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}
lv_obj_t* Application::value_bar(lv_obj_t* list, const std::string& name, double low, double high, double step, double value,
                                 std::function<std::string(double)> shown, std::function<void(double)> set) {
    auto* bar = list_bar(list, name, {.value = " "}, Action::Slide);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_PRESS_LOCK);
    auto& b = bindings_.back();
    b.low = low; b.high = high; b.step = step; b.at = value; b.shown = std::move(shown); b.set = std::move(set);
    // Its value takes the right half, as on a module's bar.
    auto* v = lv_obj_get_child(bar, 2);
    lv_obj_set_x(v, compact_width * 2 / 5); lv_obj_set_width(v, compact_width * 3 / 5 - 12);
    lv_obj_set_width(lv_obj_get_child(bar, 1), compact_width * 54 / 100);
    paint_slide(bar, b.at, b.low, b.high, b.shown);
    return bar;
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
            message(naming_scene_ ? "Up to 12 characters." : renaming_ ? "Rename keeps modules, values and cables." : "New patch starts with IN connected to OUT.");
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
                const auto* source = port_of(*m, b.symbol, Direction::Output);
                lv_obj_set_style_line_color(preview_, color(cable_cv_ ? control_ink : source && source->type == PortType::Midi ? midi_ink : accent), 0);
                lv_obj_set_style_line_width(preview_, 2, 0);
                if (cable_cv_) preview_points_[0] = {static_cast<lv_value_precise_t>(m->x + module_width / 2 - pan_x_), static_cast<lv_value_precise_t>(m->y - pan_y_)};
                else preview_points_[0] = {static_cast<lv_value_precise_t>(m->x + module_width - pan_x_), static_cast<lv_value_precise_t>(m->y + (from ? from->centre : module_height_of(*m) / 2) - pan_y_)};
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
                        if (item.id != b.id && cx >= item.x && cx < item.x + module_width && cy >= item.y && cy < item.y + module_height_of(item)) {
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
                            std::abs(point.x + pan_x_ - item.x - module_width / 2) <= 24 && std::abs(point.y - 72 + pan_y_ - item.y - module_height_of(item)) <= 24) destination = item.id;
                if (destination && code == LV_EVENT_RELEASED) {
                    const auto outputs = cv_ports(*m, Direction::Output);
                    cv_from_ = cable_source_; cv_to_ = destination; cv_only_ = true;
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
    if (b.action == Action::LooperCommand) {
        if (looper_binding_) {
            // Binding: the command is picked for the next stomp, not played.
            if (code == LV_EVENT_PRESSED) {
                looper_bind_command_ = b.number;
                looper_bind_target_ = b.symbol == "quick" ? 0 : looper_target_;
                message("Press a stomp to send " + looper_command_name(b.number) + " to the " + looper_target_name(looper_bind_target_) + ".");
                rebuild_ = true;
            }
            return;
        }
        if (code == LV_EVENT_PRESSED && !looper_pressed_.contains(target)) {
            // Pin SELECTED at press time so a later selection change still
            // releases the loop that received the original press.
            const int aim = b.symbol == "quick" ? looper_selected_ + 1 : looper_target_ == 0 ? looper_selected_ + 1 : looper_target_;
            if (engine_.looper_command(b.id, b.number, aim, true)) looper_pressed_[target] = {b.id, b.number, aim};
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (const auto held = looper_pressed_.find(target); held != looper_pressed_.end()) {
                engine_.looper_command(held->second.id, held->second.command, held->second.target, false);
                looper_pressed_.erase(held);
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
        const bool segments = segmented_bar(target);
        const bool slides = segments || ((it->kind == ParameterKind::Continuous || it->kind == ParameterKind::Integer) && !is_euclidean_toggle(*it));
        const int span = bar_span(target);
        // A segmented mode takes the choice under the finger.
        const auto chosen = [&] { auto p = *it; p.value = segment_at(target, p, point.x); return p; };
        // Relative, as an encoder is: the value moves by how far the finger
        // goes, the whole width for the whole range, so touching never jumps it.
        const auto dragged = [&] {
            auto p = *it;
            p.value = value_from_normalized(p, std::clamp(drag_origin_ + static_cast<double>(point.x - press_.x) / span, 0.0, 1.0));
            if (p.kind == ParameterKind::Integer) p.value = std::round(p.value);
            return p;
        };
        if (code == LV_EVENT_PRESSED) {
            press_ = point; bar_moving_ = bar_dragged_ = bar_scrolling_ = false;
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
            // A finger going up or down is scrolling the page, even if it
            // drifts sideways later: the bar never takes that press.
            auto* indev = lv_indev_active();
            if (!bar_moving_ && !bar_scrolling_) {
                if (std::abs(dx) > 12 && std::abs(dx) > 2 * std::abs(dy) && !(indev && lv_indev_get_scroll_obj(indev))) {
                    bar_moving_ = bar_dragged_ = true;
                    // A sideways drag is the value's: the list must not scroll under it.
                    if (parameter_list_) lv_obj_remove_flag(parameter_list_, LV_OBJ_FLAG_SCROLLABLE);
                } else if (std::abs(dy) > 6 || (indev && lv_indev_get_scroll_obj(indev))) bar_scrolling_ = true;
            }
            if (bar_moving_ && segments) paint_bar(target, chosen());
            else if (bar_moving_) { const auto p = dragged(); paint_bar(target, p); stream_parameter(b.id, b.symbol, p.value, true); update_euclidean(); }
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (bar_moving_) {
                // Release sends the final position.
                if (code == LV_EVENT_RELEASED && segments) {
                    if (const auto p = chosen(); p.value != std::round(it->value)) submit(SetParameter{b.id, b.symbol, p.value, true});
                    else paint_bar(target, *it);
                } else if (code == LV_EVENT_RELEASED) { const auto p = dragged(); paint_bar(target, p); stream_parameter(b.id, b.symbol, p.value, true); update_euclidean(); }
                else if (segments) paint_bar(target, *it);
                if (parameter_list_) lv_obj_add_flag(parameter_list_, LV_OBJ_FLAG_SCROLLABLE);
                bar_moving_ = false;
            }
        } else if (code == LV_EVENT_LONG_PRESSED && !bar_moving_ && !bar_scrolling_ && b.symbol != general_output_symbol) {
            // Held still: the control's own page, MIDI learn and per scene. The release that follows is not a tap.
            bar_dragged_ = true;
            open_learn(b.id, b.symbol);
        } else if (code == LV_EVENT_CLICKED && !bar_dragged_ && !bar_scrolling_ && segments) {
            if (const auto p = chosen(); p.value != std::round(it->value)) submit(SetParameter{b.id, b.symbol, p.value, true});
        } else if (code == LV_EVENT_CLICKED && !bar_dragged_ && !slides) submit(SetParameter{b.id, b.symbol, stepped_choice(*it), true});
        return;
    }
    if (b.action == Action::Slide) {
        // As a module's bar: relative, a sideways drag over the whole width for
        // the whole range; a swipe up or down scrolls the list instead.
        lv_point_t point{};
        if (auto* indev = lv_indev_active()) lv_indev_get_point(indev, &point);
        auto* list = lv_obj_get_parent(target);
        const auto dragged = [&] {
            double v = b.low + std::clamp(drag_origin_ + static_cast<double>(point.x - press_.x) / bar_span(target), 0.0, 1.0) * (b.high - b.low);
            if (b.step > 0) v = b.low + std::round((v - b.low) / b.step) * b.step;
            return std::clamp(v, b.low, b.high);
        };
        if (code == LV_EVENT_PRESSED) {
            press_ = point; bar_moving_ = bar_scrolling_ = false;
            drag_origin_ = b.high > b.low ? (b.at - b.low) / (b.high - b.low) : 0;
        } else if (code == LV_EVENT_PRESSING) {
            const int dx = point.x - press_.x, dy = point.y - press_.y;
            auto* indev = lv_indev_active();
            if (!bar_moving_ && !bar_scrolling_) {
                if (std::abs(dx) > 12 && std::abs(dx) > 2 * std::abs(dy) && !(indev && lv_indev_get_scroll_obj(indev))) {
                    bar_moving_ = true; lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLLABLE);
                } else if (std::abs(dy) > 6 || (indev && lv_indev_get_scroll_obj(indev))) bar_scrolling_ = true;
            }
            if (bar_moving_) paint_slide(target, dragged(), b.low, b.high, b.shown);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (bar_moving_) {
                if (code == LV_EVENT_RELEASED) {
                    b.at = dragged(); paint_slide(target, b.at, b.low, b.high, b.shown);
                    // The callback may rebuild the page, and this binding with it.
                    if (const auto set = b.set) set(b.at);
                } else paint_slide(target, b.at, b.low, b.high, b.shown);
                lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
                bar_moving_ = false;
            }
        }
        return;
    }
    if (code != LV_EVENT_CLICKED) return;
    switch (b.action) {
    case Action::ModulePage:
        if (pedal_page_ != b.number) { pedal_page_ = b.number; parameter_focus_ = 0; parameter_scroll_ = 0; adjusting_ = false; rebuild_ = true; }
        break;
    case Action::LooperSection: looper_section_ = b.number; looper_binding_ = false; parameter_focus_ = 0; parameter_scroll_ = 0; rebuild_ = true; break;
    case Action::LooperBind:
        looper_binding_ = !looper_binding_; looper_bind_command_ = -1; rebuild_ = true;
        message(looper_binding_ ? "Pick Act on, tap a command, then press a stomp. Pressing it again on the same command unbinds it." : "Binding done.");
        break;
    case Action::LooperTarget: {
        const auto* m = find_module(graph_, b.id);
        const int count = m ? std::clamp(int(parameter(*m, "loops").value), 1, 8) : 1;
        looper_target_ = (looper_target_ + 1) % (count + 2); rebuild_ = true; break;
    }
    case Action::LooperSelect: looper_selected_ = b.number; submit(SetParameter{b.id, "selected", double(b.number)}); break;
    case Action::LooperCount: {
        if (const auto* m = find_module(graph_, b.id)) submit(SetParameter{b.id, "loops", std::clamp(parameter(*m, "loops").value + b.number, 1.0, 8.0)});
        break;
    }
    case Action::Navigate: navigate(static_cast<Page>(b.number)); break;
    case Action::Add: add_module(b.symbol); break;
    case Action::Duplicate: {
        const auto* m = find_module(graph_, b.id);
        const int height = m ? module_height_of(*m) : module_height;
        if (const auto slot = vacant_slot(height)) {
            const auto [x, y] = *slot;
            submit(DuplicateModule{b.id, x, y});
            reveal(x, y, height); navigate(Page::Patch);
        } else message("No free module slot remains on this canvas.");
        break;
    }
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
        // From the screen: also written into the current scene.
        const auto* m = find_module(graph_, b.id); if (m) submit(SetBypass{b.id, !m->bypassed, true}); break;
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
    case Action::MidiToggle:
        change_midi([&](midi::Settings& s) { auto& flag = b.number ? s.start_stop : s.clock_out; flag = !flag; });
        break;
    case Action::MidiTempoSource:
        change_midi([&](midi::Settings& s) { s.tempo_source = b.number ? midi::TempoSource::Clock : midi::TempoSource::Patch; });
        break;
    case Action::MidiChannel:
        // Off, channels 1 to 16, any: round and round.
        change_midi([&](midi::Settings& s) { s.program_channel = (s.program_channel + b.number + midi::any_channel + 1) % (midi::any_channel + 1); });
        break;
    case Action::MidiDevice:
        change_midi([&](midi::Settings& s) {
            auto& off = b.number ? s.outputs_off : s.inputs_off;
            if (!off.erase(b.symbol)) off.insert(b.symbol);
        });
        break;
    case Action::MidiLearn:
        learn_waiting_ = false;
        learn_request_ = submit(LearnMidi{learn_id_, learn_symbol_});
        break;
    case Action::MidiForget: submit(LearnMidi{learn_id_, learn_symbol_, true}); break;
    case Action::SceneSelect: {
        const auto& scenes = graph_.scenes.scenes;
        const auto index = static_cast<std::size_t>(b.number);
        if (index >= scenes.size()) break;
        const auto pick = scene_pick_;
        scene_pick_ = ScenePick::None; rebuild_ = true;
        const auto current = graph_.scenes.current;
        const auto name = scenes[index].name;
        if (pick == ScenePick::CopyTo) {
            if (index == current) { message("A scene copied over itself stays as it is."); break; }
            const auto from = scenes[current].name;
            if (const auto request = submit(CopyScene{current, index})) accepted_notes_[request] = [from, name] { return "Copied " + from + " over " + name + "."; };
        } else if (pick == ScenePick::Delete) {
            if (index == current && scenes.size() > 1) { message("Play another scene before deleting this one."); break; }
            ask("Delete scene", "Delete " + name + "?", scenes.size() == 1 ? "The patch goes back to having no scenes; its parameters are the same in every scene again."
                                                                            : "Its stored state is lost. The other scenes stay as they are.",
                "Delete", [this, index, name] {
                    navigate(Page::Scenes);
                    if (const auto request = submit(DeleteScene{index})) accepted_notes_[request] = [name] { return "Deleted " + name + "."; };
                });
            confirmation_.back = Page::Scenes;
        } else if (const auto request = submit(SelectScene{index})) accepted_notes_[request] = [name] { return "Playing " + name + "."; };
        break;
    }
    case Action::SceneAdd:
        if (const auto request = submit(AddScene{}))
            accepted_notes_[request] = [this] { return "Added " + graph_.scenes.scenes[graph_.scenes.current].name + ": the patch as it is now."; };
        break;
    case Action::SceneRename:
        if (graph_.scenes.scenes.empty()) break;
        if (pending()) { message("Wait for pending edits before naming a scene."); break; }
        naming_scene_ = true; renaming_ = saving_as_ = false;
        draft_name_ = graph_.scenes.scenes[graph_.scenes.current].name;
        message("Up to 12 characters.");
        navigate(Page::NamePatch); break;
    case Action::ScenePick:
        scene_pick_ = static_cast<ScenePick>(b.number);
        // The only scene needs no picking.
        if (scene_pick_ == ScenePick::Delete && graph_.scenes.scenes.size() == 1) {
            Binding only{this, Action::SceneSelect};
            handle(only, e);
            break;
        }
        if (scene_pick_ == ScenePick::CopyTo) message("Copy " + graph_.scenes.scenes[graph_.scenes.current].name + " over which scene?");
        else message("Delete which scene? Not the one playing.");
        rebuild_ = true;
        break;
    case Action::SceneCancel: scene_pick_ = ScenePick::None; message("Tap a scene to play it."); rebuild_ = true; break;
    case Action::SceneMode: {
        if (graph_.scenes.scenes.empty()) { message("Add a scene first: SCENES at the top."); break; }
        const bool scene_mode = !graph_.scenes.scene_mode;
        if (const auto request = submit(SetSceneMode{scene_mode}))
            accepted_notes_[request] = [this, scene_mode] { return scene_mode ? scene_hint() : std::string("Stomps act as assigned."); };
        break;
    }
    case Action::SceneMark: {
        const bool marked = b.number == 1;
        if (marked == scene_parameter_marked(graph_, learn_id_, learn_symbol_)) break;
        const auto* m = find_module(graph_, learn_id_);
        const auto name = m ? parameter(*m, learn_symbol_).name : learn_symbol_;
        if (const auto request = submit(MarkSceneParameter{learn_id_, learn_symbol_, marked}))
            accepted_notes_[request] = [name, marked] { return name + (marked ? " is now per scene." : " is now the same in every scene."); };
        break;
    }
    case Action::SceneParameter: open_module(b.id); break;
    case Action::AddLoadMessage: {
        auto list = graph_.load_messages;
        if (list.size() < max_load_messages) { list.push_back({}); submit(SetLoadMessages{list}); }
        break;
    }
    case Action::RemoveLoadMessage: {
        auto list = graph_.load_messages;
        if (static_cast<std::size_t>(b.number) < list.size()) { list.erase(list.begin() + b.number); submit(SetLoadMessages{list}); }
        break;
    }
    case Action::LoadMessage: {
        // The channel steps round from 1 to 16.
        auto list = graph_.load_messages;
        if (const auto i = static_cast<std::size_t>(b.number / 8); i < list.size()) {
            list[i].channel = (list[i].channel - 1 + (b.number % 8 ? 1 : 15)) % 16 + 1;
            submit(SetLoadMessages{list});
        }
        break;
    }
    case Action::ClockRestart:
        if (midi_) { midi_->request_restart(); message("The clock restarts: Stop, then Start."); }
        break;
    case Action::ChooseFile: selected_ = b.id; navigate(Page::FilePicker); break;
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
        renaming_ = b.number == 1; saving_as_ = b.number == 2; naming_scene_ = false;
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
        confirmation_.back = Page::PatchActions;
        break;
    }
    case Action::StartupStored: {
        if (!store_) break;
        const bool already = store_->startup() == b.symbol;
        const auto issue = store_->set_startup(already ? std::string() : b.symbol);
        message(!issue.empty() ? issue : already ? "No startup patch: the engine keeps whatever it plays." : "This patch loads when the instrument starts.");
        refresh_stored(); rebuild_ = true; break;
    }
    case Action::OpenPatch: patch_selected_ = b.symbol; navigate(Page::PatchActions); break;
    case Action::AssignControl:
        editing_control_ = b.symbol; picking_module_ = 0; list_scroll_.erase(Page::AssignPicker);
        message("Choose what " + control_label(b.symbol) + " controls in this patch.");
        navigate(Page::AssignPicker); break;
    case Action::EditControl:
        editing_control_ = b.symbol;
        if (pinned(b.symbol) || (store_ && store_->global_controls().contains(b.symbol))) navigate(Page::AssignmentDetail);
        else {
            picking_module_ = 0; list_scroll_.erase(Page::AssignPicker);
            message("Choose what " + control_label(b.symbol) + " controls in this patch.");
            navigate(Page::AssignPicker);
        }
        break;
    case Action::ClearControl:
        if (pinned(b.symbol)) submit(Unassign{b.symbol});
        else if (store_ && store_->global_controls().contains(b.symbol)) {
            const auto issue = store_->set_global_control(b.symbol, {});
            message(issue.empty() ? control_label(b.symbol) + " cleared." : issue);
        }
        navigate(Page::Controls); break;
    case Action::PickModule: picking_module_ = b.id; list_scroll_.erase(Page::AssignPicker); rebuild_ = true; break;
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
        // The first tap picks a card; tapping it again, or Add, adds it, in
        // the variant its pills show.
        if (b.symbol.empty() || b.symbol == browser_pick_ ||
            (browser_cards_.count(b.symbol) && browser_cards_.count(browser_pick_) && browser_cards_[b.symbol] == browser_cards_[browser_pick_])) {
            if (!browser_pick_.empty()) add_module(browser_pick_);
        } else browser_pick(b.symbol, false);
        break;
    case Action::BrowserVariant:
        // A pill picks its variant and the card; tapping it again adds it.
        if (b.symbol == browser_pick_) add_module(browser_pick_);
        else browser_pick(b.symbol, false);
        break;
    case Action::CvChoice:
        if (cv_output_.empty()) {
            const auto* from = find_module(graph_, cv_from_);
            const auto* to = find_module(graph_, cv_to_);
            const auto inputs = from && to ? connectable_ports(*from, *to, b.symbol, cv_only_) : std::vector<const Port*>{};
            if (inputs.size() == 1) {
                submit(Connect{{cv_from_, cv_to_, b.symbol, inputs.front()->symbol}});
                navigate(Page::Patch);
            } else {
                cv_output_ = b.symbol; list_scroll_.erase(Page::CvConnect); rebuild_ = true;
            }
        } else { submit(Connect{{cv_from_, cv_to_, cv_output_, b.symbol}}); navigate(Page::Patch); }
        break;
    default: break;
    }
}
void Application::add_module(const std::string& uri) {
    const auto* d = definition(uri);
    const int height = d ? card_height(d->ports) : module_height;
    const auto slot = vacant_slot(height);
    if (!slot) { message("No free module slot remains on this canvas."); return; }
    const auto [x, y] = *slot;
    if (const auto request = submit(AddModule{uri, x, y})) { pending_[request] = PendingKind::Add; adding_[request] = d ? d->name : uri; }
    reveal(x, y, height); navigate(Page::Patch);
}
void Application::commit_name() {
    if (naming_scene_) {
        if (pending()) { message("Wait for pending edits before naming a scene."); return; }
        if (auto issue = validate_scene_name(draft_name_); !issue.empty()) { message(issue); return; }
        const auto index = graph_.scenes.current;
        const auto name = draft_name_;
        navigate(Page::Scenes);
        if (const auto request = submit(RenameScene{index, name})) accepted_notes_[request] = [name] { return "Renamed " + name + "."; };
        return;
    }
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
        renaming_ = naming_scene_ = false; saving_as_ = true; draft_name_ = graph_.name;
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
    save_restore_id_ = graph_.patch_id;
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
    Command command = load_command(*patch, graph_.patch_id.empty() ? std::nullopt : store_->find(graph_.patch_id));
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
std::vector<Application::Pill> Application::header_pills() const {
    const bool ready = engine_.state() == EngineState::Ready;
    std::vector<Pill> pills;
    switch (page_) {
    case Page::Settings:
        pills.push_back({"Controls", Action::Navigate, 0, {}, static_cast<int>(Page::Controls)});
        if (midi_) pills.push_back({"MIDI", Action::Navigate, 0, {}, static_cast<int>(Page::Midi)});
        pills.push_back({"Diagnostics", Action::Navigate, 0, {}, static_cast<int>(Page::Diagnostics)});
        if (power_off_ || restart_) pills.push_back({"Power", Action::Navigate, 0, {}, static_cast<int>(Page::Power)});
        break;
    case Page::Patches:
        // What can be done to the patch playing.
        if (!store_) {
            pills.push_back({"Save", Action::SaveSession, 0, {}, 0, true, !ready});
            pills.push_back({"Rename", Action::NameEntry, 0, {}, 1, false, !ready});
            break;
        }
        pills.push_back({"Save", Action::SavePatch, 0, {}, 0, true, !ready});
        pills.push_back({"Save as", Action::SavePatch, 0, {}, 1, false, !ready});
        pills.push_back({"Rename", Action::NameEntry, 0, {}, 1, false, !ready});
        pills.push_back({"Revert", Action::Revert, 0, {}, 0, false, graph_.patch_id.empty() || !graph_.dirty});
        if (midi_) pills.push_back({"MIDI", Action::Navigate, 0, {}, static_cast<int>(Page::PatchMidi)});
        break;
    case Page::PatchActions:
        if (const auto patch = store_ ? store_->find(patch_selected_) : std::nullopt) {
            const bool playing = patch->id == graph_.patch_id;
            // Loading the patch playing goes back to its saved revision.
            pills.push_back({playing ? "Load saved" : "Load", Action::LoadStored, 0, patch->id, 0, true, !patch->revision || (playing && !graph_.dirty)});
            pills.push_back({"Delete", Action::DeleteStored, 0, patch->id});
        }
        break;
    case Page::PatchMidi:
        pills.push_back({"+ Add", Action::AddLoadMessage, 0, {}, 0, true, graph_.load_messages.size() >= max_load_messages || !ready});
        break;
    case Page::NamePatch: pills.push_back({naming_scene_ ? "Rename" : saving_as_ ? "Save" : renaming_ ? "Rename" : "Create", Action::CommitName, 0, {}, 0, true}); break;
    case Page::Scenes: {
        // While a tap picks a scene, only the way out.
        if (scene_pick_ != ScenePick::None) { pills.push_back({"Cancel", Action::SceneCancel, 0, {}, 0, true}); break; }
        const auto count = graph_.scenes.scenes.size();
        pills.push_back({"Add", Action::SceneAdd, 0, {}, 0, true, count >= max_scenes || !ready});
        pills.push_back({"Rename", Action::SceneRename, 0, {}, 0, false, !count || !ready});
        pills.push_back({"Copy to", Action::ScenePick, 0, {}, static_cast<int>(ScenePick::CopyTo), false, count < 2 || !ready});
        pills.push_back({"Delete", Action::ScenePick, 0, {}, static_cast<int>(ScenePick::Delete), false, !count || !ready});
        break;
    }
    case Page::Tempo: pills.push_back({"Tap", Action::Tap, 0, {}, 0, true}); break;
    case Page::MidiLearn: {
        const auto* m = find_module(graph_, learn_id_);
        const auto it = m ? std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == learn_symbol_; }) : std::vector<Parameter>::const_iterator{};
        if (!m || it == m->parameters.end()) break;
        if (learn_waiting_ || learn_request_) { pills.push_back({"Cancel", Action::Navigate, 0, {}, static_cast<int>(Page::Parameters), true}); break; }
        pills.push_back({"Learn", Action::MidiLearn, 0, {}, 0, true});
        pills.push_back({"Forget", Action::MidiForget, 0, {}, 0, false, !it->midi});
        break;
    }
    case Page::AssignmentDetail:
        pills.push_back({"Assign", Action::AssignControl, 0, editing_control_});
        pills.push_back({"Clear", Action::ClearControl, 0, editing_control_});
        break;
    case Page::ConfirmAction:
        pills.push_back({confirmation_.button, Action::ConfirmAction, 0, {}, 0, true});
        pills.push_back({"Cancel", Action::CancelAction});
        break;
    case Page::ConfirmReplace:
        pills.push_back({"Replace patch", Action::Confirm, 0, {}, 0, true});
        pills.push_back({"Cancel", Action::CancelReplace});
        break;
    default: break;
    }
    return pills;
}
void Application::rebuild() {
    rebuild_ = news_ = false;
    rebuilt_ms_ = clock_ms_;
    if (!screen_) {
        screen_ = lv_obj_create(nullptr);
        lv_obj_remove_style_all(screen_); lv_obj_set_size(screen_, 800, 480);
        lv_obj_set_style_bg_color(screen_, color(background), 0); lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
        lv_screen_load(screen_);
    } else {
        if (browser_list_) browser_scroll_ = lv_obj_get_scroll_y(browser_list_);
        if (parameter_list_ && page_ == Page::Parameters) parameter_scroll_ = lv_obj_get_scroll_y(parameter_list_);
        if (page_list_) list_scroll_[page_list_page_] = lv_obj_get_scroll_y(page_list_);
        lv_obj_clean(screen_);
    }
    browser_list_ = browser_add_ = parameter_list_ = page_list_ = nullptr; browser_cards_.clear(); browser_pills_.clear();
    bars_.clear();
    bindings_.clear(); module_widgets_.clear(); input_jacks_.clear(); cv_inputs_.clear(); dots_.clear(); cables_.clear(); canvas_ = nullptr; footer_ = nullptr;
    tempo_value_ = cpu_value_ = network_value_ = nullptr;
    tuner_note_ = tuner_detail_ = tuner_status_ = tuner_needle_ = nullptr;
    release_looper(); looper_cards_ = {}; looper_commands_.clear();
    euclidean_status_ = nullptr;
    euclidean_rings_ = {}; euclidean_halos_ = {}; euclidean_dots_ = {}; euclidean_step_labels_ = {}; euclidean_tiles_ = {};
    flow_line_ = flow_middle_ = nullptr; flow_drawn_.reset();
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
    case Page::Patch:
        // In scene mode, the scene playing.
        if (graph_.scenes.scene_mode && !graph_.scenes.scenes.empty()) section += " // " + upper(graph_.scenes.scenes[graph_.scenes.current].name);
        break;
    case Page::Browser: section = "BROWSER"; title.clear(); break;
    case Page::Parameters: case Page::ModuleActions: {
        section = page_ == Page::Parameters ? "MODULE" : "ACTIONS";
        // Edits here go into the current scene: say which.
        if (page_ == Page::Parameters && !graph_.scenes.scenes.empty()) section += " // " + upper(graph_.scenes.scenes[graph_.scenes.current].name);
        const auto* m = find_module(graph_, selected_);
        title = m ? m->name : "Module";
        break;
    }
    case Page::Scenes:
        section = "SCENES";
        title = graph_.scenes.scenes.empty() ? "Scenes" : graph_.scenes.scenes[graph_.scenes.current].name;
        break;
    case Page::Patches: section = "PATCHES"; break;
    case Page::PatchActions: {
        section = "PATCHES";
        const auto it = std::find_if(stored_.begin(), stored_.end(), [&](const StoredPatch& p) { return p.id == patch_selected_; });
        title = it != stored_.end() ? it->name : "Saved patch";
        break;
    }
    case Page::Settings: section = "SETTINGS"; title = engine_.capabilities().simulation ? "Simulator settings" : "Engine settings"; break;
    case Page::NamePatch:
        section = naming_scene_ ? "SCENES" : "PATCHES";
        title = naming_scene_ ? "Rename scene" : saving_as_ ? "Save as a new patch" : renaming_ ? "Rename patch" : "New patch";
        break;
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
    case Page::Midi: section = "SETTINGS"; title = "MIDI"; break;
    case Page::PatchMidi: section = "PATCHES"; title = "MIDI on load"; break;
    case Page::MidiLearn: {
        section = "PARAMETER";
        const auto* m = find_module(graph_, learn_id_);
        title = m ? m->name : "Module";
        break;
    }
    case Page::Halting: section = "POWER"; title = restarting_ ? "Restarting" : "Shutting down"; break;
    }
    // The dot is the engine: green when it answers.
    auto* dot = panel(header, 24, title.empty() ? 32 : 17, 8, 8, ready ? accent : warning);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    auto* crumb = label(header, "PEEPO // " + section, 40, title.empty() ? 27 : 12, 0, faint, &peepo_mono_14);
    lv_obj_set_style_text_letter_space(crumb, 2, 0);
    lv_obj_t* heading = nullptr;
    if (!title.empty()) {
        heading = label(header, upper(title), 24, 32, patch ? 312 : page_ == Page::Settings ? 330 : page_ == Page::Parameters ? 440 : 600, text, &peepo_bold_24);
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
        auto* add = primary(pill(button(header, "+ Add", 348, 12, 96, Action::Navigate, 0, {}, static_cast<int>(Page::Browser))));
        // Nothing can be added until the engine answers again.
        if (!ready) lv_obj_add_state(add, LV_STATE_DISABLED);
        pill(button(header, "Scenes", 452, 12, 96, Action::Navigate, 0, {}, static_cast<int>(Page::Scenes)));
        pill(button(header, "Patches", 556, 12, 104, Action::Navigate, 0, {}, static_cast<int>(Page::Patches)));
        pill(button(header, "Settings", 672, 12, 104, Action::Navigate, 0, {}, static_cast<int>(Page::Settings)));
    } else if (const auto wanted = header_pills(); !wanted.empty()) {
        // What the page can do, as pills as wide as their names; its main action white.
        auto* pills = lv_obj_create(header); lv_obj_remove_style_all(pills);
        lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(pills, 8, 0);
        lv_obj_remove_flag(pills, LV_OBJ_FLAG_CLICKABLE);
        for (const auto& w : wanted) {
            auto* b = pill(button(pills, w.name, 0, 0, 0, w.action, w.id, w.symbol, w.number));
            lv_obj_set_width(b, LV_SIZE_CONTENT); lv_obj_set_style_pad_hor(b, 18, 0);
            if (w.primary) primary(b);
            if (w.disabled) lv_obj_add_state(b, LV_STATE_DISABLED);
        }
        lv_obj_align(pills, LV_ALIGN_RIGHT_MID, -24, 0);
        // The title takes the room the pills leave.
        if (heading) { lv_obj_update_layout(header); lv_obj_set_width(heading, std::min(lv_obj_get_x(pills) - 40, 600)); }
    } else if (const auto* m = page_ == Page::Parameters ? find_module(graph_, selected_) : nullptr; m && module_available(*m)) {
        // On or bypassed, in the module's colour, and its other actions.
        auto* pills = lv_obj_create(header); lv_obj_remove_style_all(pills);
        lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(pills, 8, 0);
        lv_obj_remove_flag(pills, LV_OBJ_FLAG_CLICKABLE);
        if (looper_uri(m->uri)) {
            constexpr const char* sections[]{"Commands", "Levels", "Rate", "Global"};
            constexpr int widths[]{104, 80, 64, 80};
            for (int s = 0; s < 4; ++s) {
                auto* tab = pill(button(pills, sections[s], 0, 0, widths[s], Action::LooperSection, m->id, {}, s));
                if (looper_section_ == s) primary(tab);
            }
        }
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
    case Page::Midi: midi_page(content); break;
    case Page::MidiLearn: midi_learn(content); break;
    case Page::PatchMidi: patch_midi(content); break;
    case Page::PatchActions: patch_actions(content); break;
    case Page::Scenes: scenes_page(content); break;
    case Page::Halting: halting(content); break;
    }
    if (page_list_) {
        lv_obj_update_layout(page_list_);
        if (const auto it = list_scroll_.find(page_list_page_); it != list_scroll_.end()) lv_obj_scroll_to_y(page_list_, it->second, LV_ANIM_OFF);
        scroll_rail(content, page_list_, 0, 360, accent);
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
    if (patch) {
        // Where Back is elsewhere: what the stomps do, stomps as assigned or
        // scenes, saved with the patch. A tap changes it.
        const bool scene_mode = graph_.scenes.scene_mode;
        auto* mode = slim(button(foot, scene_mode ? "Scene" : "Stomp", 14, 0, 112, Action::SceneMode));
        if (scene_mode) {
            lv_obj_set_style_bg_color(mode, color(accent), 0); lv_obj_set_style_border_width(mode, 0, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(mode, 0), color(background), 0);
        }
    }
    const int message_x = page_ == Page::Halting ? 24 : 142;
    footer_ = label(foot, message_, message_x, 7, patch && selected_cable_ ? 280 - message_x - 8 : 592 - message_x, faint, &peepo_mono_14);
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
        unsigned out_channels = 1, in_channels = 1; bool cv = false, midi = false;
        if (const auto* source = find_module(graph_, edge.from)) for (const auto& p : module_ports(*source))
            if (p.symbol == edge.output && p.direction == Direction::Output) { out_channels = p.channels; cv = p.type == PortType::Cv; midi = p.type == PortType::Midi; }
        if (const auto* sink = find_module(graph_, edge.to)) for (const auto& p : module_ports(*sink))
            if (p.symbol == edge.input && p.direction == Direction::Input) in_channels = p.channels;
        for (unsigned channel = 0; channel < std::max(out_channels, in_channels); ++channel) {
        auto* line = lv_line_create(parent);
        lv_obj_set_style_line_color(line, color(selected || !routing.error.empty() ? warning : cv ? control_ink : midi ? midi_ink : cable_ink), 0);
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
        const int height = module_height_of(m);
        auto* card = panel(parent, m.x - pan_x_, m.y - pan_y_, module_width, height);
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
        // The name in the largest type that shows it whole: no word broken,
        // no line cut off. Only a name too long even at 10 px is shortened.
        const auto title = upper(m.name);
        const lv_font_t* font = &peepo_bold_10;
        for (const auto* f : {&peepo_bold_14, &peepo_bold_12})
            if (text_fits(title, f, module_width - 8, height - 10)) { font = f; break; }
        auto* name = label(card, title, 0, 0, module_width - 8, m.bypassed ? faint : text, font);
        lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_update_layout(name);
        if (lv_obj_get_height(name) > height - 10) {
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_height(name, height - 10);
        }
        lv_obj_align(name, LV_ALIGN_CENTER, 0, 0);
        const auto& ports = module_ports(m);
        // The stomps that act on it, by letter in a white dot on its top right
        // corner, clear of the name and of the connectors. Not in scene mode,
        // where the stomps play scenes.
        if (const auto stomps = stomps_on(graph_, m); !stomps.empty() && !m.fixed && !graph_.scenes.scene_mode) {
            std::string digits;
            for (const auto& n : stomps) digits += (digits.empty() ? "" : " ") + n;
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
            if (!side_port(port)) continue;
            const bool output = port.direction == Direction::Output, midi = port.type == PortType::Midi;
            for (unsigned channel = 0; channel < port.channels; ++channel) {
            auto* hit = lv_obj_create(card); lv_obj_remove_style_all(hit);
            const auto place = *jack(m, port.symbol, port.direction, channel);
            lv_obj_set_size(hit, 44, place.height); lv_obj_set_pos(hit, output ? module_width - 22 : -22, place.centre - place.height / 2);
            lv_obj_remove_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(hit, LV_OBJ_FLAG_PRESS_LOCK);
            // Audio is a round jack in the module's colour; MIDI a diamond in its own.
            auto* dot = panel(hit, midi ? 16 : 15, place.height / 2 - (midi ? 6 : 7), midi ? 12 : 14, midi ? 12 : 14, background);
            lv_obj_set_style_radius(dot, midi ? 2 : LV_RADIUS_CIRCLE, 0);
            if (midi) {
                lv_obj_set_style_transform_pivot_x(dot, 6, 0); lv_obj_set_style_transform_pivot_y(dot, 6, 0);
                lv_obj_set_style_transform_rotation(dot, 450, 0);
            }
            lv_obj_set_style_border_width(dot, 3, 0); lv_obj_set_style_border_color(dot, color(midi ? midi_ink : ink), 0);
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
            lv_obj_set_size(hit, 44, 44); lv_obj_set_pos(hit, module_width / 2 - 22, output ? -22 : height - 22);
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
        const auto* port = port_of(*m, symbol, Direction::Input);
        lv_obj_set_style_border_color(jack, color(active ? (valid ? accent : border) : port && port->type == PortType::Midi ? midi_ink : module_ink(*m)), 0);
        lv_obj_set_style_outline_color(jack, color(accent), 0);
        lv_obj_set_style_outline_width(jack, valid ? 4 : 0, 0);
        lv_obj_set_style_outline_opa(jack, LV_OPA_COVER, 0);
    }
}
std::optional<std::pair<int, int>> Application::vacant_slot(int height) const {
    // Prefer the current viewport, then search the larger canvas.
    for (int visible = 1; visible >= 0; --visible)
        for (int y = module_min_y; y <= module_max_y; y += 120)
            for (int x = module_min_x; x <= module_max_x; x += 144) {
                if (visible && (x < pan_x_ + 24 || x + module_width > pan_x_ + 776 || y < pan_y_ + 12 || y + height > pan_y_ + 360)) continue;
                // Clear of every card by 36 across and 54 down, tall ones too.
                const bool occupied = std::any_of(graph_.modules.begin(), graph_.modules.end(), [&](const Module& m) {
                    return std::abs(m.x - x) < module_width + 36 && y < m.y + module_height_of(m) + 54 && m.y < y + height + 54;
                });
                if (!occupied) return std::pair{x, y};
            }
    return {};
}
void Application::reveal(int x, int y, int height) {
    if (x < pan_x_ + 24 || x + module_width > pan_x_ + 776) pan_x_ = std::clamp(x - 24, 0, canvas_width - 800);
    if (y < pan_y_ + 12 || y + height > pan_y_ + 360) pan_y_ = std::clamp(y - 12, 0, canvas_height - 360);
}
void Application::update_cables() {
    // LVGL positions are lazy; resolve the cards before reading coordinates.
    if (canvas_) lv_obj_update_layout(canvas_);
    for (auto& c : cables_) {
        auto a = module_widgets_.find(c.connection.from), b = module_widgets_.find(c.connection.to);
        if (a == module_widgets_.end() || b == module_widgets_.end()) continue;
        const auto* source = find_module(graph_, c.connection.from); const auto* sink = find_module(graph_, c.connection.to);
        if (c.cv) {
            // Top of the source to the underside of the destination.
            sample_cable(c.line, c.points, lv_obj_get_x(a->second) + module_width / 2, lv_obj_get_y(a->second), lv_obj_get_x(b->second) + module_width / 2, lv_obj_get_y(b->second) + (sink ? module_height_of(*sink) : module_height), true);
            continue;
        }
        const auto out = source ? jack(*source, c.connection.output, Direction::Output, c.out_channel) : std::nullopt;
        const auto in = sink ? jack(*sink, c.connection.input, Direction::Input, c.in_channel) : std::nullopt;
        sample_cable(c.line, c.points, lv_obj_get_x(a->second) + module_width, lv_obj_get_y(a->second) + (out ? out->centre : module_height_of(*source) / 2),
                     lv_obj_get_x(b->second), lv_obj_get_y(b->second) + (in ? in->centre : module_height_of(*sink) / 2), false);
    }
}
void Application::browser(lv_obj_t* header, lv_obj_t* content, lv_obj_t* foot) {
    const auto& groups = module_groups();
    // A card per module, or per family: its variants, in the overlay's order.
    struct Card { std::string name; std::vector<const ModuleDefinition*> variants; bool usable = false; };
    std::vector<std::vector<Card>> members(groups.size());
    std::map<std::string, std::pair<std::size_t, std::size_t>> family_cards;
    for (const auto& d : catalogue()) {
        if (d.category == "Endpoint" || d.hidden) continue;
        auto& cards = members[module_group(d.category)];
        const auto at = d.family.empty() ? family_cards.end() : family_cards.find(d.family);
        auto& card = at != family_cards.end() ? members[at->second.first][at->second.second] : cards.emplace_back(Card{d.family.empty() ? d.name : d.family, {}});
        if (!d.family.empty() && at == family_cards.end()) family_cards[d.family] = {module_group(d.category), cards.size() - 1};
        card.variants.push_back(&d);
        card.usable = card.usable || d.availability == Availability::Available;
    }
    for (auto& cards : members) {
        for (auto& c : cards)
            std::stable_sort(c.variants.begin(), c.variants.end(), [](const ModuleDefinition* a, const ModuleDefinition* b) { return a->variant_order < b->variant_order; });
        // Usable modules first, each part alphabetical.
        std::stable_sort(cards.begin(), cards.end(), [](const Card& a, const Card& b) { return a.usable != b.usable ? a.usable : a.name < b.name; });
    }
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
            if (std::any_of(members[gi].begin(), members[gi].end(), [&](const Card& c) { return c.variants[0]->category == name; })) categories.push_back(name);
        for (const auto& c : members[gi])
            if (std::find(categories.begin(), categories.end(), c.variants[0]->category) == categories.end()) categories.push_back(c.variants[0]->category);
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
        for (const auto& c : members[gi]) {
            const auto* d = c.variants[0];
            if (!browser_category_.empty() && d->category != browser_category_) continue;
            const bool usable = c.usable;
            // A family offers the variants it can add, one pill each; the one
            // last picked is what a tap on the card picks.
            std::vector<const ModuleDefinition*> variants;
            for (const auto* v : c.variants) if (v->availability == Availability::Available) variants.push_back(v);
            const ModuleDefinition* chosen = variants.empty() ? d : variants[0];
            if (const auto last = browser_variant_.find(d->family); !d->family.empty() && last != browser_variant_.end())
                for (const auto* v : variants) if (v->uri == last->second) chosen = v;
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
            auto* title = label(card, upper(c.name), 18, 18, 380 - 18 - 16 - lv_obj_get_width(tag) - 12, text, &peepo_bold_20);
            lv_label_set_long_mode(title, LV_LABEL_LONG_DOT); lv_obj_set_height(title, 26);
            // An entry that cannot be added says why instead of what it does.
            const auto& what = d->family_description.empty() ? chosen->description : d->family_description;
            const auto about = usable ? what : "Needs setup: " + (d->availability_reason.empty() ? d->description : d->availability_reason);
            const bool offers = variants.size() > 1;
            auto* description = label(card, about, 18, 54, 344, muted, &lv_font_montserrat_18);
            lv_label_set_long_mode(description, LV_LABEL_LONG_DOT); lv_obj_set_height(description, offers ? 46 : 92);
            if (usable) {
                bind(card, Action::BrowserPick, 0, chosen->uri);
                if (group) lv_group_add_obj(group, card);
                for (const auto* v : variants) { browser_cards_[v->uri] = card; browser_order_.push_back(v->uri); }
            }
            if (offers) {
                auto* row = lv_obj_create(card); lv_obj_remove_style_all(row);
                lv_obj_set_pos(row, 18, 112); lv_obj_set_size(row, 344, 32);
                lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row, 6, 0);
                lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
                for (const auto* v : variants) {
                    auto* pill = lv_button_create(row); lv_obj_remove_style_all(pill);
                    lv_obj_set_size(pill, LV_SIZE_CONTENT, 32); lv_obj_set_style_pad_hor(pill, 12, 0);
                    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
                    lv_obj_set_style_bg_color(pill, color(background), 0); lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
                    lv_obj_set_style_bg_color(pill, color(g.bright), LV_STATE_CHECKED);
                    lv_obj_set_style_bg_opa(pill, LV_OPA_80, LV_STATE_PRESSED);
                    lv_obj_set_style_border_width(pill, 1, 0); lv_obj_set_style_border_color(pill, color(rim), 0);
                    lv_obj_set_style_border_width(pill, 0, LV_STATE_CHECKED);
                    auto* l = label(pill, upper(v->variant), 0, 0, 0, g.bright, &peepo_bold_14);
                    lv_obj_set_style_text_color(l, color(background), LV_STATE_CHECKED);
                    lv_obj_set_style_text_letter_space(l, 1, 0); lv_obj_center(l);
                    if (v == chosen) { lv_obj_add_state(pill, LV_STATE_CHECKED); lv_obj_add_state(l, LV_STATE_CHECKED); }
                    bind(pill, Action::BrowserVariant, 0, v->uri);
                    browser_pills_[v->uri] = pill;
                }
            }
            if (!usable) {
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
    // On a family's card, its pill for this variant, which the card remembers.
    if (const auto* d = definition(uri); d && !d->family.empty() && browser_pills_.count(uri)) {
        browser_variant_[d->family] = uri;
        for (const auto& [other, pill] : browser_pills_) {
            const auto* v = definition(other);
            if (!v || v->family != d->family) continue;
            lv_obj_set_state(pill, LV_STATE_CHECKED, other == uri);
            lv_obj_set_state(lv_obj_get_child(pill, 0), LV_STATE_CHECKED, other == uri);
        }
    }
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
        auto* list = page_list(parent);
        list_bar(list, "Delete module", {}, Action::Remove, m->id);
        list_heading(list, "Module unavailable", warning);
        list_note(list, "Saved values and cables are retained for repair. Parameters, bypass and new connections are disabled. You can move or delete the placeholder.");
        list_note(list, m->uri, faint);
        return;
    }
    // Every control on one page, as bars in two columns; a long module scrolls.
    // A tuner shows its meter above them; Euclidean shows its four concentric rings beside its four track columns.
    const auto layout = module_layout(*m);
    if (layout == ModuleLayout::Euclidean) {
        euclidean_view(parent, *m);
        return;
    }
    if (layout == ModuleLayout::Looper) { looper_view(parent, *m); return; }
    const auto ink = module_ink(*m);
    const bool tuner = layout == ModuleLayout::Tuner;
    const int top = tuner ? 220 : 0;
    lv_obj_t* list = nullptr;
    if (layout == ModuleLayout::Pedal) list = pedal_view(parent, *m);
    else {
        if (tuner) tuner_meter(parent);
        list = bar_list(parent, *m, top);
        if (tuner) lv_obj_set_style_pad_top(list, 4, 0);
        if (const auto* d = definition(m->uri); d && !d->file.property.empty()) file_bar(list, *m);
        for (const auto& section : parameter_sections(*m)) {
            if (!section.name.empty()) {
                auto* heading = label(list, upper(section.name), 0, 0, 0, ink, &peepo_mono_14);
                lv_obj_set_width(heading, LV_PCT(100)); lv_obj_set_style_text_letter_space(heading, 2, 0);
            }
            for (const auto* p : section.parameters) parameter_bar(list, *m, *p, compact_width, compact_height);
        }
        if (bars_.empty()) { label(list, "This module has no controls.", 0, 0, 600, muted); return; }
    }
    if (bars_.empty()) return;
    if (!focus_symbol_.empty()) {
        for (std::size_t i = 0; i < bars_.size(); ++i) if (bars_[i].symbol == focus_symbol_) parameter_focus_ = i;
        focus_symbol_.clear();
    }
    if (parameter_focus_ >= bars_.size()) { parameter_focus_ = bars_.size() - 1; adjusting_ = false; }
    highlight(bars_[parameter_focus_].widget, true, adjusting_, bar_ink(*m, bars_[parameter_focus_].symbol));
    if (!list) return;
    lv_obj_update_layout(list);
    lv_obj_scroll_to_y(list, parameter_scroll_, LV_ANIM_OFF);
    // A page with tabs has them at the top of the rail, so its line starts below them.
    const int rail_top = layout == ModuleLayout::Pedal ? 112 : top;
    scroll_rail(parent, list, rail_top, 360 - rail_top, ink);
}
lv_obj_t* Application::bar_list(lv_obj_t* parent, const Module& m, int top) {
    auto* list = scroll_list(parent, top);
    // A bypassed module's controls still work, but read as not playing.
    if (m.bypassed) lv_obj_set_style_opa(list, LV_OPA_50, 0);
    parameter_list_ = list;
    return list;
}
lv_obj_t* Application::scroll_list(lv_obj_t* parent, int top) {
    // Two columns of bars, with room beside and between them to scroll by.
    auto* list = lv_obj_create(parent); lv_obj_remove_style_all(list);
    lv_obj_set_pos(list, rail_width, top); lv_obj_set_size(list, 800 - rail_width, 360 - top);
    lv_obj_set_style_pad_left(list, 8, 0); lv_obj_set_style_pad_right(list, 800 - rail_width - 8 - 2 * compact_width - 40, 0);
    lv_obj_set_style_pad_top(list, 16, 0); lv_obj_set_style_pad_bottom(list, 16, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(list, 40, 0); lv_obj_set_style_pad_row(list, 10, 0);
    // Clickable, so a swipe in the gaps between bars scrolls it; the rail
    // replaces its scrollbar.
    lv_obj_set_scroll_dir(list, LV_DIR_VER); lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    return list;
}
void Application::scroll_rail(lv_obj_t* parent, lv_obj_t* list, int top, int height, std::uint32_t ink) {
    auto* rail = lv_obj_create(parent); lv_obj_remove_style_all(rail);
    lv_obj_set_pos(rail, 0, top); lv_obj_set_size(rail, rail_width, height);
    lv_obj_remove_flag(rail, LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(rail, LV_OBJ_FLAG_PRESS_LOCK);
    auto* line = panel(rail, rail_width / 2 - 1, rail_inset, 2, height - 2 * rail_inset, rim);
    lv_obj_set_style_radius(line, 1, 0); lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    auto* ball = panel(rail, (rail_width - rail_ball) / 2, rail_inset, rail_ball, rail_ball, ink);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(ball, 0, 0);
    lv_obj_remove_flag(ball, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(rail, rail_dragged, LV_EVENT_ALL, list);
    lv_obj_add_event_cb(list, rail_followed, LV_EVENT_SCROLL, rail);
    // Only a list longer than the screen has a rail.
    lv_obj_update_layout(list);
    if (lv_obj_get_scroll_y(list) + lv_obj_get_scroll_bottom(list) <= 0) lv_obj_add_flag(rail, LV_OBJ_FLAG_HIDDEN);
    else place_rail_ball(rail, list);
}
lv_obj_t* Application::pedal_view(lv_obj_t* parent, const Module& m) {
    const auto* layout = pedal_layout(m.uri);
    const auto ink = module_ink(m);
    const auto find = [&](const std::string& symbol) -> const Parameter* {
        const auto it = std::find_if(m.parameters.begin(), m.parameters.end(), [&](const Parameter& p) { return p.symbol == symbol; });
        return it == m.parameters.end() ? nullptr : &*it;
    };
    // The two tabs, at the top of the rail.
    for (int page = 0; page < 2; ++page) {
        auto* icon = button(parent, "", 6, 10 + page * 52, 44, Action::ModulePage, m.id, {}, page);
        lv_obj_set_height(icon, 44); lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
        const bool shown = pedal_page_ == page;
        if (shown) { lv_obj_set_style_bg_color(icon, color(module_fill(m)), 0); lv_obj_set_style_border_color(icon, color(ink), 0); }
        const auto ink_of = shown ? text : muted;
        const auto shape = [&](int x, int y, int w, int h, bool solid, int radius = 2) {
            auto* o = panel(icon, x, y, w, h, ink_of);
            lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_radius(o, radius, 0);
            if (solid) lv_obj_set_style_border_width(o, 0, 0);
            else { lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(o, 2, 0); lv_obj_set_style_border_color(o, color(ink_of), 0); }
            return o;
        };
        if (page == 0) {
            // Three sliders, each with its handle somewhere along it.
            for (int i = 0; i < 3; ++i) {
                shape(10, 14 + i * 7, 24, 2, true);
                shape(i == 1 ? 24 : i == 2 ? 16 : 12, 12 + i * 7, 4, 6, true);
            }
        } else {
            // Two on/off switches, one on and one off.
            shape(11, 11, 22, 10, false, 5); shape(24, 14, 6, 4, true, 2);
            shape(11, 23, 22, 10, false, 5); shape(14, 26, 6, 4, true, 2);
        }
    }
    const bool wave = layout->wave && pedal_page_ == 0;
    if (wave) flow_view(parent, m);
    auto* list = bar_list(parent, m, wave ? flow_height : 0);
    const auto heading = [&](const std::string& name) {
        auto* h = label(list, upper(name), 0, 0, 0, ink, &peepo_mono_14);
        lv_obj_set_width(h, LV_PCT(100)); lv_obj_set_style_text_letter_space(h, 2, 0);
    };
    std::vector<std::string> placed;
    for (const auto& b : layout->main) placed.push_back(b.symbol);
    for (const auto* s : layout->stomps) placed.push_back(s);
    if (pedal_page_ == 0) {
        for (const auto& b : layout->main)
            if (const auto* p = find(b.symbol)) {
                auto shown = *p;
                if (b.name) shown.name = b.name;
                parameter_bar(list, m, shown, compact_width, compact_height, b.segmented);
            }
        std::vector<const Parameter*> stomps;
        for (const auto* s : layout->stomps) if (const auto* p = find(s)) stomps.push_back(p);
        if (!stomps.empty()) heading("Stomps");
        for (const auto* p : stomps) parameter_bar(list, m, *p, compact_width, compact_height);
        return list;
    }
    // Everything else, under the overlay's groups; a mode of up to three
    // choices shows them all.
    for (const auto& s : parameter_sections(m)) {
        std::vector<const Parameter*> rest;
        for (const auto* p : s.parameters) if (std::find(placed.begin(), placed.end(), p->symbol) == placed.end()) rest.push_back(p);
        if (rest.empty()) continue;
        heading(s.name.empty() ? "More" : s.name);
        for (const auto* p : rest)
            parameter_bar(list, m, *p, compact_width, compact_height, p->kind == ParameterKind::Enumeration && p->choices.size() <= 3);
    }
    return list;
}
// The wave: as wide as the two columns of bars, above them.
void Application::flow_view(lv_obj_t* parent, const Module& m) {
    auto* box = panel(parent, flow_left, 10, flow_width, flow_height - 10);
    lv_obj_set_style_radius(box, 8, 0);
    if (m.bypassed) lv_obj_set_style_opa(box, LV_OPA_50, 0);
    // A looping wave swings about the middle, drawn as a dotted line.
    flow_middle_ = lv_line_create(box);
    flow_middle_points_ = {{{flow_inset, flow_box_height / 2}, {flow_width - flow_inset, flow_box_height / 2}}};
    lv_line_set_points(flow_middle_, flow_middle_points_.data(), 2);
    lv_obj_set_style_line_color(flow_middle_, color(rim), 0);
    lv_obj_set_style_line_width(flow_middle_, 1, 0);
    lv_obj_set_style_line_dash_width(flow_middle_, 2, 0); lv_obj_set_style_line_dash_gap(flow_middle_, 6, 0);
    flow_line_ = lv_line_create(box);
    lv_obj_set_style_line_color(flow_line_, color(module_ink(m)), 0);
    lv_obj_set_style_line_width(flow_line_, 3, 0);
    lv_obj_set_style_line_rounded(flow_line_, true, 0);
    for (auto* o : {flow_middle_, flow_line_}) lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    flow_drawn_.reset();
    update_flow();
}
void Application::update_flow() {
    if (!flow_line_) return;
    const auto* m = find_module(graph_, selected_);
    if (!m) return;
    // A bar being dragged is drawn as it moves, before the engine answers.
    const auto value = [&](const char* symbol) {
        if (stream_ && stream_->id == m->id && stream_->symbol == symbol) return stream_->latest;
        for (const auto& p : m->parameters) if (p.symbol == symbol) return p.value;
        return 0.0;
    };
    const std::array<double, 5> drawn{value("shape_param"), value("slope_param"), value("smoothness_param"),
                                      std::round(value("ramp_param")), std::round(value("range_param"))};
    if (flow_drawn_ == drawn) return;
    flow_drawn_ = drawn;
    const FlowSettings settings{drawn[0], drawn[1], drawn[2], static_cast<FlowRamp>(std::clamp(static_cast<int>(drawn[3]), 0, 2)), static_cast<int>(drawn[4])};
    const auto wave = flow_wave(settings, static_cast<int>(flow_steps));
    const double across = flow_width - 2.0 * flow_inset, high = flow_box_height - 2.0 * flow_inset;
    for (std::size_t i = 0; i < flow_steps; ++i)
        flow_points_[i] = {static_cast<lv_value_precise_t>(std::round(flow_inset + across * static_cast<double>(i) / (flow_steps - 1))),
                           static_cast<lv_value_precise_t>(std::round(flow_inset + (1 - wave[i]) * high))};
    lv_line_set_points(flow_line_, flow_points_.data(), static_cast<std::uint32_t>(flow_steps));
    if (settings.ramp == FlowRamp::Looping) lv_obj_remove_flag(flow_middle_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(flow_middle_, LV_OBJ_FLAG_HIDDEN);
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
void Application::release_looper() {
    for (const auto& [widget, held] : looper_pressed_) engine_.looper_command(held.id, held.command, held.target, false);
    looper_pressed_.clear();
}
void Application::looper_view(lv_obj_t* parent, const Module& m) {
    const int count = std::clamp(int(parameter(m, "loops").value), 1, 8);
    const auto live_selection = engine_.looper_status(m.id)[48];
    looper_selected_ = std::clamp(int(live_selection.value_or(parameter(m, "selected").value)), 1, count);
    looper_target_ = std::clamp(looper_target_, 0, count + 1);
    // The stomps bound to a command for a target, as on the patch: a white dot
    // holding each stomp's letter.
    // Drawn last, over the buttons, at the top right corner of each.
    struct Badge { int x, y, id, target; };
    std::vector<Badge> badges;
    const auto badge = [&](const Badge& at) {
        std::string digits;
        for (const auto& a : graph_.assignments)
            if (a.module == m.id && is_stomp(a.control) && looper_assignment_command(a) == at.id && looper_assignment_target(a) == at.target && assignment_target(graph_, a))
                digits += (digits.empty() ? "" : " ") + stomp_letter(a.control);
        if (digits.empty()) return;
        const int width = 20 + 14 * static_cast<int>(std::count(digits.begin(), digits.end(), ' '));
        auto* dot = panel(parent, at.x - width + 6, at.y - 8, width, 20, text);
        lv_obj_set_style_radius(dot, 10, 0); lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        auto* number = label(dot, digits, 0, 1, width, background, &peepo_mono_14);
        lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
    };
    const auto command = [&](lv_obj_t* where, const char* name, int x, int y, int width, int id, bool quick = false) {
        auto* b = button(where, name, x, y, width, Action::LooperCommand, m.id, quick ? "quick" : "", id);
        lv_obj_set_size(b, width, 40);
        if (!looper_command_supported(id) || engine_.state() != EngineState::Ready) lv_obj_add_state(b, LV_STATE_DISABLED);
        // While binding, the command picked for the next stomp is lit.
        if (looper_binding_ && looper_bind_command_ == id && !quick) lv_obj_set_style_border_color(b, color(accent), 0);
        if (where == parent) badges.push_back({x + width, y, id, quick ? 0 : looper_target_});
        looper_commands_.push_back(b);
        return b;
    };
    // The settings of one section, as bars and switches.
    const auto bars = [&](const std::vector<std::pair<std::string, const char*>>& symbols) {
        auto* list = lv_obj_create(parent); lv_obj_remove_style_all(list);
        lv_obj_set_pos(list, 0, 4); lv_obj_set_size(list, 800, 166);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_hor(list, 20, 0); lv_obj_set_style_pad_row(list, 8, 0); lv_obj_set_style_pad_column(list, 16, 0);
        lv_obj_set_scroll_dir(list, LV_DIR_VER); parameter_list_ = list;
        for (const auto& [symbol, name] : symbols) {
            const auto found = std::find_if(m.parameters.begin(), m.parameters.end(), [&](const Parameter& p) { return p.symbol == symbol; });
            if (found == m.parameters.end()) continue;
            auto shown = *found;
            if (name) shown.name = name;
            parameter_bar(list, m, shown);
            if (auto* group = lv_group_get_default()) lv_group_add_obj(group, bars_.back().widget);
        }
        return list;
    };
    const auto prefix = "l" + std::to_string(looper_selected_) + "_";
    if (looper_section_ == 0) {
        const auto aim = looper_target_ == 0 ? std::string("SELECTED") : looper_target_ == 1 ? std::string("ALL") : "LOOP " + std::to_string(looper_target_ - 1);
        auto* target = button(parent, "ACT ON: " + aim, 14, 4, 250, Action::LooperTarget, m.id); lv_obj_set_height(target, 40);
        command(parent, "PREV", 282, 4, 100, 65);
        command(parent, "NEXT", 390, 4, 100, 64);
        auto* bind = button(parent, looper_binding_ ? "BINDING" : "BIND", 508, 4, 130, Action::LooperBind, m.id); lv_obj_set_height(bind, 40);
        if (looper_binding_) primary(bind);
        auto* remove = button(parent, "- LOOP", 658, 4, 128, Action::LooperCount, m.id, {}, -1); lv_obj_set_height(remove, 40);
        if (count == 1) lv_obj_add_state(remove, LV_STATE_DISABLED);
        constexpr const char* names[]{"RECORD", "OVERDUB", "MULTIPLY", "INSERT", "REPLACE", "SUBST", "REVERSE", "MUTE",
            "UNDO", "REDO", "TRIGGER", "ONCE", "PAUSE", "SOLO", "DELAY", "CLEAR"};
        constexpr int ids[]{5,6,7,8,2,16,3,9,0,1,15,14,21,24,10,17};
        for (int i = 0; i < 16; ++i) command(parent, names[i], 14 + (i % 8) * 97, 60 + (i / 8) * 50, 92, ids[i]);
    } else if (looper_section_ == 1) {
        bars({{prefix+"level", "Loop level"}, {prefix+"in_gain", "Input gain"}, {prefix+"thresh", "Rec threshold"}, {prefix+"feedback", "Feedback"},
              {prefix+"play_fb", nullptr}, {prefix+"sync", nullptr}, {prefix+"play_sync", nullptr}});
    } else if (looper_section_ == 2) {
        // Plain speed: pitch follows it, as on tape.
        command(bars({{prefix+"rate", "Rate"}}), "REVERSE", 0, 0, 180, 3, true);
    } else {
        auto* list = bars({{"dry_level", nullptr}, {"quantize", nullptr}, {"round", nullptr}, {"mute_quant", nullptr}, {"dub_quant", nullptr},
                           {"repl_quant", nullptr}, {"sync_to", nullptr}, {"rel_sync", nullptr}, {"eighths", nullptr}, {"fade", nullptr},
                           {"safety_fb", nullptr}, {"loop_secs", nullptr}});
        command(list, "PREV LOOP", 0, 0, 180, 65);
        command(list, "NEXT LOOP", 0, 0, 180, 64);
        auto* remove = button(list, "REMOVE LOOP", 0, 0, 180, Action::LooperCount, m.id, {}, -1); lv_obj_set_height(remove, 40);
        if (count == 1) lv_obj_add_state(remove, LV_STATE_DISABLED);
    }
    auto* row = lv_obj_create(parent); lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, 14, 182); lv_obj_set_size(row, 772, 116);
    lv_obj_set_scroll_dir(row, LV_DIR_HOR); lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_ACTIVE);
    for (int n = 0; n < count; ++n) {
        auto& c = looper_cards_[std::size_t(n)];
        c.card = button(row, "", n * 194, 0, 180, Action::LooperSelect, m.id, {}, n + 1);
        lv_obj_set_height(c.card, 110); lv_obj_delete(lv_obj_get_child(c.card, 0));
        label(c.card, "LOOP " + std::to_string(n + 1), 12, 8, 156, text, &peepo_bold_14);
        c.time = label(c.card, "--", 12, 30, 156, muted, &peepo_mono_14);
        c.cycle = label(c.card, "", 12, 49, 156, faint, &peepo_mono_14);
        c.state = label(c.card, "WAITING", 12, 70, 156, muted, &peepo_bold_14);
        c.bar = panel(c.card, 12, 96, 0, 4, muted); lv_obj_set_style_border_width(c.bar, 0, 0);
        for (auto* child : {c.time, c.cycle, c.state, c.bar}) lv_obj_remove_flag(child, LV_OBJ_FLAG_CLICKABLE);
    }
    constexpr const char* quick[]{"REC/DUB", "OVERDUB", "PLAY", "PAUSE", "UNDO", "CLEAR"};
    constexpr int ids[]{33,6,15,21,0,17};
    for (int n = 0; n < 6; ++n) {
        auto* b = command(parent, quick[n], 14 + n * 108, 312, 100, ids[n], true);
        if (n == 0) primary(b);
    }
    auto* add = button(parent, "+ LOOP", 670, 312, 116, Action::LooperCount, m.id, {}, 1); lv_obj_set_height(add, 40);
    if (count == 8) lv_obj_add_state(add, LV_STATE_DISABLED);
    for (const auto& at : badges) badge(at);
    update_looper();
}
void Application::update_looper() {
    if (!looper_cards_[0].card) return;
    const auto* m = find_module(graph_, selected_); if (!m) return;
    const auto live = engine_.looper_status(m->id);
    const int count = std::clamp(int(parameter(*m, "loops").value), 1, 8);
    const int selected = live[48] ? std::clamp(int(*live[48]), 1, count) : looper_selected_;
    if (selected != looper_selected_) {
        looper_selected_ = selected;
        // Reflect navigation events into the saved setting. Plugins cannot
        // write their input controls; without this, tapping the old input
        // value after Next would produce no control edge in Ingen.
        if (parameter(*m, "selected").value != selected) submit(SetParameter{m->id, "selected", double(selected)});
        if (looper_section_ == 1 || looper_section_ == 2) rebuild_ = true;
    }
    const auto time = [](double v) { std::ostringstream s; s << std::fixed << std::setprecision(2) << v; return s.str(); };
    for (int n = 0; n < count; ++n) {
        const auto& c = looper_cards_[std::size_t(n)]; if (!c.card) continue;
        const auto base = std::size_t(n) * 6;
        const int state = live[base] ? int(*live[base]) : -1;
        const bool waiting = live[base + 2].value_or(0) > .5;
        const auto ink = waiting ? warning : state == 2 ? 0xf87171 : state == 5 ? warning : state == 4 ? module_ink(*m) : muted;
        const auto name = live[base] ? looper_state(state) : engine_.state() != EngineState::Ready ? "OFFLINE" : "WAITING";
        if (name != std::string(lv_label_get_text(c.state))) lv_label_set_text(c.state, name);
        lv_obj_set_style_text_color(c.state, color(ink), 0);
        lv_obj_set_style_opa(c.state, waiting && (clock_ms_ / 300) % 2 ? LV_OPA_40 : LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(c.card, color(n + 1 == selected ? text : border), 0);
        lv_obj_set_style_border_width(c.card, n + 1 == selected ? 2 : 1, 0);
        const double length = live[base + 3].value_or(0), pos = live[base + 5].value_or(0), cycle = live[base + 4].value_or(0);
        const auto position = length > 0 ? time(pos) + " / " + time(length) + " s" : "--";
        lv_label_set_text(c.time, position.c_str());
        const auto cy = cycle > 0 && std::abs(cycle - length) > .001 ? "CYCLE " + time(cycle) + " s" : std::string();
        lv_label_set_text(c.cycle, cy.c_str());
        lv_obj_set_style_bg_color(c.bar, color(ink), 0);
        lv_obj_set_width(c.bar, length > 0 ? int(std::clamp(pos / length, 0.0, 1.0) * 156) : 0);
    }
    if (footer_ && (message_.empty() || clock_ms_ - message_ms_ >= 4000)) {
        const auto state = live[std::size_t(selected - 1) * 6];
        // The sync source: none, the tempo or a loop (Sync to).
        const auto sync_to = std::find_if(m->parameters.begin(), m->parameters.end(), [](const Parameter& p) { return p.symbol == "sync_to"; });
        const int to = sync_to == m->parameters.end() ? 0 : int(std::lround(sync_to->value));
        const auto sync = to <= 0 ? std::string("FREE") : to == 1 ? std::string("SYNC TEMPO") : "SYNC LOOP " + std::to_string(to - 1);
        const auto status = "LOOP " + std::to_string(selected) + " // " + (state ? looper_state(int(*state)) : "WAITING") + " // " + sync;
        lv_label_set_text(footer_, status.c_str());
    }
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
void Application::parameter_bar(lv_obj_t* parent, const Module& m, const Parameter& p, int width, int height, bool segmented) {
    segmented = segmented && p.kind == ParameterKind::Enumeration && p.choices.size() >= 2;
    auto* bar = panel(parent, 0, 0, width, height);
    lv_obj_set_style_radius(bar, 10, 0); lv_obj_set_style_clip_corner(bar, true, 0);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_PRESS_LOCK);
    auto* fill = panel(bar, 0, 0, 0, height, module_fill(m));
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
    // And the MIDI message that sets it, learned by holding the bar.
    if (p.midi) caption += (caption.empty() ? "" : "  ") + midi_binding_text(*p.midi);
    // Each scene keeps its own value, marked by holding the bar.
    if (scene_parameter_marked(graph_, m.id, p.symbol)) caption += (caption.empty() ? "" : "  ") + std::string("Per scene");
    // The name takes the left half and more, the value the right.
    const int name_width = width * 54 / 100, value_x = width * 2 / 5, inset = width < bar_width ? 12 : 16;
    auto* name = label(bar, upper(p.name), inset, caption.empty() ? (height - 18) / 2 : height / 2 - 18, name_width, text, &peepo_bold_14);
    lv_obj_set_style_text_letter_space(name, 1, 0);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT); lv_obj_set_height(name, 18);
    auto* value = label(bar, "", value_x, (height - 22) / 2, width - value_x - inset, text, &peepo_bold_18);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT); lv_obj_set_height(value, 22);
    if (!caption.empty() && !segmented) label(bar, caption, inset, height / 2 + 1, name_width, muted, &peepo_mono_14);
    if (segmented) {
        // Like the toggle on a pedal, the choices say what it is: no name, no value.
        lv_obj_add_flag(bar, LV_OBJ_FLAG_USER_1);
        lv_obj_add_flag(name, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(value, LV_OBJ_FLAG_HIDDEN);
        const int count = static_cast<int>(p.choices.size());
        for (int i = 0; i < count; ++i) {
            const int x = i * width / count, w = (i + 1) * width / count - x;
            auto* choice = label(bar, upper(p.choices[static_cast<std::size_t>(i)]), x + 2, (height - 18) / 2, w - 4, faint, &peepo_bold_14);
            lv_obj_set_style_text_align(choice, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(choice, LV_LABEL_LONG_DOT); lv_obj_set_height(choice, 18);
        }
        // A thin line between neighbouring choices.
        for (int i = 1; i < count; ++i) {
            auto* line = panel(bar, i * width / count, 8, 1, height - 16, border);
            lv_obj_set_style_border_width(line, 0, 0); lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    paint_bar(bar, p);
    bind(bar, Action::Parameter, m.id, p.symbol);
    bars_.push_back({m.id, p.symbol, bar});
}
void Application::file_bar(lv_obj_t* parent, const Module& m) {
    const auto& file = definition(m.uri)->file;
    auto* bar = panel(parent, 0, 0, compact_width, compact_height);
    lv_obj_set_style_radius(bar, 10, 0);
    // Children as on a parameter bar: an empty fill, the name, the value.
    lv_obj_remove_flag(panel(bar, 0, 0, 0, compact_height), LV_OBJ_FLAG_CLICKABLE);
    auto* name = label(bar, upper(file.label), 12, (compact_height - 18) / 2, 100, text, &peepo_bold_14);
    lv_obj_set_style_text_letter_space(name, 1, 0);
    // The file's name only: where it lives says nothing on a small screen.
    auto* chosen = label(bar, m.file.empty() ? "None chosen" : file_name(m.file), 112, (compact_height - 22) / 2, compact_width - 124, m.file.empty() ? faint : text, &peepo_bold_18);
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
        if (files_ && engine_.state() == EngineState::Ready) { selected_ = m->id; navigate(Page::FilePicker); }
        else message("Files are not available here.");
        return;
    }
    const auto& p = parameter(*m, bar.symbol);
    if (p.kind == ParameterKind::Toggle || p.kind == ParameterKind::Enumeration || is_euclidean_toggle(p)) { submit(SetParameter{m->id, p.symbol, stepped_choice(p), true}); return; }
    // With one encoder, its push switches between choosing a bar and turning it.
    adjusting_ = !adjusting_;
    highlight(bar.widget, true, adjusting_, bar_ink(*m, bar.symbol));
    message(adjusting_ ? "Encoder 1 turns " + p.name + "; push it again to move on." : "Encoder 1 picks a control again.");
}
void Application::open_module(InstanceId id) {
    // The same module again keeps its place; another starts at the top.
    if (id != selected_) { parameter_focus_ = 0; parameter_scroll_ = 0; pedal_page_ = 0; }
    selected_ = id; patch_focus_ = id; adjusting_ = false;
    message("Drag a bar to set it. E1 picks a bar, E2 turns it.");
    navigate(Page::Parameters);
}
Page Application::back_page() const {
    // Back returns to where the page was opened from.
    switch (page_) {
    case Page::NamePatch: return naming_scene_ ? Page::Scenes : Page::Patches;
    case Page::Controls: case Page::Diagnostics: case Page::Power: case Page::Midi: return Page::Settings;
    case Page::MidiLearn: return Page::Parameters;
    case Page::PatchMidi: case Page::PatchActions: return Page::Patches;
    case Page::ConfirmReplace: return confirm_origin_;
    case Page::ConfirmAction: return confirmation_.back;
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
    // The page's own encoder edits the bar on screen, so the current scene;
    // a pinned one is played, like a knob on a pedal.
    if ((p->kind == ParameterKind::Continuous || p->kind == ParameterKind::Integer) && !is_euclidean_toggle(*p)) stream_parameter(id, symbol, next, !pin);
    else submit(SetParameter{id, symbol, next, !pin});
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
    reveal(m->x, m->y, module_height_of(*m));
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
    auto* list = page_list(parent);
    // With the tempo taken from incoming clock, the clock sets it; a step or a tap lasts until the next clock.
    const bool clocked = midi_ && midi_settings_.tempo_source == midi::TempoSource::Clock;
    value_bar(list, "Tempo", minimum_tempo, maximum_tempo, 1, std::round(shown), [](double bpm) { return metric_text(bpm, false) + " BPM"; },
              [this](double bpm) { set_tempo(bpm); });
    if (clocked) list_bar(list, "MIDI clock", {.value = midi_status_.clock_in ? "FOLLOWING" : "WAITING", .value_ink = midi_status_.clock_in ? accent : muted, .still = true});
    else if (midi_) {
        // The MIDI clock out: Stop, then Start, to line another device's pattern up by hand.
        auto* restart = list_bar(list, "Restart clock", {.caption = "Stop, then Start"}, Action::ClockRestart);
        if (!midi_->connected() || !midi_settings_.clock_out) lv_obj_add_state(restart, LV_STATE_DISABLED);
    }
    list_bar(list, "Slower", {.value = "-1"}, Action::TempoStep, 0, {}, -1);
    list_bar(list, "Faster", {.value = "+1"}, Action::TempoStep, 0, {}, 1);
    std::string following;
    unsigned count = 0;
    for (const auto& m : graph_.modules)
        if (const auto* d = definition(m.uri); d && !d->tempo_symbol.empty() && module_available(m)) { following += (count++ ? ", " : "") + m.name; }
    list_heading(list, count ? "Follow the tempo" : "Nothing follows it yet");
    list_note(list, count ? following : "Modules with a tempo, such as Chorus, Flanger or the sequencers, take it when they are added.");
    list_note(list, "Saved with the patch. Encoder 1 turns it; Tap, or a footswitch, taps it.", faint);
}
void Application::change_midi(const std::function<void(midi::Settings&)>& change) {
    if (!midi_) return;
    auto next = midi_->settings();
    change(next);
    if (const auto issue = midi_->save(next); !issue.empty()) { message(issue); return; }
    midi_settings_ = next;
    message("MIDI settings saved for every patch.");
    rebuild_ = true;
}
void Application::open_learn(InstanceId id, const std::string& symbol) {
    if (engine_.state() != EngineState::Ready) { message("The engine is unavailable."); return; }
    learn_id_ = id; learn_symbol_ = symbol;
    learn_waiting_ = false; learn_request_ = 0;
    navigate(Page::MidiLearn);
}
void Application::midi_page(lv_obj_t* parent) {
    const auto& s = midi_settings_;
    const bool following = s.tempo_source == midi::TempoSource::Clock;
    auto* list = page_list(parent);
    list_heading(list, "Clock");
    list_bar(list, "Clock out", {.value = s.clock_out ? "ON" : "OFF", .caption = following ? "Off while following" : "24 per beat", .lit = s.clock_out && !following}, Action::MidiToggle, 0, {}, 0);
    list_bar(list, "Start / stop", {.value = s.start_stop ? "ON" : "OFF", .caption = "With the clock", .lit = s.start_stop}, Action::MidiToggle, 0, {}, 1);
    list_heading(list, "Tempo from");
    choice_bar(list, {"Patch", "MIDI clock"}, following ? 1 : 0, Action::MidiTempoSource);
    list_bar(list, "Incoming clock", {.value = following ? (midi_status_.clock_in ? metric_text(midi_status_.clock_in, false) + " BPM" : "NONE") : "--",
                                      .value_ink = following && midi_status_.clock_in ? accent : muted, .still = true});
    list_heading(list, "Program change");
    const auto channel = s.program_channel;
    list_bar(list, "Loads patch N", {.value = channel == midi::program_off ? "OFF" : channel == midi::any_channel ? "ANY" : "CH " + std::to_string(channel),
                                     .value_ink = channel ? accent : muted, .caption = "Tap: next channel"}, Action::MidiChannel, 0, {}, 1);
    list_bar(list, "Previous channel", {}, Action::MidiChannel, 0, {}, -1);
    // On the same channel: a control change whose value 0 to 7 plays scene 1 to 8.
    value_bar(list, "Plays scene", 0, 127, 1, s.scene_controller, [](double v) { return "CC " + std::to_string(std::lround(v)); },
              [this](double v) { change_midi([&](midi::Settings& m) { m.scene_controller = static_cast<int>(std::lround(v)); }); });
    list_note(list, channel ? "Value 0 to 7 plays scene 1 to 8 of the patch, on the channel above." : "Scenes follow the program-change channel: off for now.", faint);
    list_heading(list, "Devices");
    if (!midi_->connected()) { list_note(list, "The instrument lists its MIDI devices here."); return; }
    // A device that sends and receives shows once, with both switches.
    struct Device { std::string label, input, output; };
    std::vector<Device> devices;
    auto add = [&](const std::string& name, bool input) {
        const auto shown = midi::device_label(name);
        auto it = std::find_if(devices.begin(), devices.end(), [&](const Device& d) { return d.label == shown; });
        if (it == devices.end()) { devices.push_back({shown, {}, {}}); it = devices.end() - 1; }
        (input ? it->input : it->output) = name;
    };
    for (const auto& name : midi_status_.inputs) add(name, true);
    for (const auto& name : midi_status_.outputs) add(name, false);
    if (devices.empty()) { list_note(list, "None plugged in. A class-compliant USB MIDI device is found by itself."); return; }
    for (const auto& d : devices) {
        // In: it plays into Peepo. Out: it hears Peepo's MIDI and clock. A row each.
        for (int out = 0; out < 2; ++out) {
            const auto& port = out ? d.output : d.input;
            if (port.empty()) {
                auto* gap = lv_obj_create(list); lv_obj_remove_style_all(gap); lv_obj_set_size(gap, compact_width, compact_height);
                continue;
            }
            const bool on = !(out ? s.outputs_off : s.inputs_off).contains(port);
            list_bar(list, d.label, {.value = on ? "ON" : "OFF", .caption = out ? "Out: hears Peepo" : "In: plays Peepo", .lit = on, .keep_case = true}, Action::MidiDevice, 0, port, out);
        }
    }
}
void Application::midi_learn(lv_obj_t* parent) {
    const auto* m = find_module(graph_, learn_id_);
    const auto it = m ? std::find_if(m->parameters.begin(), m->parameters.end(), [&](const Parameter& p) { return p.symbol == learn_symbol_; }) : std::vector<Parameter>::const_iterator{};
    auto* list = page_list(parent);
    if (!m || it == m->parameters.end()) { list_note(list, "Nothing to learn here."); return; }
    list_heading(list, it->name, module_ink(*m));
    if (learn_waiting_ || learn_request_) {
        list_bar(list, "Waiting", {.value = "MOVE ONE", .value_ink = accent, .caption = "A knob, fader, pedal or pad", .still = true});
        list_note(list, "The first message on any channel sets this control from now on. A key sets it only when the control is a switch.");
        return;
    }
    const auto& scenes = graph_.scenes;
    list_bar(list, "Set by", {.value = it->midi ? midi_binding_text(*it->midi) : "NOTHING", .value_ink = it->midi ? accent : muted, .still = true});
    if (scenes.scenes.empty()) {
        list_note(list, "Learn waits for the next control moved on a MIDI device, and replaces what set it before. Saved with the patch.");
        return;
    }
    // Whether each scene keeps its own value: the two choices across both columns.
    list_heading(list, "Scenes");
    const bool marked = scene_parameter_marked(graph_, m->id, it->symbol);
    choice_bar(list, {"Same in every scene", "Per scene"}, marked ? 1 : 0, Action::SceneMark, 0, {}, 2 * compact_width, module_fill(*m));
    std::string names;
    for (std::size_t i = 0; i < scenes.scenes.size(); ++i)
        names += (i == 0 ? "" : i + 1 == scenes.scenes.size() ? " and " : ", ") + scenes.scenes[i].name;
    list_note(list, (scenes.scenes.size() == 1 ? "Per scene: " + names + " keeps its own value." : "Per scene: " + names + " each keep their own value.") +
              "\nSame: one value for every scene. " + std::to_string(scenes.parameters.size()) + " of " + std::to_string(max_scene_parameters) + " marked in this patch.");
}
std::size_t Application::shown_bank() const {
    const auto& s = graph_.scenes;
    const auto banks = (s.scenes.size() + scene_bank - 1) / scene_bank;
    if (scene_bank_ && scene_bank_->scene == s.current && scene_bank_->patch == graph_.patch_id && scene_bank_->bank < banks) return scene_bank_->bank;
    return s.current / scene_bank;
}
void Application::scene_stomp(const Gesture& g) {
    if (g.kind != Gesture::Kind::Press) return;
    const auto& s = graph_.scenes;
    const auto name = control_label(g.control);
    const auto n = static_cast<std::size_t>(std::stoul(g.control.substr(5)));
    // D (A+B) and E (B+C): the previous and next bank, round and round.
    if (n > layout_.stomps) {
        if (n > layout_.stomps + 2) return;
        const auto banks = (s.scenes.size() + scene_bank - 1) / scene_bank;
        if (banks < 2) { message(name + ": one bank only. " + scene_hint()); return; }
        const auto bank = (shown_bank() + (n == layout_.stomps + 1 ? banks - 1 : 1)) % banks;
        scene_bank_ = SceneBank{bank, s.current, graph_.patch_id};
        message("Bank " + std::to_string(bank + 1) + ". " + scene_hint());
        return;
    }
    const auto index = shown_bank() * scene_bank + n - 1;
    if (index >= s.scenes.size()) { message(name + ": bank " + std::to_string(shown_bank() + 1) + " has no scene " + std::to_string(index + 1) + "."); return; }
    // A performance change of scene, as a tap on the Scenes page: the scene's
    // saved state, the patch left unedited.
    const auto scene = s.scenes[index].name;
    if (const auto request = submit(SelectScene{index})) accepted_notes_[request] = [name, scene] { return name + ": playing " + scene + "."; };
}
std::string Application::scene_hint() const {
    const auto& s = graph_.scenes;
    if (s.scenes.empty()) return {};
    // The bank stomps A to C play.
    const auto first = shown_bank() * scene_bank;
    std::string letters, names;
    for (auto i = first; i < std::min(first + scene_bank, s.scenes.size()); ++i) {
        letters += (letters.empty() ? "" : " ") + std::string(1, static_cast<char>('A' + (i - first)));
        names += (names.empty() ? "" : ", ") + s.scenes[i].name;
    }
    return letters + ": " + names + "." + (s.scenes.size() > scene_bank ? " D E: bank." : "");
}
void Application::scenes_page(lv_obj_t* parent) {
    const auto& s = graph_.scenes;
    auto* list = page_list(parent);
    if (s.scenes.empty()) {
        list_note(list, "No scenes yet. Add keeps the patch as it is now as a scene: every module on or bypassed, and the parameters marked per scene.");
        return;
    }
    // By bank, as the stomps reach them in scene mode; the one playing outlined.
    for (std::size_t i = 0; i < s.scenes.size(); ++i) {
        if (i % scene_bank == 0) list_heading(list, "Bank " + std::to_string(i / scene_bank + 1));
        const bool playing = i == s.current;
        BarLook look{.value = playing ? "PLAYING" : "", .value_ink = accent,
                     .caption = "Scene " + std::to_string(i + 1) + "   stomp " + std::string(1, static_cast<char>('A' + i % scene_bank))};
        if (playing) look.edge = accent;
        list_bar(list, s.scenes[i].name, look, Action::SceneSelect, 0, {}, static_cast<int>(i));
    }
    list_heading(list, "Per scene  " + std::to_string(s.parameters.size()) + " of " + std::to_string(max_scene_parameters));
    bool any = false;
    for (const auto& t : s.parameters) {
        const auto* m = scene_target(graph_, t);
        if (!m) continue;
        const auto& p = parameter(*m, t.symbol);
        list_bar(list, p.name, {.value = format_value(p), .caption = m->name, .keep_case = false}, Action::SceneParameter, m->id, t.symbol);
        any = true;
    }
    if (!any) list_note(list, "None yet. Hold a parameter on its module page to give each scene its own value.");
}
void Application::patch_midi(lv_obj_t* parent) {
    const auto& messages = graph_.load_messages;
    auto* list = page_list(parent);
    // A control change: its channel, controller and value, each dragged like a module's bar.
    const auto change = [this](std::size_t i, int field, double v) {
        auto next = graph_.load_messages;
        if (i >= next.size()) return;
        const int value = static_cast<int>(std::round(v));
        (field == 0 ? next[i].channel : field == 1 ? next[i].number : next[i].value) = value;
        if (next != graph_.load_messages) submit(SetLoadMessages{next});
    };
    const auto number = [](double v) { return std::to_string(static_cast<int>(std::round(v))); };
    for (std::size_t i = 0; i < messages.size(); ++i) {
        list_heading(list, "Control change " + std::to_string(i + 1), midi_ink);
        value_bar(list, "Channel", 1, 16, 1, messages[i].channel, number, [change, i](double v) { change(i, 0, v); });
        value_bar(list, "Controller", 0, 127, 1, messages[i].number, number, [change, i](double v) { change(i, 1, v); });
        value_bar(list, "Value", 0, 127, 1, messages[i].value, number, [change, i](double v) { change(i, 2, v); });
        list_bar(list, "Remove", {}, Action::RemoveLoadMessage, 0, {}, static_cast<int>(i));
    }
    if (messages.empty()) list_note(list, "This patch sends no MIDI when it loads.");
    list_note(list, "Sent to every MIDI device as the patch loads, and again when changed here; for a pattern or preset on a pedal or drum machine. Saved with the patch.", faint);
}
void Application::file_picker(lv_obj_t* parent) {
    const auto* m = find_module(graph_, selected_);
    const auto* d = m ? definition(m->uri) : nullptr;
    auto* list = page_list(parent);
    if (!d || d->file.property.empty() || !files_) { list_note(list, "Nothing to choose here."); return; }
    std::string types;
    for (const auto& e : d->file.extensions) types += (types.empty() ? "." : ", .") + e;
    const auto found = files_->list(d->file.folder, d->file.extensions);
    if (found.empty()) {
        list_heading(list, "No " + types + " files yet");
        list_note(list, "Copy them into " + (files_->root() / d->file.folder).generic_string() + " (for example over SSH), then open this page again.");
        return;
    }
    const auto ink = module_ink(*m);
    list_heading(list, std::to_string(found.size()) + (found.size() == 1 ? " file" : " files") + " in " + d->file.folder + "/", ink);
    // File names keep their case; the chosen one is lit in the module's colour.
    for (const auto& file : found)
        list_bar(list, file.name, {.lit = file.engine_path == m->file, .keep_case = true, .fill = module_fill(*m)}, Action::PickFile, m->id, file.engine_path);
}
void Application::confirm_action(lv_obj_t* parent) {
    auto* list = page_list(parent);
    list_heading(list, confirmation_.heading);
    list_note(list, confirmation_.body, text);
}
void Application::stored_patches(lv_obj_t* parent) {
    // Like a module page: every saved patch on one list, two to a row; a long
    // list scrolls. What can be done to the patch playing is in the header.
    auto* list = page_list(parent);
    list_bar(list, "+ New patch", {}, Action::NameEntry, 0, {}, 0);
    if (engine_.capabilities().simulation) list_bar(list, "Parallel demo", {}, Action::ParallelDemo);
    list_heading(list, "Saved");
    if (stored_.empty()) list_note(list, "Nothing saved yet. Save keeps this patch on disk, with its plugin state, through restarts.");
    for (const auto& patch : stored_) {
        const bool playing = patch.id == graph_.patch_id, trouble = !patch.revision || !patch.issue.empty() || !patch.missing_files.empty();
        BarLook look;
        // What the patch holds, and on the right whether it plays or starts.
        look.caption = !patch.revision ? "Unreadable" : !patch.missing_files.empty() ? std::to_string(patch.missing_files.size()) + " missing files"
                                                      : std::to_string(patch.bundle.blocks.size()) + " modules" + (playing && patch.startup ? " / startup" : "");
        look.caption_ink = trouble ? warning : muted;
        look.value = playing ? "PLAYING" : patch.startup ? "STARTUP" : "";
        look.value_ink = playing ? accent : muted;
        if (playing) look.edge = accent;
        list_bar(list, patch.name, look, Action::OpenPatch, 0, patch.id);
    }
}
void Application::patch_actions(lv_obj_t* parent) {
    const auto patch = store_ ? store_->find(patch_selected_) : std::nullopt;
    auto* list = page_list(parent);
    if (!patch) { list_note(list, "That patch is no longer stored."); return; }
    const bool playing = patch->id == graph_.patch_id;
    list_bar(list, "Load at startup", {.value = patch->startup ? "ON" : "OFF", .caption = "When Peepo starts", .lit = patch->startup}, Action::StartupStored, 0, patch->id);
    list_bar(list, "Revision", {.value = patch->revision ? std::to_string(patch->revision) : "--", .caption = patch->revision ? std::to_string(patch->bundle.blocks.size()) + " modules" : "Unreadable",
                                .caption_ink = patch->revision ? 0u : warning, .still = true});
    if (playing) list_note(list, graph_.dirty ? "Playing now, with unsaved edits: Load saved discards them." : "Playing now, as saved.");
    std::string trouble = patch->issue;
    if (!patch->missing_files.empty()) {
        std::string files;
        for (const auto& file : patch->missing_files) files += (files.empty() ? "" : ", ") + file;
        trouble += (trouble.empty() ? "" : " ") + std::string("Missing files: ") + files + ".";
    }
    if (!trouble.empty()) list_note(list, trouble, warning);
}
void Application::patches(lv_obj_t* parent) {
    if (store_) { stored_patches(parent); return; }
    // Without a store: one snapshot in memory, until the app closes.
    auto* list = page_list(parent);
    list_bar(list, "+ New patch", {}, Action::NameEntry, 0, {}, 0);
    list_bar(list, "Parallel demo", {}, Action::ParallelDemo);
    list_heading(list, "Snapshot");
    auto* snapshot = list_bar(list, has_saved_ ? saved_.name : "No snapshot yet",
                              {.value = has_saved_ ? "LOAD" : "", .caption = has_saved_ ? std::to_string(saved_.modules.size()) + " modules" : "Save keeps one"}, Action::LoadSession);
    if (!has_saved_) lv_obj_add_state(snapshot, LV_STATE_DISABLED);
    list_note(list, "Session storage is in memory. Closing the app clears it.");
}
void Application::name_patch(lv_obj_t* parent) {
    auto* input = lv_textarea_create(parent); lv_obj_set_pos(input, 24, 20); lv_obj_set_size(input, 752, 56);
    lv_textarea_set_one_line(input, true);
    // Preserve a backend name longer than the limit for newly typed text.
    const auto longest = naming_scene_ ? max_scene_name : 48;
    lv_textarea_set_text(input, draft_name_.c_str()); lv_textarea_set_max_length(input, static_cast<std::uint32_t>(longest));
    lv_textarea_set_placeholder_text(input, naming_scene_ ? "Scene name" : "Patch name");
    lv_obj_set_style_text_font(input, &peepo_bold_24, 0); lv_obj_set_style_text_color(input, color(text), 0);
    lv_obj_set_style_bg_color(input, color(surface), 0); lv_obj_set_style_bg_opa(input, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(input, color(rim), 0); lv_obj_set_style_border_width(input, 1, 0);
    lv_obj_set_style_radius(input, 8, 0);
    lv_obj_set_style_pad_all(input, 12, 0);
    bind(input, Action::NameInput);
    if (auto* group = lv_group_get_default()) lv_group_add_obj(group, input);
    label(parent, naming_scene_ ? "Up to 12 characters. Back leaves the name as it was." :
          renaming_ && lv_textarea_get_cursor_pos(input) > 48 ?
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
    auto* list = page_list(parent);
    if (!from || !to) { list_note(list, "Module no longer exists"); return; }
    const bool choosing_output = cv_output_.empty();
    const auto ports = connectable_ports(*from, *to, cv_output_, cv_only_);
    list_heading(list, choosing_output ? "Outputs of " + from->name : from->name + " / " + cv_output_ + " > inputs of " + to->name);
    // Each port's edge in its cable's colour: control, MIDI or audio.
    for (const auto* port : ports) {
        const auto ink = port->type == PortType::Cv ? control_ink : port->type == PortType::Midi ? midi_ink : cable_ink;
        list_bar(list, port->symbol, {.caption = port->type == PortType::Cv ? "Control" : port->type == PortType::Midi ? "MIDI" : "Audio", .caption_ink = ink, .edge = ink},
                 Action::CvChoice, 0, port->symbol);
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
    cv_from_ = from_id; cv_to_ = to_id; cv_only_ = false;
    cv_output_ = outputs.size() == 1 ? outputs.front()->symbol : std::string();
    navigate(Page::CvConnect);
}
void Application::confirm_replace(lv_obj_t* parent) {
    auto* list = page_list(parent);
    list_heading(list, graph_.name + " has unsaved edits");
    list_note(list, std::string("Replace this patch and discard them? Cancel to return and save ") + (store_ ? "it first." : "a session snapshot first."), text);
}
void Application::module_actions(lv_obj_t* parent) {
    const auto* m = find_module(graph_, selected_);
    auto* list = page_list(parent);
    if (!m || m->fixed) { list_note(list, "Module actions unavailable"); return; }
    const bool available = module_available(*m);
    list_bar(list, "Parameters", {}, Action::Edit, m->id);
    auto* bypass = list_bar(list, "Software bypass", {.value = m->bypassed ? "ON" : "OFF", .lit = m->bypassed, .fill = module_fill(*m)}, Action::Bypass, m->id);
    auto* duplicate = list_bar(list, "Duplicate", {}, Action::Duplicate, m->id);
    list_bar(list, "Delete module", {}, Action::Remove, m->id);
    if (!available) {
        lv_obj_add_state(bypass, LV_STATE_DISABLED); lv_obj_add_state(duplicate, LV_STATE_DISABLED);
        const auto* d = definition(m->uri);
        const auto why = d && !d->availability_reason.empty() ? d->availability_reason : std::string("its plugin is not installed here");
        list_heading(list, "Not available", warning);
        list_note(list, "Not available: " + why + ". Its cables carry no sound. Delete module removes it and its cables; the saved patch keeps it until you save again.", warning);
        return;
    }
    // While this page is open a stomp press assigns rather than plays (quick_assign).
    std::vector<std::string> switching;
    for (const auto& id : layout_.ids())
        for (const auto* a : bypass_assignments(graph_, id)) if (a->module == m->id) switching.push_back(control_label(id));
    list_heading(list, "Stomps");
    list_note(list, "Press a stomp to switch this module with it.", text);
    list_note(list, (switching.empty() ? std::string("No stomp switches it yet.") : joined_names(switching) + " switches it; press it again to stop.") +
                    " A stomp can switch several modules. Each one flips, so one on and one bypassed swap places.");
    list_note(list, "Duplicate keeps parameter values and software bypass; cables and control assignments stay with the original. "
                    "Settings > Controls assigns parameters too.", faint);
}
void Application::settings(lv_obj_t* parent) {
    auto* list = page_list(parent);
    if (!engine_.capabilities().simulation) {
        list_heading(list, "Engine");
        const bool up = engine_.state() == EngineState::Ready;
        list_bar(list, "Connection", {.value = status_text(), .value_ink = up ? accent : warning, .caption = engine_note_.empty() ? "Retried every second" : engine_note_,
                                      .caption_ink = engine_note_.empty() ? 0u : warning, .still = true});
        list_bar(list, "Connect", {}, Action::State, 0, {}, static_cast<int>(EngineState::Ready));
        list_bar(list, "Disconnect", {.caption = "The engine keeps playing"}, Action::State, 0, {}, static_cast<int>(EngineState::Disconnected));
        if (network_) {
            list_heading(list, "Network");
            network_value_ = list_note(list, "");
            update_network();
        }
        list_heading(list, "Storage");
        list_note(list, store_ ? "Patches are stored in " + store_->root().generic_string() : "Patch storage is not available: session snapshots only.");
        list_note(list, "Temperature, memory, the power supply and CPU load: Diagnostics.", faint);
        return;
    }
    list_heading(list, "Windows simulator / 800 x 480");
    list_bar(list, "Ready", {}, Action::State, 0, {}, static_cast<int>(EngineState::Ready));
    list_bar(list, "Disconnect", {}, Action::State, 0, {}, static_cast<int>(EngineState::Disconnected));
    list_bar(list, "Engine error", {}, Action::State, 0, {}, static_cast<int>(EngineState::Error));
    list_bar(list, "Reject next edit", {}, Action::Reject);
    list_bar(list, "Missing module", {}, Action::MissingFixture);
    list_note(list, "No audio processing. No hardware connection required. Use these states to exercise the interface.");
    list_note(list, "Keys: Q/A W/S turn encoders 1 and 2, keys 1 and 2 push them, Z X C are stomps A B C, V and B press A+B (D) and B+C (E).", faint);
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
    // Six readings: what it is, the reading, and a short line on what it
    // means. Nothing is estimated: a reading this machine does not give is
    // said to be missing.
    struct Reading { const char* name; std::string value, detail; std::uint32_t ink; };
    const auto& r = reading_;
    const bool measured = static_cast<bool>(system_);
    std::vector<Reading> readings;
    const auto cpu = engine_.telemetry().cpu_percent;
    readings.push_back({"Engine", status_text(), metric_text(cpu, true) == "--" ? "No CPU load reported" : "CPU " + metric_text(cpu, true) + (engine_.capabilities().simulation ? ", simulated" : ""),
                        engine_.state() == EngineState::Ready ? accent : warning});
    if (!r.under_voltage) readings.push_back({"Power", "--", "Not measured here", muted});
    else if (*r.under_voltage) readings.push_back({"Power", "TOO LOW", "Audio can crackle", warning});
    else if (low_voltage_seen_) readings.push_back({"Power", "WAS LOW", "Use a 5 V, 5 A supply", warning});
    else readings.push_back({"Power", "OK", "Never too low", accent});
    if (r.temperature_c) readings.push_back({"Temperature", decimal(*r.temperature_c, 1) + " C", *r.temperature_c >= 80 ? "Hot: slows near 85 C" : "Slows near 85 C", *r.temperature_c >= 80 ? warning : text});
    else readings.push_back({"Temperature", "--", "Not measured here", muted});
    if (r.memory_total && r.memory_available) {
        const bool short_of = *r.memory_available * 10 < *r.memory_total;
        readings.push_back({"Memory", gigabytes(*r.memory_available) + " free", "Of " + gigabytes(*r.memory_total), short_of ? warning : text});
    } else readings.push_back({"Memory", "--", "Not measured here", muted});
    std::error_code error;
    const auto space = store_ ? std::filesystem::space(store_->root(), error) : std::filesystem::space_info{};
    if (store_ && !error && space.capacity) {
        const bool short_of = space.available * 20 < space.capacity;
        readings.push_back({"Storage", gigabytes(space.available) + " free", "Of " + gigabytes(space.capacity), short_of ? warning : text});
    } else readings.push_back({"Storage", "--", "Session snapshots only", muted});
    if (r.uptime_s) readings.push_back({"Uptime", duration(*r.uptime_s), "Since boot", text});
    else readings.push_back({"Uptime", "--", "Not measured here", muted});
    auto* list = page_list(parent);
    for (const auto& reading : readings) list_bar(list, reading.name, {.value = reading.value, .value_ink = reading.ink, .caption = reading.detail, .still = true});
    list_note(list, "Memory is shared by the engine, its plugins and this screen; storage is where patches are saved. The CPU slows down near 85 C.");
    list_note(list, measured ? "Read every 2 s. Audio dropouts (xruns) are not counted here yet." : "This computer's readings are not shown; the instrument shows its own.", faint);
}
void Application::power(lv_obj_t* parent) {
    auto* list = page_list(parent);
    if (power_off_) list_bar(list, "Shut down", {.caption = "Then unplug it"}, Action::PowerOff);
    if (restart_) list_bar(list, "Restart", {.caption = "Back with its startup patch"}, Action::Restart);
    list_note(list, "Saved patches are safe. After Shut down, unplug the instrument when the screen goes dark. Restart starts the whole instrument again; it comes back by itself.");
    if (graph_.dirty) list_note(list, "This patch has unsaved edits. Save it in Patches first to keep them.", warning);
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
        if (const auto* m = find_module(graph_, selected_); m && looper_uri(m->uri)) {
            auto* group = lv_group_get_default();
            auto* focused = group ? lv_group_get_focused(group) : nullptr;
            if (turn && first) break; // Ordinary group order includes header, bars, cards and commands.
            if (turn && focused) {
                const auto bar = std::find_if(bars_.begin(), bars_.end(), [&](const Bar& b) { return b.widget == focused; });
                if (bar != bars_.end()) turn_parameter(bar->id, bar->symbol, g, nullptr);
                else if (std::any_of(looper_cards_.begin(), looper_cards_.end(), [&](const LooperCard& c) { return c.card == focused; })) {
                    looper_selected_ = std::clamp(looper_selected_ + g.steps, 1, int(parameter(*m, "loops").value));
                    submit(SetParameter{m->id, "selected", double(looper_selected_)});
                }
            } else if (tap && focused && !lv_obj_has_state(focused, LV_STATE_DISABLED)) {
                lv_obj_send_event(focused, LV_EVENT_PRESSED, nullptr);
                lv_obj_send_event(focused, LV_EVENT_RELEASED, nullptr);
                lv_obj_send_event(focused, LV_EVENT_CLICKED, nullptr);
            }
            return;
        }
        if (bars_.empty()) return;
        // The two tabs read as one list: past the main tab's last bar comes
        // the first of the other, and back.
        if (const auto* m = find_module(graph_, selected_); turn && first && !adjusting_ && m && module_layout(*m) == ModuleLayout::Pedal) {
            const int next = static_cast<int>(parameter_focus_) + g.steps;
            if ((pedal_page_ == 0 && next >= static_cast<int>(bars_.size())) || (pedal_page_ == 1 && next < 0)) {
                pedal_page_ = next < 0 ? 0 : 1; parameter_scroll_ = 0;
                // Built again, the focus is clamped to the panel's last bar.
                parameter_focus_ = next < 0 ? bars_.size() + 100 : 0;
                rebuild_ = true; return;
            }
        }
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
    // Encoder 2 on a value bar turns it, a step a detent (or a hundredth of its range).
    if (auto* focused = lv_group_get_focused(group); turn && !first && focused) {
        for (auto& bound : bindings_)
            if (bound.widget == focused && bound.action == Action::Slide) {
                const double step = bound.step > 0 ? bound.step : (bound.high - bound.low) / 100;
                bound.at = std::clamp(bound.at + g.steps * step, bound.low, bound.high);
                paint_slide(focused, bound.at, bound.low, bound.high, bound.shown);
                if (const auto set = bound.set) set(bound.at);
                return;
            }
    }
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
    // Binding on the Loo-pi page: the stomp is being bound, and does not play.
    if (looper_binding_ && page_ == Page::Parameters) {
        if (const auto* m = find_module(graph_, selected_); m && looper_uri(m->uri)) { if (pressed) looper_bind(g.control); return; }
    }
    // In scene mode the stomps play scenes, never what they are assigned; a
    // Loo-pi command held from before still lets go.
    if (graph_.scenes.scene_mode && !graph_.scenes.scenes.empty()) {
        if (const auto held = looper_stomps_.find(g.control); !pressed && held != looper_stomps_.end()) {
            engine_.looper_command(held->second.id, held->second.command, held->second.target, false);
            looper_stomps_.erase(held);
            return;
        }
        scene_stomp(g);
        return;
    }
    // Footswitch modules hear the stomp as well as whatever it is assigned.
    std::vector<std::string> said;
    const bool listened = drive_footswitches(g.control, pressed, said);
    // A held Loo-pi command is released where it was pressed.
    if (!pressed) {
        if (const auto held = looper_stomps_.find(g.control); held != looper_stomps_.end()) {
            engine_.looper_command(held->second.id, held->second.command, held->second.target, false);
            looper_stomps_.erase(held);
            return;
        }
    }
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
        if (pin->action == ControlAction::Looper) {
            // Press and release, as a held footswitch, so long presses work.
            // With the page open the selected loop is fixed at the press;
            // otherwise Loo-pi's own selection (target 0) is used.
            if (!pressed) return;
            const int command = *looper_assignment_command(*pin);
            int target = looper_assignment_target(*pin);
            if (const auto live = engine_.looper_status(m->id)[48]; target == 0 && live && command != 64 && command != 65) target = int(*live) + 1;
            if (engine_.looper_command(m->id, command, target, true)) {
                looper_stomps_[g.control] = {m->id, command, target};
                said.insert(said.begin(), m->name + " / " + looper_command_name(command) + ", " + looper_target_name(target));
            } else said.insert(said.begin(), m->name + " is not ready for commands");
            message(name + ": " + joined_names(said));
            return;
        }
        // Switch actions go straight to the engine: never coalesced with turns.
        const auto* p = assignment_parameter(graph_, *pin);
        auto current = p->value;
        if (const auto sent = switch_sent_.find(g.control); sent != switch_sent_.end()) {
            if (pending_.contains(sent->second.request) && sent->second.id == m->id && sent->second.symbol == p->symbol) current = sent->second.value;
            else switch_sent_.erase(sent);
        }
        const auto value = switched_value(*p, current, *pin, pressed);
        if (value && *value != current) {
            auto shown = *p; shown.value = *value;
            if (const auto request = submit(SetParameter{m->id, p->symbol, *value})) switch_sent_[g.control] = {m->id, p->symbol, *value, request};
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
void Application::looper_bind(const std::string& control) {
    const auto name = control_label(control);
    const auto* m = find_module(graph_, selected_);
    if (!m) return;
    if (looper_bind_command_ < 0) { message("Tap a command first, then press " + name + "."); return; }
    const auto what = looper_command_name(looper_bind_command_) + " to the " + looper_target_name(looper_bind_target_);
    // Pressing it again on the same command and target unbinds it.
    if (const auto* held = pinned(control); held && held->module == m->id && looper_assignment_command(*held) == looper_bind_command_ &&
        looper_assignment_target(*held) == looper_bind_target_) {
        submit(Unassign{control});
        message(name + " no longer sends " + what + ".");
        return;
    }
    const bool replaces = pinned(control) != nullptr;
    Assignment a;
    a.control = control; a.module = m->id; a.action = ControlAction::Looper; a.mode = SwitchMode::Momentary;
    a.parameter = std::to_string(looper_bind_command_);
    a.minimum = a.maximum = looper_bind_target_;
    submit(Assign{a});
    message(name + " now sends " + what + (replaces ? ", instead of what it did" : "") + ". Press it again to unbind.");
    rebuild_ = true;
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
        if (a->action == ControlAction::Looper)
            return m->name + " / " + looper_command_name(*looper_assignment_command(*a)) + ", " + looper_target_name(looper_assignment_target(*a));
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
    // One bar per control: what it does in this patch. Tap it to change that.
    auto* list = page_list(parent);
    for (const auto& id : layout_.ids()) {
        const auto* pin = pinned(id);
        const bool orphan = orphaned(id);
        BarLook look{.caption = assignment_text(id), .caption_ink = orphan ? warning : pin ? text : muted};
        // Turning or pressing a control shows which bar it is.
        if (id == active_control_) look.edge = accent;
        list_bar(list, control_label(id), look, Action::EditControl, 0, id);
    }
    list_note(list, "Tap a control to choose what it does; a footswitch can also change patch or tap the tempo in every patch. "
                    "D is A and B pressed together, E is B and C.");
    if (graph_.scenes.scene_mode)
        list_note(list, "This patch is in scene mode: A, B and C play scenes, D and E change bank. The pill at the bottom left of the patch changes it.", accent);
}
void Application::assign_picker(lv_obj_t* parent) {
    auto* list = page_list(parent);
    const bool stomp = is_stomp(editing_control_);
    if (!picking_module_) {
        if (stomp && store_) {
            list_heading(list, "Every patch");
            list_bar(list, "Next patch", {}, Action::PickTarget, 0, "@next-patch");
            list_bar(list, "Previous patch", {}, Action::PickTarget, 0, "@previous-patch");
            list_bar(list, "Tap tempo", {}, Action::PickTarget, 0, "@tap-tempo");
        }
        std::vector<const Module*> modules;
        for (const auto& m : graph_.modules)
            if (!m.fixed && module_available(m) && (stomp || !m.parameters.empty())) modules.push_back(&m);
        list_heading(list, "This patch");
        if (modules.empty()) list_note(list, "Nothing in this patch can be controlled yet.");
        for (const auto* m : modules) list_bar(list, m->name, {.lit = false, .edge = module_ink(*m)}, Action::PickModule, m->id);
        return;
    }
    const auto* m = find_module(graph_, picking_module_);
    if (!m) { list_note(list, "Module no longer exists."); return; }
    list_heading(list, m->name, module_ink(*m));
    if (stomp) list_bar(list, "Bypass", {}, Action::PickTarget, 0, "@bypass");
    for (const auto& p : m->parameters) list_bar(list, p.name, {}, Action::PickTarget, 0, p.symbol);
}
void Application::assignment_detail(lv_obj_t* parent) {
    const auto& id = editing_control_;
    auto* list = page_list(parent);
    list_heading(list, assignment_text(id));
    const auto* a = pinned(id);
    if (!a) { list_note(list, "This control acts the same in every patch."); return; }
    const auto* p = assignment_parameter(graph_, *a);
    if (orphaned(id)) {
        list_note(list, "Its module was removed, or now holds a different plugin, so it is never applied. Clear it, then assign the control again.", warning);
        return;
    }
    if (a->action == ControlAction::Looper) {
        list_note(list, "Held: the command is pressed. Released: it is let go, so a long press works as on the page. To change it, bind it again on the Loo-pi page.");
        return;
    }
    if (!p) {
        list_note(list, "Each press switches the software bypass of each module: one that plays is bypassed, one bypassed plays, so two set opposite ways swap places. "
                        "To add or remove a module, hold it on the patch and press this stomp.");
        return;
    }
    list_bar(list, "Invert", {.value = a->inverted ? "ON" : "OFF", .lit = a->inverted}, Action::InvertControl);
    if (is_stomp(id)) choice_bar(list, {"Toggle", "Momentary"}, a->mode == SwitchMode::Momentary ? 1 : 0, Action::SwitchModeChoice);
    if (p->kind != ParameterKind::Toggle && p->kind != ParameterKind::Enumeration) {
        // The two ends of the range, in the parameter's own (linear or logarithmic) scale.
        const Parameter shape = *p;
        const auto end_bar = [&](const char* name, int end) {
            auto at = shape; at.value = end ? a->maximum : a->minimum;
            value_bar(list, name, 0, 1, shape.kind == ParameterKind::Continuous ? 0 : 1.0 / std::max(1.0, shape.maximum - shape.minimum), normalized_value(at),
                [shape](double v) { auto q = shape; q.value = value_from_normalized(shape, v); if (shape.kind != ParameterKind::Continuous) q.value = std::round(q.value); return format_value(q); },
                [this, shape, end](double v) {
                    const auto* now = pinned(editing_control_);
                    if (!now) return;
                    auto changed = *now;
                    double value = value_from_normalized(shape, v);
                    if (shape.kind != ParameterKind::Continuous) value = std::round(value);
                    (end ? changed.maximum : changed.minimum) = value;
                    if (changed.minimum > changed.maximum) { message("From must not pass To; use Invert to reverse the direction."); rebuild_ = true; return; }
                    submit(Assign{changed});
                });
        };
        end_bar("From", 0); end_bar("To", 1);
    }
    list_note(list, is_stomp(id) ? (a->mode == SwitchMode::Momentary ? "Held: To. Released: From." : "Each press: From, then To.")
                                 : "Clockwise moves from From towards To; push and turn for fine steps.");
}
}
