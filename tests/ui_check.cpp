// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "ui/application.hpp"
#include "engine/simulated_engine.hpp"
#include "core/tuning.hpp"
// Test-only geometry access for real pointer presses on the pinned keyboard.
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::uint32_t clock_ms = 0;
lv_point_t pointer{};
bool pressed = false;
std::uint32_t keypad_key = 0;
bool keypad_pressed = false;
void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void read_pointer(lv_indev_t*, lv_indev_data_t* data) {
    data->point = pointer; data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
void read_keypad(lv_indev_t*, lv_indev_data_t* data) {
    data->key = keypad_key; data->state = keypad_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
void step(peepo::Application& app, unsigned duration = 192) {
    for (unsigned i = 0; i < duration; i += 16) {
        clock_ms += 16; lv_timer_handler(); app.tick(16);
    }
}
void click(peepo::Application& app, int x, int y) {
    pointer = {x, y}; pressed = true; step(app, 64); pressed = false; step(app);
}
void keypad_press(peepo::Application& app, std::uint32_t key) {
    keypad_key = key; keypad_pressed = true; step(app, 64); keypad_pressed = false; step(app);
}
void drag(peepo::Application& app, int ax, int ay, int bx, int by) {
    pointer = {ax, ay}; pressed = true; step(app, 64);
    for (int i = 1; i <= 8; ++i) { pointer = {ax + (bx - ax) * i / 8, ay + (by - ay) * i / 8}; step(app, 32); }
    pressed = false; step(app);
}
void hold(peepo::Application& app, int x, int y) {
    pointer = {x, y}; pressed = true; step(app, 640); pressed = false; step(app);
}
lv_obj_t* widget(lv_obj_t* root, const lv_obj_class_t* type) {
    if (lv_obj_check_type(root, type)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = widget(lv_obj_get_child(root, i), type)) return found;
    return nullptr;
}
// The last label whose text is exactly the given text, in tree order.
lv_obj_t* exact_label(lv_obj_t* root, const std::string& text) {
    lv_obj_t* found = nullptr;
    if (lv_obj_check_type(root, &lv_label_class) && text == lv_label_get_text(root)) found = root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* deeper = exact_label(lv_obj_get_child(root, i), text)) found = deeper;
    return found;
}
// The last label whose text contains the given text, in tree order.
lv_obj_t* text_label(lv_obj_t* root, const std::string& text) {
    lv_obj_t* found = nullptr;
    if (lv_obj_check_type(root, &lv_label_class) && std::string(lv_label_get_text(root)).find(text) != std::string::npos) found = root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* deeper = text_label(lv_obj_get_child(root, i), text)) found = deeper;
    return found;
}
// Scroll a browser pill or card into view and tap it. A card's title comes
// after its category tag, so the last match is the title.
void tap(peepo::Application& app, const std::string& text) {
    auto* l = exact_label(app.screen(), text); check(l != nullptr, "browser entry missing");
    lv_obj_scroll_to_view_recursive(l, LV_ANIM_OFF); lv_obj_update_layout(app.screen());
    lv_area_t area; lv_obj_get_coords(l, &area);
    check(area.y1 >= 0 && area.y2 < 432, "browser entry could not be scrolled into view");
    click(app, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2);
}
// A parameter bar on a module page, by the name it shows, scrolled into view.
// The page title comes first in tree order, so the last match is the bar's.
lv_obj_t* bar(peepo::Application& app, const std::string& name) {
    auto* l = exact_label(app.screen(), name); check(l != nullptr, "parameter bar missing");
    auto* b = lv_obj_get_parent(l);
    lv_obj_scroll_to_view_recursive(b, LV_ANIM_OFF); lv_obj_update_layout(app.screen());
    return b;
}
lv_point_t bar_point(peepo::Application& app, const std::string& name) {
    lv_area_t area; lv_obj_get_coords(bar(app, name), &area);
    check(area.y1 >= 72 && area.y2 < 432, "parameter bar could not be scrolled into view");
    return {area.x1 + 60, (area.y1 + area.y2) / 2};
}
// Drag a bar sideways by a share of its width, or tap it.
void slide(peepo::Application& app, const std::string& name, double share) {
    const auto at = bar_point(app, name);
    drag(app, at.x, at.y, at.x + static_cast<int>(share * 372), at.y);
}
void tap_bar(peepo::Application& app, const std::string& name) {
    const auto at = bar_point(app, name);
    click(app, at.x, at.y);
}
// What a bar says: its third child is the value.
std::string bar_value(peepo::Application& app, const std::string& name) {
    return lv_label_get_text(lv_obj_get_child(bar(app, name), 2));
}
// The first label with exactly this text below the header, in tree order: a
// category chip comes before the card tags that repeat its name.
lv_obj_t* first_label(lv_obj_t* root, const std::string& text) {
    if (lv_obj_check_type(root, &lv_label_class) && text == lv_label_get_text(root)) {
        lv_area_t area; lv_obj_get_coords(root, &area);
        if (area.y1 >= 72) return root;
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = first_label(lv_obj_get_child(root, i), text)) return found;
    return nullptr;
}
void tap_first(peepo::Application& app, const std::string& text) {
    auto* l = first_label(app.screen(), text); check(l != nullptr, "label missing");
    lv_area_t area; lv_obj_get_coords(l, &area);
    click(app, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2);
}
// A header pill, by its label: On or Bypassed, Actions.
void tap_header(peepo::Application& app, const std::string& text) {
    std::function<lv_obj_t*(lv_obj_t*)> find = [&](lv_obj_t* root) -> lv_obj_t* {
        if (lv_obj_check_type(root, &lv_label_class) && text == lv_label_get_text(root)) {
            lv_area_t area; lv_obj_get_coords(root, &area);
            if (area.y2 < 72) return root;
        }
        for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i)
            if (auto* found = find(lv_obj_get_child(root, i))) return found;
        return nullptr;
    };
    auto* l = find(app.screen()); check(l != nullptr, "header pill missing");
    lv_area_t area; lv_obj_get_coords(l, &area);
    click(app, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2);
}
bool disabled_button(lv_obj_t* l) { return l && lv_obj_has_state(lv_obj_get_parent(l), LV_STATE_DISABLED); }
// The first tap on a card picks it, the second adds it.
void pick_and_add(peepo::Application& app, const std::string& name) { tap(app, name); tap(app, name); }
void add_module(peepo::Application& app, const std::string& group, const std::string& name) {
    click(app, 490, 36); check(app.page() == peepo::Page::Browser, "browser navigation failed");
    tap(app, group); pick_and_add(app, name);
}
void keyboard_key(peepo::Application& app, const std::string& key) {
    auto* kb = widget(app.screen(), &lv_keyboard_class); check(kb != nullptr, "keyboard missing");
    auto* matrix = reinterpret_cast<lv_buttonmatrix_t*>(kb);
    lv_area_t bounds; lv_obj_get_coords(kb, &bounds);
    for (unsigned i = 0; i < matrix->btn_cnt; ++i) if (key == lv_buttonmatrix_get_button_text(kb, i)) {
        const auto area = matrix->button_areas[i];
        check(area.y2 - area.y1 + 1 >= 44, "keyboard key shorter than touch target");
        check(bounds.y1 + area.y1 >= 72 && bounds.y1 + area.y2 < 432, "keyboard key clipped by viewport");
        click(app, bounds.x1 + (area.x1 + area.x2) / 2, bounds.y1 + (area.y1 + area.y2) / 2); return;
    }
    throw std::runtime_error("keyboard key unavailable: " + key);
}
void word(std::ofstream& f, std::uint32_t n, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) f.put(static_cast<char>((n >> (i * 8)) & 255));
}
void capture(peepo::Application& app, const std::filesystem::path& path) {
    lv_obj_update_layout(app.screen());
    auto* buf = lv_snapshot_take(app.screen(), LV_COLOR_FORMAT_RGB888);
    check(buf != nullptr, "snapshot failed");
    check(buf->header.w == 800 && buf->header.h == 480, "logical display dimensions changed");
    std::ofstream f(path, std::ios::binary); check(f.good(), "screenshot file unavailable");
    const unsigned stride = (800 * 3 + 3) & ~3U;
    f.put('B'); f.put('M'); word(f, 54 + stride * 480, 4); word(f, 0, 4); word(f, 54, 4);
    word(f, 40, 4); word(f, 800, 4); word(f, 480, 4); word(f, 1, 2); word(f, 24, 2);
    word(f, 0, 4); word(f, stride * 480, 4); word(f, 2835, 4); word(f, 2835, 4); word(f, 0, 4); word(f, 0, 4);
    for (int y = 479; y >= 0; --y) f.write(reinterpret_cast<const char*>(buf->data + y * buf->header.stride), stride);
    lv_draw_buf_destroy(buf);
}
}
int main(int argc, char** argv) {
    try {
        const auto output = std::filesystem::path(argc > 1 ? argv[1] : "screenshots");
        std::filesystem::create_directories(output);
        lv_init(); lv_tick_set_cb([] { return clock_ms; });
        auto* display = lv_display_create(800, 480);
        std::vector<std::uint32_t> framebuffer(800 * 480);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_buffers(display, framebuffer.data(), nullptr, static_cast<std::uint32_t>(framebuffer.size() * 4), LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, std::uint8_t*) { lv_display_flush_ready(d); });
        auto* indev = lv_indev_create(); lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, read_pointer); lv_indev_set_display(indev, display);
        auto* group = lv_group_create(); lv_group_set_default(group);
        auto* keypad = lv_indev_create(); lv_indev_set_type(keypad, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(keypad, read_keypad); lv_indev_set_display(keypad, display); lv_indev_set_group(keypad, group);
        peepo::SimulatedEngine engine;
        {
            peepo::Application app(engine); step(app);
            capture(app, output / "01-patch.bmp");
            click(app, 225, 252); check(app.page() == peepo::Page::Parameters, "tap did not open parameters");
            capture(app, output / "02-parameters.bmp");
            check(exact_label(app.screen(), "TONE") && exact_label(app.screen(), "LEVEL") && bar_value(app, "DRIVE") == "32 %", "Drive's bars are not all on its page");
            slide(app, "DRIVE", 0.5);
            check(peepo::find_module(app.graph(), 3)->parameters[0].value > 70, "dragging a bar did not update the authoritative value");
            check(bar_value(app, "DRIVE") == peepo::format_value(peepo::find_module(app.graph(), 3)->parameters[0]), "the bar does not show the new value");
            click(app, 70, 456); check(app.page() == peepo::Page::Patch, "back failed");
            click(app, 150, 218);
            check(app.graph().connections.size() == 8 && !app.pending(), "cable selection changed graph");
            capture(app, output / "10-selected-cable.bmp");
            click(app, 344, 456);
            check(app.graph().connections.size() == 8, "deselect deleted cable");
            click(app, 150, 218); click(app, 500, 456);
            check(app.graph().connections.size() == 7, "explicit cable delete failed");
            check(app.graph().connections.front().from == 3, "cable hit test removed the wrong cable");
            pointer = {116, 218}; pressed = true; step(app, 64);
            capture(app, output / "14-cable-targets.bmp");
            pressed = false; step(app);
            drag(app, 116, 218, 184, 218);
            check(app.graph().connections.size() == 8, "reconnect cable failed");
            click(app, 398, 244);
            // All six of Clouds' controls on one page: no pages to turn.
            for (const char* name : {"POSITION", "SIZE", "MIX", "FREEZE", "DENSITY", "PITCH"}) check(exact_label(app.screen(), name) != nullptr, "a Clouds control is not on its page");
            check(bar_value(app, "FREEZE") == "OFF", "a switch does not say it is off");
            tap_bar(app, "FREEZE");
            check(peepo::find_module(app.graph(), 4)->parameters[3].value == 1 && bar_value(app, "FREEZE") == "ON", "tapping the freeze switch failed");
            capture(app, output / "07-clouds.bmp");
            slide(app, "DENSITY", 0.5);
            check(peepo::find_module(app.graph(), 4)->parameters[4].value > 70, "dragging Density changed the wrong value");
            slide(app, "PITCH", 0.4);
            const auto pitch = peepo::find_module(app.graph(), 4)->parameters[5].value;
            check(pitch > 12 && pitch == std::round(pitch), "integer bar did not quantize pitch");
            // A tap on a sliding bar only picks it.
            tap_bar(app, "MIX");
            check(peepo::find_module(app.graph(), 4)->parameters[2].value == 40 && !app.pending(), "tapping a sliding bar changed its value");
            check(peepo::find_module(app.graph(), 4)->parameters[3].value == 1, "other edits lost the freeze value");
            click(app, 70, 456);
            click(app, 562, 244);
            // A mode steps to the next on each tap, and wraps.
            check(bar_value(app, "MODE") == "CLEAN", "the Delay mode is not shown");
            tap_bar(app, "MODE");
            check(peepo::find_module(app.graph(), 5)->parameters[3].value == 1 && bar_value(app, "MODE") == "TAPE", "tapping the mode did not step it");
            capture(app, output / "08-delay.bmp");
            click(app, 70, 456);
            drag(app, 225, 244, 225, 124);
            check(peepo::find_module(app.graph(), 3)->y == 24, "module drag failed to snap");
            check(app.page() == peepo::Page::Patch, "drag accidentally opened parameters");
            click(app, 490, 36); check(app.page() == peepo::Page::Browser, "browser navigation failed");
            check(text_label(app.screen(), "ALL // ") != nullptr && exact_label(app.screen(), "CLOUDS") != nullptr, "browser did not open on every group");
            check(text_label(app.screen(), "BPM") == nullptr, "browser kept the status bar");
            capture(app, output / "03-browser.bmp");
            tap(app, "TIME");
            check(text_label(app.screen(), "Room, hall") != nullptr && exact_label(app.screen(), "CLOUDS") == nullptr, "group filter did not apply");
            check(text_label(app.screen(), "TIME // 2 MODULES") != nullptr, "status does not count the group");
            tap(app, "REVERB"); check(app.page() == peepo::Page::Browser && app.graph().modules.size() == 5, "first tap on a card added it");
            capture(app, output / "03-browser-group.bmp");
            click(app, 702, 456); check(app.graph().modules.size() == 6, "add module failed");
            check(app.page() == peepo::Page::Patch, "add did not return to patch");
            // Drop a cable from IN onto the added Reverb's input hit area.
            const auto* reverb = peepo::find_module(app.graph(), 6); check(reverb != nullptr, "new instance missing");
            drag(app, 116, 218, reverb->x, reverb->y + 86);
            check(app.graph().connections.size() == 9, "output to input drag failed");
            click(app, 707, 36); check(app.page() == peepo::Page::Settings, "settings navigation failed");
            capture(app, output / "04-settings.bmp");
            click(app, 273, 238); check(engine.state() == peepo::EngineState::Disconnected, "disconnect control failed");
            click(app, 70, 456); click(app, 396, 244);
            check(app.page() == peepo::Page::Parameters, "module navigation while disconnected failed");
            const auto old_mix = peepo::find_module(app.graph(), 4)->parameters[2].value;
            slide(app, "MIX", 0.5);
            check(peepo::find_module(app.graph(), 4)->parameters[2].value == old_mix, "a bar dragged while disconnected changed the engine");
            click(app, 70, 456); click(app, 716, 36); click(app, 104, 238);
            check(engine.state() == peepo::EngineState::Ready, "reconnect control failed");
            click(app, 135, 312); click(app, 70, 456); click(app, 396, 244);
            tap_header(app, "ON"); check(!peepo::find_module(app.graph(), 4)->bypassed, "rejected bypass accepted");
            check(app.message().find("rejection") != std::string::npos, "rejection feedback missing");
            tap_header(app, "ON"); check(peepo::find_module(app.graph(), 4)->bypassed, "accepted bypass missing");
            capture(app, output / "60-bypassed-module.bmp");
            tap_header(app, "BYPASSED"); check(!peepo::find_module(app.graph(), 4)->bypassed, "the Bypassed pill did not switch the module on");
            tap_header(app, "ON");
            click(app, 70, 456); click(app, 600, 36); click(app, 130, 364);
            capture(app, output / "05-session.bmp");
            click(app, 70, 456); click(app, 396, 244); tap_header(app, "ACTIONS"); click(app, 592, 252);
            check(app.graph().modules.size() == 5, "delete module failed");
            click(app, 600, 36); click(app, 700, 264);
            check(app.page() == peepo::Page::ConfirmReplace, "unsaved patch load was not guarded");
            click(app, 650, 364);
            check(app.graph().modules.size() == 6, "session restore failed");
            capture(app, output / "06-edited-patch.bmp");
            // Pan the view without changing any graph coordinates or dirty state.
            const auto before_pan = app.graph();
            drag(app, 420, 384, 260, 384);
            for (std::size_t i = 0; i < app.graph().modules.size(); ++i)
                check(app.graph().modules[i].x == before_pan.modules[i].x && app.graph().modules[i].y == before_pan.modules[i].y,
                    "panning changed module coordinates");
            check(app.graph().dirty == before_pan.dirty && !app.pending(), "panning submitted an engine edit");
            capture(app, output / "11-panned-canvas.bmp");
            // Cable hit testing and module gestures use the same viewport transform.
            click(app, 474, 218); check(app.graph().connections.size() == 9, "panned cable tap deleted cable");
            click(app, 500, 456); check(app.graph().connections.size() == 8, "panned cable delete missed target");
            drag(app, 444, 218, 504, 218); check(app.graph().connections.size() == 9, "panned cable reconnection failed");
            drag(app, 408, 244, 456, 244);
            check(peepo::find_module(app.graph(), 5)->x == 576, "panned module movement lost world coordinates");
            hold(app, 236, 244); check(app.page() == peepo::Page::ModuleActions, "long press did not open actions");
            capture(app, output / "12-module-actions.bmp");
            const auto source = *peepo::find_module(app.graph(), 4);
            click(app, 170, 252); check(app.graph().modules.size() == 7, "context duplicate failed");
            const auto copy = app.graph().modules.back();
            check(copy.id != source.id && copy.uri == source.uri && copy.bypassed == source.bypassed &&
                copy.parameters[3].value == source.parameters[3].value, "UI duplicate did not preserve state");
            check(app.graph().connections.size() == 9, "UI duplicate copied cables");
            click(app, 276, 36); // Tapping the patch name resets the viewport, not the graph.
            click(app, source.x + 42, source.y + 100);
            check(app.page() == peepo::Page::Parameters, "tapping the patch name did not restore original module location");
            tap_header(app, "ACTIONS"); check(app.page() == peepo::Page::ModuleActions, "parameter Actions button failed");
            click(app, 170, 180); check(app.page() == peepo::Page::Parameters, "context Parameters action failed");
            click(app, 70, 456);
            engine.reject_next();
            hold(app, source.x + 42, source.y + 100); click(app, 170, 252);
            check(app.graph().modules.size() == 7 && app.message().find("rejection") != std::string::npos,
                "rejected context duplicate changed graph or lost feedback");
            hold(app, copy.x + 42, copy.y + 100);
            check(app.page() == peepo::Page::ModuleActions, "duplicate module did not open context actions");
            click(app, 600, 180);
            check(peepo::find_module(app.graph(), copy.id)->bypassed != copy.bypassed, "context software bypass failed");
            click(app, 600, 252);
            check(!peepo::find_module(app.graph(), copy.id) && app.graph().connections.size() == 9, "context delete changed unrelated cables");
            // Run enough redraw cycles to expose stale bindings and queued callbacks.
            for (int i = 0; i < 10; ++i) { click(app, 490, 36); click(app, 70, 456); }
            check(!app.pending(), "pending requests never resolved");
            // Replace the graph through the backend and fill the visible grid.
            auto dense = peepo::demo_graph(); dense.name = "Dense patch";
            dense.modules.resize(2); dense.connections.clear();
            dense.modules[0].x = 24; dense.modules[0].y = 24;
            dense.modules[1].x = 600; dense.modules[1].y = 264;
            const auto* drive = peepo::definition("urn:peepo:demo:drive");
            peepo::InstanceId id = 10;
            for (int y : {24, 144, 264}) for (int x : {24, 168, 312, 456, 600}) {
                if ((x == 24 && y == 24) || (x == 600 && y == 264)) continue;
                dense.modules.push_back({id++, drive->uri, drive->name, x, y, false, false, drive->parameters});
            }
            engine.submit(peepo::LoadGraph{dense}); step(app, 256);
            check(app.graph().name == "Dense patch" && app.graph().modules.size() == 15, "backend graph replacement was not displayed");
            capture(app, output / "15-dense-canvas.bmp");
            add_module(app, "TEXTURE", "CLOUDS");
            check(app.graph().modules.size() == 16 && app.graph().modules.back().x > 672, "full viewport prevented adding beyond it");
            capture(app, output / "13-larger-canvas.bmp");
            click(app, 66, 112);
            check(app.page() == peepo::Page::Parameters, "new offscreen module was not revealed after add");
            click(app, 70, 456); click(app, 276, 36);
            engine.submit(peepo::LoadGraph{peepo::demo_graph()}); step(app, 256);
            click(app, 707, 36); click(app, 374, 312);
            check(app.page() == peepo::Page::Patch && app.graph().connections.size() == 8 &&
                !peepo::module_available(*peepo::find_module(app.graph(), 4)), "missing module fixture lost graph");
            check(text_label(app.screen(), "Not available here: Missing texture") != nullptr, "the canvas did not say which module is missing");
            capture(app, output / "16-missing-module.bmp");
            click(app, 396, 244); check(app.page() == peepo::Page::Parameters, "missing placeholder could not be inspected");
            capture(app, output / "17-missing-parameters.bmp");
            check(widget(app.screen(), &lv_slider_class) == nullptr, "missing module exposed editable sliders");
            click(app, 70, 456); hold(app, 396, 244);
            check(app.page() == peepo::Page::ModuleActions && text_label(app.screen(), "Not available: ") && text_label(app.screen(), "until you save again"),
                  "the missing module's actions did not explain it");
            capture(app, output / "45-missing-actions.bmp");
            click(app, 170, 252);
            check(app.graph().modules.size() == 5 && !app.pending(), "disabled missing-module duplication submitted an edit");
            click(app, 600, 252); check(app.graph().modules.size() == 4 && app.graph().connections.size() == 4, "missing placeholder deletion failed");
            click(app, 490, 36); tap(app, "GAIN");
            capture(app, output / "18-catalogue-status.bmp");
            check(text_label(app.screen(), "compatibility") != nullptr, "untested entry reason missing");
            pick_and_add(app, "NAM FIXTURE"); check(app.page() == peepo::Page::Browser && app.graph().modules.size() == 4 && !app.pending(), "untested catalogue entry could be added");
            check(text_label(app.screen(), "asset") != nullptr, "unavailable entry reason missing");
            pick_and_add(app, "CABINET FIXTURE"); check(app.page() == peepo::Page::Browser && app.graph().modules.size() == 4, "unavailable catalogue entry could be added");
            tap(app, "CV + UTIL"); pick_and_add(app, "MONO UTILITY");
            check(app.graph().modules.back().uri == "urn:peepo:demo:mono", "available mono fixture could not be added");

            // Create, cancel and rename through actual on-screen keyboard presses.
            click(app, 600, 36); click(app, 320, 364);
            check(app.page() == peepo::Page::NamePatch, "new patch naming did not open");
            click(app, 710, 36); check(app.graph().modules.size() == 5 && app.message().find("Enter a patch name") != std::string::npos, "blank name was accepted");
            for (const auto* key : {"t", "e", "s", "t"}) keyboard_key(app, key);
            capture(app, output / "20-name-keyboard.bmp");
            click(app, 710, 36); check(app.page() == peepo::Page::ConfirmReplace, "new patch did not guard unsaved edits");
            capture(app, output / "21-replace-confirmation.bmp");
            click(app, 130, 364); check(app.page() == peepo::Page::NamePatch && app.graph().modules.size() == 5, "cancel replaced graph or lost draft");
            check(std::string(lv_textarea_get_text(widget(app.screen(), &lv_textarea_class))) == "test", "cancel lost typed name");
            keyboard_key(app, LV_SYMBOL_BACKSPACE); keyboard_key(app, "e");
            keyboard_key(app, LV_SYMBOL_OK);
            check(app.page() == peepo::Page::ConfirmReplace, "keyboard OK did not request create");
            engine.reject_next(); click(app, 650, 364);
            check(app.graph().modules.size() == 5 && app.message().find("rejection") != std::string::npos, "rejected new patch replaced graph");
            click(app, 650, 364); check(app.page() == peepo::Page::Patch && app.graph().name == "tese" && app.graph().modules.size() == 2, "new patch accepted wrong name or endpoints");
            capture(app, output / "22-new-patch.bmp");
            click(app, 600, 36); click(app, 505, 364);
            keyboard_key(app, LV_SYMBOL_BACKSPACE); keyboard_key(app, "t");
            keyboard_key(app, LV_SYMBOL_OK);
            check(app.page() == peepo::Page::Patches && app.graph().name == "test", "rename via keyboard failed");
            click(app, 130, 364); check(!app.graph().dirty, "accepted session save did not clear dirty marker");
            click(app, 320, 364); keyboard_key(app, "a"); keyboard_key(app, LV_SYMBOL_KEYBOARD);
            check(app.page() == peepo::Page::Patches && app.graph().name == "test", "keyboard cancel changed patch");
            click(app, 320, 364); keyboard_key(app, "a"); keyboard_key(app, LV_SYMBOL_OK);
            check(app.page() == peepo::Page::Patch && app.graph().name == "a", "clean patch creation required an unnecessary replacement guard");
            click(app, 600, 36); click(app, 700, 264); click(app, 650, 364);
            check(app.graph().name == "test" && !app.graph().dirty, "saved session did not survive new patch");
            check(!app.pending(), "new editor requests did not resolve");

            // Long names, clamped edge movement and keypad traversal.
            auto names = peepo::demo_graph(); names.name = "A very long patch name that must leave navigation visible";
            names.modules[2].name = "An exceptionally long effect module display name";
            engine.submit(peepo::LoadGraph{names}); step(app, 256); click(app, 276, 36);
            capture(app, output / "23-long-names.bmp");
            drag(app, 232, 244, 30, 90);
            check(peepo::find_module(app.graph(), 3)->x == peepo::module_min_x && peepo::find_module(app.graph(), 3)->y == peepo::module_min_y, "edge drag did not clamp and snap");
            check(app.page() == peepo::Page::Patch && !app.pending(), "edge drag opened parameters or left pending work");
            click(app, 276, 36);
            auto* focus = lv_group_get_focused(group);
            keypad_press(app, LV_KEY_NEXT); check(lv_group_get_focused(group) != focus, "Tab did not traverse controls");
            keypad_press(app, LV_KEY_ENTER); check(app.page() == peepo::Page::Browser, "keyboard Enter did not activate Add");
            click(app, 70, 456); check(app.page() == peepo::Page::Patch, "browser Back did not return to the patch"); click(app, 600, 36); click(app, 320, 364);
            keypad_press(app, 'p'); keypad_press(app, 'c');
            check(std::string(lv_textarea_get_text(widget(app.screen(), &lv_textarea_class))) == "pc", "physical keyboard did not enter name");
            keypad_press(app, LV_KEY_ENTER);
            check(app.page() == peepo::Page::ConfirmReplace, "physical Enter did not request new patch");
            click(app, 130, 364); keyboard_key(app, LV_SYMBOL_KEYBOARD);
            check(app.graph().name == names.name, "keyboard cancellation changed current graph");
            click(app, 505, 364);
            check(std::string(lv_textarea_get_text(widget(app.screen(), &lv_textarea_class))) == names.name,
                "opening rename silently truncated a backend name");
            keyboard_key(app, LV_SYMBOL_KEYBOARD);
            // Every module uses the same page, built from its controls, so the
            // values are the engine's whatever the order or the display name.
            click(app, 70, 456);
            engine.submit(peepo::LoadGraph{peepo::demo_graph()}); step(app, 256);
            click(app, 396, 244);
            slide(app, "DENSITY", 0.5);
            check(peepo::find_module(app.graph(), 4)->parameters[4].value > 70, "Clouds density did not update");
            tap_bar(app, "FREEZE");
            check(peepo::find_module(app.graph(), 4)->parameters[3].value == 1, "Clouds freeze failed");
            slide(app, "MIX", 0.5);
            check(peepo::find_module(app.graph(), 4)->parameters[2].value > 70, "Clouds mix failed");
            click(app, 70, 456); click(app, 564, 244);
            engine.reject_next(); tap_bar(app, "MODE");
            check(peepo::find_module(app.graph(), 5)->parameters[3].value == 0 && !app.pending() && bar_value(app, "MODE") == "CLEAN", "a rejected mode left optimistic state");
            tap_bar(app, "MODE"); tap_bar(app, "MODE");
            check(peepo::find_module(app.graph(), 5)->parameters[3].value == 2, "Delay Ping pong failed");
            tap_bar(app, "MODE");
            check(peepo::find_module(app.graph(), 5)->parameters[3].value == 0, "the mode did not wrap to the first");
            slide(app, "TIME", 0.6);
            check(peepo::find_module(app.graph(), 5)->parameters[0].value > 800, "logarithmic delay time failed");
            slide(app, "FEEDBACK", 0.5);
            check(peepo::find_module(app.graph(), 5)->parameters[1].value > 70, "Delay feedback failed");
            capture(app, output / "26-delay-character.bmp");
            click(app, 70, 456);

            auto reordered = peepo::demo_graph();
            reordered.modules[3].name = "Texture voice";
            std::reverse(reordered.modules[3].parameters.begin(), reordered.modules[3].parameters.end());
            engine.submit(peepo::LoadGraph{reordered}); step(app, 256); click(app, 396, 244);
            tap_bar(app, "FREEZE");
            const auto* reordered_clouds = peepo::find_module(app.graph(), 4);
            const auto freeze = std::find_if(reordered_clouds->parameters.begin(), reordered_clouds->parameters.end(), [](const peepo::Parameter& p) { return p.symbol == "freeze"; });
            check(freeze != reordered_clouds->parameters.end() && freeze->value == 1, "controls depended on parameter index");
            click(app, 70, 456);

            // Extra metadata is just one more bar.
            auto extended = peepo::demo_graph();
            extended.dirty = true;
            extended.modules[3].parameters.push_back({"extra", "Extra", "%", peepo::ParameterKind::Continuous, 0, 100, 50});
            engine.submit(peepo::LoadGraph{extended}); step(app, 256); click(app, 396, 244);
            slide(app, "EXTRA", 0.5);
            check(peepo::find_module(app.graph(), 4)->parameters.back().value > 70, "extended schema extra parameter inaccessible");
            click(app, 70, 456);

            // The preset uses the same replacement guard and graph commands as session restore.
            click(app, 600, 36); click(app, 690, 364);
            check(app.page() == peepo::Page::ConfirmReplace, "parallel demo did not guard unsaved edits");
            click(app, 130, 364); check(app.graph().modules[3].parameters.size() == 7, "cancelled demo lost current graph");
            click(app, 690, 364); engine.reject_next(); click(app, 650, 364);
            check(app.graph().modules[3].parameters.size() == 7 && !app.pending(), "rejected demo replaced current graph");
            click(app, 650, 364);
            check(app.page() == peepo::Page::Patch && app.graph().name == "Parallel texture" && app.graph().connections.size() == 10, "parallel starter did not load");
            capture(app, output / "27-parallel-patch.bmp");
            // Delete one side of the split; the other side and both merge cables survive.
            click(app, 342, 164); click(app, 500, 456);
            check(app.graph().connections.size() == 9, "parallel branch cable could not be removed");
            check(std::find(app.graph().connections.begin(), app.graph().connections.end(), peepo::Connection{3, 5}) != app.graph().connections.end(), "removing one branch removed the other");
            drag(app, 276, 218, 408, 110);
            check(app.graph().connections.size() == 10 && peepo::validate_graph(app.graph()).empty(), "parallel branch could not be reconnected");

            // Reference metadata: a square with two input jacks and two output jacks.
            const auto* fold = peepo::definition("http://polyeffects.com/lv2/polywarps#fold");
            check(fold != nullptr, "reference Warps entry missing");
            auto multi = peepo::demo_graph(); multi.name = "Multi-port";
            multi.modules.push_back({6, fold->uri, fold->name, 352, 264, false, false, fold->parameters, fold->ports});
            engine.submit(peepo::LoadGraph{multi}); step(app, 256);
            const auto has = [&](const peepo::Connection& c) {
                return std::find(app.graph().connections.begin(), app.graph().connections.end(), c) != app.graph().connections.end();
            };
            drag(app, 268, 218, 352, 378);
            check(has({3, 6, "out", "modulator"}), "cable dropped on the second input jack reached another port");
            drag(app, 436, 378, 664, 218);
            check(has({6, 2, "aux", "in"}), "cable dragged from the second output jack left another port");
            drag(app, 268, 218, 352, 350);
            check(has({3, 6, "out", "carrier"}) && app.graph().connections.size() == 11, "first input jack could not be patched separately");
            // Left output through one effect, right output through another.
            click(app, 310, 246); click(app, 500, 456);
            check(!has({3, 4, "out_r", "in_r"}), "right-lane cable could not be removed on its own");
            drag(app, 268, 246, 352, 378);
            check(has({3, 4}) && has({3, 6, "out_r", "modulator"}), "left and right outputs could not feed different effects");
            capture(app, output / "28-multi-port.bmp");

            // Continuous controls reach the engine while the finger is still down.
            click(app, 232, 252); check(app.page() == peepo::Page::Parameters, "Drive parameters did not open");
            const auto knob = bar_point(app, "DRIVE");
            pointer = knob; pressed = true; step(app, 64);
            pointer = {knob.x + 20, knob.y}; step(app, 64);
            pointer = {knob.x + 186, knob.y}; step(app, 320);
            check(peepo::find_module(engine.snapshot(), 3)->parameters[0].value > 70, "a dragged bar waited for release before reaching the engine");
            pointer = {knob.x + 10, knob.y}; step(app, 320);
            check(peepo::find_module(engine.snapshot(), 3)->parameters[0].value < 50, "newest gesture value did not replace the older one");
            pressed = false; step(app);
            check(!app.pending() && peepo::find_module(app.graph(), 3)->parameters[0].value < 50, "released slider left pending or stale state");
            click(app, 70, 456);

            // Add the real granular module from the browser and open its generic pages.
            click(app, 490, 36); tap(app, "TEXTURE");
            capture(app, output / "29-reference-catalogue.bmp");
            pick_and_add(app, "MIST GRANULAR");
            const auto mist = app.graph().modules.back();
            check(mist.uri == "http://polyeffects.com/lv2/polyclouds#granular" && app.page() == peepo::Page::Patch, "reference module could not be added from the browser");
            click(app, mist.x + 42, mist.y + 100);
            check(app.page() == peepo::Page::Parameters, "reference module has no page");
            // Twelve controls, all in view at once.
            for (const char* name : {"POSITION", "SIZE", "PITCH", "IN GAIN", "DENSITY", "TEXTURE", "BLEND", "SPREAD", "FEEDBACK", "REVERB", "FREEZE", "REVERSE"}) {
                auto* l = exact_label(app.screen(), name); check(l != nullptr, "a Mist Granular control is missing");
                lv_area_t area; lv_obj_get_coords(lv_obj_get_parent(l), &area);
                check(area.y1 >= 72 && area.y2 < 432, "twelve controls do not fit on one page");
            }
            capture(app, output / "30-reference-parameters.bmp");
            click(app, 70, 456);

            // Control cable: drag from the top connector of an envelope to the
            // underside of the granular module, then choose the ports.
            const auto* tides = peepo::definition("http://polyeffects.com/lv2/polytides");
            const auto* granular = peepo::definition("http://polyeffects.com/lv2/polyclouds#granular");
            check(tides && granular, "reference control modules missing");
            auto modulated = peepo::empty_graph("Control cables");
            modulated.modules.push_back({6, tides->uri, tides->name, 184, 240, false, false, tides->parameters, tides->ports});
            modulated.modules.push_back({7, granular->uri, granular->name, 352, 24, false, false, granular->parameters, granular->ports});
            engine.submit(peepo::LoadGraph{modulated}); step(app, 256); click(app, 276, 36);
            drag(app, 226, 312, 394, 152);
            check(app.page() == peepo::Page::CvConnect && !app.pending(), "control cable drop did not ask for ports");
            capture(app, output / "31-control-ports.bmp");
            click(app, 200, 160);
            check(app.page() == peepo::Page::CvConnect, "choosing the output closed the port list");
            click(app, 200, 160);
            check(app.page() == peepo::Page::Patch && has({6, 7, "out1", "freeze"}), "control cable was not created between the chosen ports");
            capture(app, output / "32-control-cable.bmp");
            drag(app, 226, 312, 80, 300);
            check(app.page() == peepo::Page::Patch && app.graph().connections.size() == 3, "control cable dropped on nothing changed the graph");
            click(app, 310, 232);
            check(app.message().find("freeze") != std::string::npos, "selected control cable was not named");
            click(app, 500, 456);
            check(!has({6, 7, "out1", "freeze"}), "control cable could not be deleted");
        }
        // Saved patches on disk, through the same Patches page and real clicks.
        const auto root = std::filesystem::temp_directory_path() / "peepo-ui-patches";
        std::filesystem::remove_all(root);
        {
            peepo::PatchStore store(root);
            check(store.open().empty(), "patch store did not open");
            peepo::SimulatedEngine stored_engine;
            peepo::Application app(stored_engine, &store); step(app);
            click(app, 600, 36); check(app.page() == peepo::Page::Patches && text_label(app.screen(), "Nothing saved yet") != nullptr, "empty store not shown");
            click(app, 94, 376); step(app, 256);
            check(store.list().size() == 1 && app.graph().patch_id == store.list()[0].id && !app.graph().dirty, "Save did not store the patch");
            const auto first = store.list()[0].id;
            check(text_label(app.screen(), "Revision 1") != nullptr && text_label(app.screen(), "Playing: First light / saved") != nullptr, "saved patch not listed");
            click(app, 70, 456); drag(app, 225, 244, 225, 124); check(app.graph().dirty, "edit after save not marked");
            click(app, 600, 36); check(text_label(app.screen(), "playing, unsaved edits") != nullptr, "unsaved edits not shown on the stored patch");
            click(app, 94, 376); step(app, 256);
            check(store.find(first)->revision == 2 && !app.graph().dirty && store.list().size() == 1, "second save did not add a revision");
            // Save as: a separate stored patch under a new name.
            click(app, 246, 376); check(app.page() == peepo::Page::NamePatch, "Save as did not ask for a name");
            keyboard_key(app, LV_SYMBOL_BACKSPACE); keyboard_key(app, "x");
            click(app, 710, 36); step(app, 256);
            check(store.list().size() == 2 && app.graph().name == "First lighx" && app.graph().patch_id != first, "Save as did not create a second patch");
            capture(app, output / "33-stored-patches.bmp");
            // Rows are by name: "First light", then "First lighx". Mark the first as the startup patch.
            click(app, 628, 159);
            check(store.startup() == first && text_label(app.screen(), "loads at startup") != nullptr, "startup choice not recorded");
            // Load the first; the current one is saved, so nothing is guarded.
            click(app, 520, 159); step(app, 256);
            check(app.page() == peepo::Page::Patch && app.graph().patch_id == first && app.graph().name == "First light" && peepo::find_module(app.graph(), 3)->y == 24, "stored patch not loaded");
            // A patch whose module is unavailable asks before loading the rest.
            stored_engine.submit(peepo::LoadGraph{peepo::missing_module_graph()}); step(app, 256);
            click(app, 600, 36); click(app, 246, 376);
            for (const auto* key : {"z", "z"}) keyboard_key(app, key);
            click(app, 710, 36); step(app, 256);
            const auto missing = app.graph().patch_id;
            check(store.find(missing).has_value(), "patch with a missing module not saved");
            click(app, 70, 456); click(app, 600, 36);
            const auto listed = store.list();
            const auto row = std::find_if(listed.begin(), listed.end(), [&](const peepo::StoredPatch& p) { return p.id == missing; }) - listed.begin();
            click(app, 520, 159 + static_cast<int>(row) * 64);
            check(app.page() == peepo::Page::ConfirmAction && text_label(app.screen(), "Not available here") != nullptr, "missing module did not ask before loading");
            capture(app, output / "34-missing-on-load.bmp");
            click(app, 664, 364); step(app, 256);
            check(app.graph().patch_id == missing && app.graph().dirty && app.message().find("Left out: Missing texture") != std::string::npos && !peepo::find_module(app.graph(), 4),
                  "load without the missing module failed or went unreported");
            // Delete asks first, and leaves the playing patch alone.
            click(app, 600, 36);
            click(app, 728, 159 + static_cast<int>(row) * 64);
            check(app.page() == peepo::Page::ConfirmAction, "delete did not ask");
            click(app, 130, 364); check(store.list().size() == 3, "cancelled delete removed the patch");
            click(app, 728, 159 + static_cast<int>(row) * 64); click(app, 664, 364);
            check(store.list().size() == 2 && !store.find(missing) && app.graph().patch_id == missing, "delete failed or changed the playing patch");

            // The engine away: the last graph stays, with a banner, and nothing can be added.
            stored_engine.set_state(peepo::EngineState::Connecting); step(app);
            click(app, 70, 456);
            check(text_label(app.screen(), "Engine connection lost") != nullptr, "lost engine not shown");
            capture(app, output / "35-reconnecting.bmp");
            click(app, 490, 36); check(app.page() == peepo::Page::Patch, "Add opened while the engine was away");
            click(app, 232, 244); check(app.page() == peepo::Page::Parameters, "the last graph could not be inspected while away");
            click(app, 70, 456);
            stored_engine.set_state(peepo::EngineState::Ready); step(app);
            check(text_label(app.screen(), "Engine connection lost") == nullptr, "banner stayed after reconnecting");
        }
        {
            // Starting up: a store with a startup patch loads it into an engine that plays nothing stored.
            peepo::PatchStore store(root);
            check(store.open().empty() && !store.startup().empty(), "startup choice lost");
            peepo::SimulatedEngine fresh;
            peepo::Application app(fresh, &store); step(app, 256);
            check(app.graph().patch_id == store.startup() && peepo::find_module(app.graph(), 3)->y == 24, "startup patch not loaded");
            // An interface restart over an engine playing something else leaves it alone.
            fresh.submit(peepo::NewPatch{"Live edit"}); step(app, 256);
            peepo::Application again(fresh, &store); step(again, 256);
            check(again.graph().name == "Live edit", "restart replaced an unsaved patch with the startup patch");
        }
        {
            // A footswitch set to change patch steps through the stored patches, in list order, and wraps.
            peepo::PatchStore store(root);
            check(store.open().empty() && store.set_global_control("stomp3", "next-patch").empty(), "global stomp action not stored");
            peepo::SimulatedEngine stepping;
            peepo::Application app(stepping, &store); step(app, 256);
            check(app.graph().name == "First light", "startup patch not loaded for patch stepping");
            app.control({peepo::ControlEvent::Type::Press, "stomp3"}); step(app, 256);
            app.control({peepo::ControlEvent::Type::Release, "stomp3"}); step(app);
            check(app.graph().name == "First lighx", "next-patch footswitch did not load the next patch");
            app.control({peepo::ControlEvent::Type::Press, "stomp3"}); step(app, 256);
            app.control({peepo::ControlEvent::Type::Release, "stomp3"}); step(app);
            check(app.graph().name == "First light", "next-patch footswitch did not wrap around");
        }
        std::filesystem::remove_all(root);
        {
            // Encoders and stomps through the same entry point the hardware will use.
            using Type = peepo::ControlEvent::Type;
            peepo::SimulatedEngine engine2;
            peepo::Application app(engine2); step(app);
            auto turn = [&](const char* control, int steps) { app.control({Type::Turn, control, steps}); step(app, 256); };
            auto press = [&](const char* control) { app.control({Type::Press, control}); step(app, 64); };
            auto release = [&](const char* control) { app.control({Type::Release, control}); step(app, 256); };
            auto value = [&](peepo::InstanceId id, std::size_t index) { return peepo::find_module(app.graph(), id)->parameters[index].value; };
            // On the patch, encoder 1 picks a module in signal order; encoder 2 has nothing to turn.
            turn("enc1", 1);
            check(app.message().find("push encoder 1 to open it") != std::string::npos && !app.pending(), "encoder 1 did not pick a module on the patch");
            // In the browser encoder 2 picks the group, encoder 1 the card, a push adds it.
            click(app, 490, 36);
            turn("enc2", 3); check(text_label(app.screen(), "TIME // ") != nullptr, "encoder 2 did not step to the third group");
            turn("enc2", -4); check(text_label(app.screen(), "UTIL // ") != nullptr, "encoder 2 did not wrap to the last group");
            turn("enc2", 1); turn("enc1", 1); press("enc1"); release("enc1");
            check(app.page() == peepo::Page::Patch && app.graph().modules.size() == 6 && app.graph().modules.back().uri == "urn:peepo:demo:drive", "encoder 1 did not pick and add the first card");
            // On a module page encoder 1 moves the highlight and encoder 2 turns what it is on.
            click(app, 232, 252); check(app.page() == peepo::Page::Parameters, "Drive parameters did not open");
            capture(app, output / "36-encoders-follow.bmp");
            turn("enc2", 3);
            check(std::abs(value(3, 0) - 38) < 1e-9 && bar_value(app, "DRIVE") == "38 %", "encoder 2 did not turn the highlighted bar by three detents");
            turn("enc1", 2); turn("enc2", -1);
            check(std::abs(value(3, 2) - -3.72) < 1e-9, "encoder 1 did not move the highlight to the third bar");
            turn("enc1", 5); turn("enc2", -1);
            check(std::abs(value(3, 2) - -4.44) < 1e-9, "the highlight did not stop at the last bar");
            turn("enc1", -9);
            press("enc2"); turn("enc2", 1); release("enc2");
            check(std::abs(value(3, 0) - 38.2) < 1e-9 && app.page() == peepo::Page::Parameters, "push and turn was not fine, or ended as a tap");
            // A single encoder does both: its push on a bar starts and stops turning it.
            turn("enc1", 1);
            const auto tone = value(3, 1);
            press("enc1"); release("enc1"); turn("enc1", 2);
            check(value(3, 1) > tone, "pushing encoder 1 did not let it turn the value");
            capture(app, output / "59-one-encoder.bmp");
            const auto turned = value(3, 1);
            press("enc1"); release("enc1"); turn("enc1", 1); turn("enc2", 1);
            check(value(3, 1) == turned && value(3, 2) != -4.44, "pushing again did not go back to moving the highlight");
            // Holding encoder 1 goes back.
            press("enc1"); step(app, 800); release("enc1");
            check(app.page() == peepo::Page::Patch, "holding encoder 1 did not go back");
            // A push on the patch opens the module encoder 1 is on: the one last opened.
            press("enc1"); release("enc1");
            check(app.page() == peepo::Page::Parameters && exact_label(app.screen(), "TONE"), "encoder 1 did not open the highlighted module");
            press("enc1"); step(app, 800); release("enc1");
            // A pinned encoder (Settings > Controls) turns its target on every page; a tap shows it, a hold unpins it.
            peepo::Assignment pin; pin.control = "enc2"; pin.module = 3; pin.parameter = "tone"; pin.minimum = 200; pin.maximum = 8000;
            engine2.submit(peepo::Assign{pin}); step(app, 256);
            const auto pinned_tone = value(3, 1);
            turn("enc2", 2);
            check(value(3, 1) > pinned_tone && app.page() == peepo::Page::Patch, "pinned encoder did not work from the patch page");
            press("enc2"); release("enc2");
            check(app.page() == peepo::Page::Parameters && text_label(app.screen(), "E2") != nullptr, "tapping a pinned encoder did not show its target");
            press("enc2"); step(app, 800); release("enc2");
            check(app.graph().assignments.empty(), "holding a pinned encoder did not unpin it");
            click(app, 70, 456);
            // A switch: encoder 1 moves onto it and its push flips it.
            click(app, 396, 244);
            turn("enc1", 3); press("enc1"); release("enc1");
            check(value(4, 3) == 1, "pushing encoder 1 on a switch did not flip it");
            press("enc1"); release("enc1");
            check(value(4, 3) == 0, "pushing again did not flip it back");
            // Elsewhere encoder 1 steps through the buttons and pushes the one highlighted.
            click(app, 70, 456); click(app, 707, 36);
            check(app.page() == peepo::Page::Settings, "Settings did not open");
            press("enc1"); step(app, 800); release("enc1");
            check(app.page() == peepo::Page::Patch, "holding encoder 1 did not leave Settings");
            turn("enc2", 3); press("enc2"); release("enc2");
            check(app.page() == peepo::Page::Settings, "encoder 2 did not step to and push Settings on the patch");
            click(app, 70, 456);
            // Stomps are assigned on the Controls page: stomp 1 toggles Clouds freeze.
            click(app, 707, 36); tap(app, "CONTROLS");
            check(app.page() == peepo::Page::Controls && text_label(app.screen(), "Moves through the page"), "Controls page did not open");
            click(app, 618, 204); check(app.page() == peepo::Page::AssignPicker, "Assign did not open the picker");
            click(app, 592, 160); check(text_label(app.screen(), "BYPASS") != nullptr, "picking Clouds did not list its controls");
            click(app, 208, 296);
            check(app.page() == peepo::Page::Controls && app.graph().assignments.size() == 1 && app.graph().assignments[0].parameter == "freeze", "stomp 1 not assigned to freeze");
            press("stomp1"); release("stomp1");
            check(value(4, 3) == 1, "stomp toggle did not switch freeze on");
            press("stomp1"); release("stomp1");
            check(value(4, 3) == 0, "stomp toggle did not switch freeze off");
            // Momentary: held on, released off.
            click(app, 726, 204); check(app.page() == peepo::Page::AssignmentDetail, "Edit did not open the assignment");
            click(app, 456, 156);
            check(app.graph().assignments[0].mode == peepo::SwitchMode::Momentary, "momentary mode not stored");
            capture(app, output / "37-assignment-detail.bmp");
            app.control({Type::Press, "stomp1"}); step(app, 256);
            check(value(4, 3) == 1, "momentary press did not hold freeze on");
            release("stomp1");
            check(value(4, 3) == 0, "momentary release did not let go");
            // Stomp 2 bypasses Delay.
            click(app, 70, 456); click(app, 618, 254); click(app, 208, 228); click(app, 208, 160);
            check(app.graph().assignments.size() == 2 && app.graph().assignments[1].action == peepo::ControlAction::Bypass, "bypass assignment not made");
            press("stomp2"); release("stomp2");
            check(peepo::find_module(app.graph(), 5)->bypassed, "bypass stomp did nothing");
            press("stomp3"); release("stomp3");
            check(app.message().find("Stomp 3 is not assigned") != std::string::npos, "an unassigned stomp did not say so");
            capture(app, output / "38-controls.bmp");
            // Controls are ignored while a name is typed.
            click(app, 70, 456); click(app, 70, 456); click(app, 600, 36); click(app, 320, 364);
            check(app.page() == peepo::Page::NamePatch, "name page did not open");
            press("stomp2"); release("stomp2");
            check(peepo::find_module(app.graph(), 5)->bypassed, "a stomp acted while a name was being typed");
            keyboard_key(app, LV_SYMBOL_KEYBOARD);
            // A removed module takes its assignment along.
            engine2.submit(peepo::RemoveModule{4}); step(app, 256);
            check(app.graph().assignments.size() == 1 && app.graph().assignments[0].control == "stomp2", "removed module kept its stomp");
        }
        // Drive the real tuner page with test telemetry through the engine contract.
        {
            struct TunerEngine : peepo::Engine {
                peepo::SimulatedEngine backing;
                std::optional<double> frequency = 440;
                std::optional<peepo::InstanceId> watched;
                peepo::RequestId submit(peepo::Command c) override { return backing.submit(std::move(c)); }
                void connect() override { backing.connect(); }
                void disconnect() override { backing.disconnect(); }
                std::vector<peepo::EngineEvent> advance(unsigned ms) override { return backing.advance(ms); }
                peepo::EngineState state() const override { return backing.state(); }
                const peepo::Graph& snapshot() const override { return backing.snapshot(); }
                peepo::EngineCapabilities capabilities() const override { return {false, true, false, false, true}; }
                void watch_tuner(std::optional<peepo::InstanceId> id) override { watched = id; }
                std::optional<double> tuner_frequency(peepo::InstanceId id) const override { return watched == id ? frequency : std::nullopt; }
            } live;
            const std::string uri = "http://gareus.org/oss/lv2/tuna#mod";
            peepo::register_definitions({{uri, "Tuner", "Utility", "Tune an instrument", 0x93a4b5,
                {{"in", peepo::Direction::Input}, {"out", peepo::Direction::Output}},
                {{"tuning", "A reference", "Hz", peepo::ParameterKind::Continuous, 220, 880, 440}}},
                {"http://gareus.org/oss/lv2/tuna#two", "Legacy tuner", "Utility", "Duplicate", 0x93a4b5, {}, {},
                 peepo::Availability::Available, {}, true, {}, {}, true}});
            live.submit(peepo::LoadGraph{peepo::empty_graph("Tuning")}); live.advance(100);
            live.submit(peepo::AddModule{uri, 184, 132}); live.advance(100);
            peepo::Application app(live); step(app);
            const auto tuner = app.graph().modules.back();
            click(app, tuner.x + 48, tuner.y + 72 + 48);
            check(text_label(app.screen(), "A4") && text_label(app.screen(), "In tune"), "tuner did not show live note and target");
            capture(app, output / "39-tuner-in-tune.bmp");
            auto* reference = bar(app, "A REFERENCE");
            live.frequency = 440 * std::pow(2.0, -23.0 / 1200); step(app);
            check(text_label(app.screen(), "Flat - tighten string") && bar(app, "A REFERENCE") == reference, "flat pitch missing or rebuilt controls");
            capture(app, output / "40-tuner-flat.bmp");
            live.frequency = 440 * std::pow(2.0, 19.0 / 1200); step(app);
            check(text_label(app.screen(), "Sharp - loosen string"), "sharp indication missing");
            capture(app, output / "41-tuner-sharp.bmp");
            live.frequency = 82.4069; step(app);
            check(exact_label(app.screen(), "E2"), "low guitar E octave incorrect");
            live.frequency = 0; step(app);
            check(text_label(app.screen(), "Waiting for a note") && !exact_label(app.screen(), "E2"), "silence retained previous note");
            capture(app, output / "42-tuner-silence.bmp");
            live.frequency = 440; live.submit(peepo::SetParameter{tuner.id, "tuning", 442}); step(app);
            check(text_label(app.screen(), "Flat - tighten string"), "A reference did not affect tuning");
            live.submit(peepo::SetBypass{tuner.id, true}); step(app);
            check(text_label(app.screen(), "Tuner bypassed"), "bypassed tuner displayed a pitch");
            live.disconnect(); step(app);
            check(!text_label(app.screen(), "A4"), "offline tuner displayed a pitch");
            click(app, 70, 456); check(!live.watched, "leaving tuner did not stop monitoring");
            live.connect(); step(app, 500);
            click(app, 490, 36); tap(app, "CV + UTIL");
            check(!text_label(app.screen(), "LEGACY TUNER"), "hidden duplicate in Add list");
            const auto invalid = peepo::tuning_from_frequency(std::nan(""), 440);
            check(!invalid && !peepo::tuning_from_frequency(440, 0), "invalid pitch/reference accepted");
            // Settings says how the instrument is reached, and follows changes while open.
            peepo::Application::Network network{true, "Wi-Fi Camarones-invitades-2.4 / 192.168.10.157 / signal good (-55 dBm)"};
            unsigned reads = 0;
            app.set_network([&] { ++reads; return network; });
            click(app, 70, 456); step(app); click(app, 707, 36);
            check(app.page() == peepo::Page::Settings && text_label(app.screen(), network.text), "settings did not show the network");
            capture(app, output / "43-settings-network.bmp");
            network = {false, "Not connected"}; step(app, 2100);
            check(text_label(app.screen(), "Not connected"), "the network line did not follow a lost connection");
            click(app, 70, 456); const auto before = reads; step(app, 4200);
            check(reads == before, "the network was read with Settings closed");
        }
        // Settings leads to Diagnostics and Power; the readings come from the platform.
        {
            const auto store_root = std::filesystem::temp_directory_path() / "peepo-ui-diagnostics";
            std::filesystem::remove_all(store_root);
            peepo::PatchStore store(store_root);
            check(store.open().empty(), "diagnostics store did not open");
            peepo::SimulatedEngine sim;
            peepo::Application app(sim, &store); step(app);
            peepo::Application::System system;
            system.temperature_c = 47.9; system.memory_total = 2053120ull * 1024; system.memory_available = 1675728ull * 1024;
            system.under_voltage = false; system.uptime_s = 11628.07;
            unsigned reads = 0;
            app.set_system([&] { ++reads; return system; });
            step(app, 256);
            check(reads == 1, "the readings were not taken at once");
            click(app, 707, 36);
            check(app.page() == peepo::Page::Settings && exact_label(app.screen(), "CONTROLS") && exact_label(app.screen(), "DIAGNOSTICS") && !exact_label(app.screen(), "POWER"),
                  "Settings pills wrong without power control");
            tap(app, "DIAGNOSTICS");
            check(app.page() == peepo::Page::Diagnostics && text_label(app.screen(), "47.9 C") && text_label(app.screen(), "1.6 GB free") &&
                  exact_label(app.screen(), "OK") && text_label(app.screen(), "3 h 13 min") && text_label(app.screen(), "SIMULATED"),
                  "Diagnostics did not show the readings");
            capture(app, output / "46-diagnostics.bmp");
            // The supply drops: said at once, on the card and in the footer.
            system.under_voltage = true; step(app, 2100);
            check(exact_label(app.screen(), "TOO LOW") && app.message().find("too low") != std::string::npos, "low voltage not reported");
            capture(app, output / "47-diagnostics-low-voltage.bmp");
            system.under_voltage = false; step(app, 2100);
            check(exact_label(app.screen(), "WAS LOW") != nullptr, "a low supply was forgotten when it recovered");
            system = {}; step(app, 2100);
            check(!text_label(app.screen(), "47.9 C") && text_label(app.screen(), "Not measured here."), "a missing reading kept an old value");
            click(app, 70, 456); check(app.page() == peepo::Page::Settings, "Diagnostics did not go back to Settings");
            // Power: shut down and restart, each confirmed, then nothing more to do.
            unsigned restarts = 0, power_offs = 0;
            std::string refusal;
            app.set_restart([&] { ++restarts; return refusal; });
            app.set_power_off([&] { ++power_offs; return refusal; });
            step(app);
            tap(app, "POWER"); check(app.page() == peepo::Page::Power && exact_label(app.screen(), "SHUT DOWN") && exact_label(app.screen(), "RESTART"), "Power page missing its choices");
            capture(app, output / "48-power.bmp");
            refusal = "Could not restart: the system refused.";
            tap(app, "RESTART"); check(app.page() == peepo::Page::ConfirmAction && text_label(app.screen(), "Restart Peepo?"), "restart was not confirmed first");
            click(app, 664, 364);
            check(restarts == 1 && app.page() == peepo::Page::Power && app.message() == refusal, "a refused restart was not reported on the Power page");
            refusal.clear();
            tap(app, "RESTART"); click(app, 130, 364);
            check(restarts == 1 && app.page() == peepo::Page::Power, "cancelled restart went ahead");
            tap(app, "SHUT DOWN"); click(app, 664, 364);
            check(power_offs == 1 && app.page() == peepo::Page::Halting && text_label(app.screen(), "shutting down") && !exact_label(app.screen(), "< BACK"),
                  "shutting down did not leave a final page");
            capture(app, output / "49-shutting-down.bmp");
            app.control({peepo::ControlEvent::Type::Turn, "enc1", 3}); step(app, 256);
            check(app.page() == peepo::Page::Halting && !app.pending(), "a control acted while shutting down");
            std::filesystem::remove_all(store_root);
        }
        // Revert: back to the last saved revision, guarded like any load.
        {
            const auto store_root = std::filesystem::temp_directory_path() / "peepo-ui-revert";
            std::filesystem::remove_all(store_root);
            peepo::PatchStore store(store_root);
            check(store.open().empty(), "revert store did not open");
            peepo::SimulatedEngine sim;
            peepo::Application app(sim, &store); step(app);
            click(app, 600, 36); check(disabled_button(exact_label(app.screen(), "REVERT")), "Revert offered for a patch never saved");
            click(app, 94, 376); step(app, 256);
            check(!app.graph().dirty && disabled_button(exact_label(app.screen(), "REVERT")), "Revert offered for a saved patch without edits");
            const auto y = peepo::find_module(app.graph(), 3)->y;
            click(app, 70, 456); drag(app, 225, 244, 225, 124); check(app.graph().dirty && peepo::find_module(app.graph(), 3)->y != y, "edit after save not made");
            click(app, 600, 36); check(!disabled_button(exact_label(app.screen(), "REVERT")), "Revert not offered for unsaved edits");
            tap(app, "REVERT"); check(app.page() == peepo::Page::ConfirmReplace, "Revert did not ask before discarding edits");
            click(app, 664, 364); step(app, 256);
            check(app.page() == peepo::Page::Patch && !app.graph().dirty && peepo::find_module(app.graph(), 3)->y == y, "Revert did not restore the saved revision");
            std::filesystem::remove_all(store_root);
        }
        // Inside a group, the finer categories filter the cards.
        {
            peepo::SimulatedEngine sim;
            peepo::Application app(sim); step(app);
            click(app, 490, 36); tap(app, "GAIN");
            check(first_label(app.screen(), "DRIVE") && first_label(app.screen(), "AMP") && first_label(app.screen(), "CABINET"), "GAIN did not offer its categories");
            tap_first(app, "AMP");
            check(text_label(app.screen(), "GAIN / AMP // 1 MODULE") && !text_label(app.screen(), "Warm saturation"), "the category did not filter the cards");
            capture(app, output / "50-browser-category.bmp");
            tap_first(app, "ALL");
            check(text_label(app.screen(), "GAIN // ") && text_label(app.screen(), "Warm saturation"), "ALL did not show the whole group again");
            tap_first(app, "AMP"); tap(app, "TIME");
            check(text_label(app.screen(), "TIME // 2 MODULES") != nullptr, "the category outlived its group");
            check(!first_label(app.screen(), "AMP"), "another group's categories shown");
        }
        // The overlay's groups as headings on the one page, then the rest under
        // More; whole numbers step by one; a mode steps on each tap.
        {
            const std::string uri = "urn:peepo:test:grouped";
            auto grouped = peepo::ModuleDefinition{uri, "Grouped", "Modulation", "Test module with groups", 0xc4b5fd,
                {{"in", peepo::Direction::Input}, {"out", peepo::Direction::Output}},
                {{"rate", "Rate", "Hz", peepo::ParameterKind::Continuous, 0.1, 10, 1, true},
                 {"voices", "Voices", "", peepo::ParameterKind::Integer, 1, 4, 2},
                 {"shape", "Shape", "", peepo::ParameterKind::Enumeration, 0, 2, 0, false, {"Sine", "Square", "Random"}},
                 {"depth", "Depth", "", peepo::ParameterKind::Continuous, 0, 1, 0.5},
                 {"spare", "Spare", "", peepo::ParameterKind::Continuous, 0, 1, 0}}};
            grouped.groups = {{"Motion", {"rate", "voices", "shape"}}, {"Amount", {"depth"}}};
            peepo::register_definitions({grouped});
            peepo::SimulatedEngine sim;
            sim.submit(peepo::LoadGraph{peepo::empty_graph("Groups")}); sim.advance(100);
            sim.submit(peepo::AddModule{uri, 184, 132}); sim.advance(100);
            peepo::Application app(sim); step(app);
            const auto id = app.graph().modules.back().id;
            const auto value = [&](const char* symbol) {
                const auto* m = peepo::find_module(app.graph(), id);
                return std::find_if(m->parameters.begin(), m->parameters.end(), [&](const peepo::Parameter& p) { return p.symbol == symbol; })->value;
            };
            click(app, 184 + 48, 132 + 72 + 48);
            check(app.page() == peepo::Page::Parameters && exact_label(app.screen(), "MOTION") && exact_label(app.screen(), "AMOUNT") && exact_label(app.screen(), "MORE") &&
                  exact_label(app.screen(), "RATE") && exact_label(app.screen(), "DEPTH") && exact_label(app.screen(), "SPARE"), "the groups are not headings on one page");
            const auto top = [&](const char* name) { lv_area_t a; lv_obj_get_coords(bar(app, name), &a); return a.y1; };
            check(top("RATE") < top("DEPTH") && top("DEPTH") < top("SPARE"), "the bars are not in group order");
            check(bar_value(app, "SHAPE") == "SINE", "the mode is not shown");
            capture(app, output / "51-grouped-parameters.bmp");
            // Encoder 1 moves in the same order: Rate, Voices, Shape, Depth.
            app.control({peepo::ControlEvent::Type::Turn, "enc1", 1}); step(app, 256);
            app.control({peepo::ControlEvent::Type::Turn, "enc2", 1}); step(app, 256);
            check(value("voices") == 3, "encoder 2 did not step the whole number up by one");
            app.control({peepo::ControlEvent::Type::Turn, "enc2", 5}); step(app, 256);
            check(value("voices") == 4, "the whole number went past its top");
            tap_bar(app, "SHAPE"); check(value("shape") == 1 && bar_value(app, "SHAPE") == "SQUARE", "a tap did not step the mode");
            app.control({peepo::ControlEvent::Type::Turn, "enc2", -1}); step(app, 256);
            check(value("shape") == 0, "encoder 2 did not turn the tapped mode back");
            app.control({peepo::ControlEvent::Type::Turn, "enc1", 1}); step(app, 256);
            app.control({peepo::ControlEvent::Type::Turn, "enc2", 10}); step(app, 256);
            check(value("depth") > 0.5 && value("spare") == 0, "encoder 1 did not move on from Shape to Depth");
        }
        // The patch tempo and a module's file. The footer's BPM opens the Tempo
        // page; steps, the TAP pad, encoder 1 and a tap-tempo footswitch set it,
        // and a module that follows it takes it. A model is chosen from the files.
        {
            const std::string uri = "urn:peepo:test:model-player";
            auto player = peepo::ModuleDefinition{uri, "Model player", "Amp", "Test module with a file and a tempo", 0xf0aa72,
                {{"in", peepo::Direction::Input}, {"out", peepo::Direction::Output}, {"control", peepo::Direction::Input, 1, peepo::PortType::Midi}},
                {{"rate", "Rate", "BPM", peepo::ParameterKind::Continuous, 40, 240, 100}, {"level", "Level", "dB", peepo::ParameterKind::Continuous, -20, 20, 0}}};
            player.tempo_symbol = "rate";
            player.file = {"urn:peepo:test:model-player#model", "Model", "nam", {"nam"}};
            peepo::register_definitions({player});
            const auto base = std::filesystem::temp_directory_path() / "peepo-ui-tempo";
            std::filesystem::remove_all(base);
            peepo::PatchStore store(base / "patches");
            peepo::FileStore files(base / "files");
            check(store.open().empty() && files.open().empty(), "tempo test stores did not open");
            peepo::SimulatedEngine sim;
            sim.submit(peepo::LoadGraph{peepo::empty_graph("Tempo")}); sim.advance(100);
            sim.submit(peepo::AddModule{uri, 184, 132}); sim.advance(100);
            peepo::Application app(sim, &store); app.set_files(&files); step(app);
            const auto id = app.graph().modules.back().id;
            const auto rate = [&] { return peepo::find_module(app.graph(), id)->parameters[0].value; };
            check(app.graph().tempo == 120 && rate() == 120, "an added module did not follow the default tempo");
            click(app, 660, 446);
            check(app.page() == peepo::Page::Tempo && exact_label(app.screen(), "120") && text_label(app.screen(), "Model player"), "the footer BPM did not open the Tempo page");
            tap(app, "+5"); check(app.graph().tempo == 125 && rate() == 125, "+5 did not reach the tempo and its follower");
            tap(app, "-1"); check(app.graph().tempo == 124, "-1 did not step the tempo down");
            // Taps half a second apart are 120 BPM.
            tap(app, "TAP"); step(app, 264); tap(app, "TAP");
            // The taps are about half a second apart, give or take the frames a
            // click and a rebuild take; the arithmetic is checked in controls_tests.
            check(std::abs(app.graph().tempo - 120) < 15 && app.graph().tempo != 124 && rate() == app.graph().tempo, "the TAP pad did not set about 120 BPM");
            capture(app, output / "52-tempo.bmp");
            const auto tapped = app.graph().tempo;
            app.control({peepo::ControlEvent::Type::Turn, "enc1", 3}); step(app, 256);
            check(std::abs(app.graph().tempo - (tapped + 3)) < 1e-9, "encoder 1 did not turn the tempo");
            // A footswitch set to tap the tempo, in every patch.
            check(store.set_global_control("stomp3", "tap-tempo").empty(), "tap-tempo footswitch not stored");
            // After a pause longer than the slowest tempo, the taps start again.
            click(app, 70, 456); step(app, 2100);
            app.control({peepo::ControlEvent::Type::Press, "stomp3"}); step(app, 64);
            app.control({peepo::ControlEvent::Type::Release, "stomp3"}); step(app, 336);
            app.control({peepo::ControlEvent::Type::Press, "stomp3"}); step(app, 64);
            app.control({peepo::ControlEvent::Type::Release, "stomp3"}); step(app, 256);
            check(std::abs(app.graph().tempo - 150) < 6 && rate() == app.graph().tempo, "the tap-tempo footswitch did not set about 150 BPM");
            // The model: none at first, then chosen from the files in nam/.
            click(app, 184 + 48, 132 + 72 + 48);
            check(app.page() == peepo::Page::Parameters && exact_label(app.screen(), "MODEL") && exact_label(app.screen(), "None chosen") && exact_label(app.screen(), "RATE"),
                  "the file is not the first bar");
            tap_bar(app, "MODEL");
            check(app.page() == peepo::Page::FilePicker && text_label(app.screen(), "No .nam files yet"), "an empty folder was not explained");
            click(app, 70, 456);
            for (const char* name : {"Clean.nam", "Crunch Lead.nam", "readme.txt"}) std::ofstream(base / "files" / "nam" / name) << "x";
            tap_bar(app, "MODEL");
            check(app.page() == peepo::Page::FilePicker && exact_label(app.screen(), "Clean.nam") && !exact_label(app.screen(), "readme.txt") && text_label(app.screen(), "2 files"),
                  "the models were not listed");
            capture(app, output / "53-file-picker.bmp");
            tap(app, "Crunch Lead.nam");
            const auto chosen = (base / "files").generic_string() + "/nam/Crunch Lead.nam";
            check(app.page() == peepo::Page::Parameters && peepo::find_module(app.graph(), id)->file == chosen && exact_label(app.screen(), "Crunch Lead.nam"),
                  "choosing a model did not give it to the module");
            capture(app, output / "54-file-tile.bmp");
            std::filesystem::remove_all(base);
        }
        // A module with no output control of its own shows the general Output, in dB, on its page.
        {
            const std::string uri = "urn:peepo:test:chorus";
            peepo::register_definitions({{uri, "Chorus", "Modulation", "Test chorus", 0xc4b5fd,
                {{"in", peepo::Direction::Input}, {"out", peepo::Direction::Output}},
                {{"depth", "Depth", "", peepo::ParameterKind::Continuous, 0, 1, 0.5}, peepo::general_output_parameter()}}});
            peepo::SimulatedEngine sim;
            sim.submit(peepo::LoadGraph{peepo::empty_graph("Output")}); sim.advance(100);
            sim.submit(peepo::AddModule{uri, 184, 132}); sim.advance(100);
            peepo::Application app(sim); step(app);
            const auto chorus = app.graph().modules.back();
            click(app, chorus.x + 48, chorus.y + 72 + 48);
            check(app.page() == peepo::Page::Parameters && text_label(app.screen(), "OUTPUT") && text_label(app.screen(), "0 dB"), "general Output missing from the module page");
            capture(app, output / "44-general-output.bmp");
        }
        // Stomps on the patch, as on Beebo: hold a module and press a stomp, and
        // the stomp switches that module's bypass beside any others; a numbered
        // dot marks it on the canvas. Two set opposite ways swap places. A
        // Footswitch module hears its stomp and turns it into a control signal.
        {
            using Type = peepo::ControlEvent::Type;
            const std::string footswitch = "urn:peepo:lv2:footswitch";
            peepo::register_definitions({{footswitch, "Foot switch", "Control", "Test footswitch", 0xe3c770,
                {{"out", peepo::Direction::Output, 1, peepo::PortType::Cv}},
                {{"stomp", "Stomp", "", peepo::ParameterKind::Enumeration, 0, 2, 1, false, {"1", "2", "3"}},
                 {"latching", "Latching", "", peepo::ParameterKind::Toggle, 0, 1, 0},
                 {"on_value", "On value", "", peepo::ParameterKind::Continuous, 0, 1, 1},
                 {"off_value", "Off value", "", peepo::ParameterKind::Continuous, 0, 1, 0},
                 {"pressed", "Pressed", "", peepo::ParameterKind::Toggle, 0, 1, 0}}}});
            peepo::SimulatedEngine sim;
            peepo::Application app(sim); step(app);
            auto press = [&](const char* control) { app.control({Type::Press, control}); step(app, 64); };
            auto release = [&](const char* control) { app.control({Type::Release, control}); step(app, 256); };
            auto bypassed = [&](peepo::InstanceId id) { return peepo::find_module(app.graph(), id)->bypassed; };
            hold(app, 236, 244); check(app.page() == peepo::Page::ModuleActions, "holding Drive did not open its actions");
            press("stomp1"); release("stomp1");
            check(app.graph().assignments.size() == 1 && app.graph().assignments[0].module == 3 && app.graph().assignments[0].action == peepo::ControlAction::Bypass &&
                  app.page() == peepo::Page::ModuleActions && !bypassed(3), "a stomp pressed on Drive's actions did not assign its bypass, or played");
            check(text_label(app.screen(), "Stomp 1 switches it") != nullptr, "the actions page does not say which stomp switches the module");
            capture(app, output / "55-stomp-quick-assign.bmp");
            click(app, 70, 456); hold(app, 396, 244);
            press("stomp1"); release("stomp1");
            check(app.graph().assignments.size() == 2 && app.graph().assignments[1].module == 4, "a second module did not join the stomp");
            click(app, 70, 456);
            check(app.page() == peepo::Page::Patch && exact_label(app.screen(), "1") != nullptr, "the patch does not number the stomp on its modules");
            // Clouds starts bypassed, so each press swaps which of the two plays.
            sim.submit(peepo::SetBypass{4, true}); step(app, 256);
            capture(app, output / "56-stomp-badges.bmp");
            press("stomp1");
            check(app.message().find("Drive bypassed, Clouds on") != std::string::npos, "the stomp did not say what it switched");
            release("stomp1");
            check(bypassed(3) && !bypassed(4), "the stomp did not swap Drive and Clouds");
            press("stomp1"); release("stomp1");
            check(!bypassed(3) && bypassed(4), "the stomp did not swap them back");
            // Settings > Controls names both, and Edit explains the swap.
            click(app, 707, 36); tap(app, "CONTROLS");
            check(text_label(app.screen(), "Drive, Clouds / bypass") != nullptr, "Controls does not list both modules of the stomp");
            click(app, 70, 456); click(app, 70, 456);
            // Pressing it again on a module's actions takes that module off.
            hold(app, 236, 244); press("stomp1"); release("stomp1");
            check(app.graph().assignments.size() == 1 && app.graph().assignments[0].module == 4, "pressing again did not take Drive off the stomp");
            click(app, 70, 456);
            // A Footswitch module set to stomp 2 is pressed and released with it.
            sim.submit(peepo::AddModule{footswitch, 600, 264}); step(app, 256);
            const auto id = app.graph().modules.back().id;
            const auto value = [&](const char* symbol) {
                const auto* m = peepo::find_module(app.graph(), id);
                return std::find_if(m->parameters.begin(), m->parameters.end(), [&](const peepo::Parameter& p) { return p.symbol == symbol; })->value;
            };
            check(exact_label(app.screen(), "2") != nullptr, "the Footswitch module does not show its stomp");
            press("stomp2");
            check(app.message().find("Foot switch on") != std::string::npos, "the stomp did not say it pressed the Footswitch module");
            step(app, 192);
            check(value("pressed") == 1, "the Footswitch module did not hear its stomp go down");
            release("stomp2");
            check(value("pressed") == 0, "the Footswitch module did not hear its stomp come up");
            press("stomp3"); release("stomp3");
            check(value("pressed") == 0, "the Footswitch module heard another stomp");
            sim.submit(peepo::SetParameter{id, "latching", 1}); step(app, 256);
            press("stomp2"); release("stomp2");
            check(value("pressed") == 1, "a latching Footswitch did not stay on after a press");
            press("stomp2"); release("stomp2");
            check(value("pressed") == 0, "a latching Footswitch did not go off on the next press");
            capture(app, output / "57-footswitch.bmp");
        }
        // A time in beats: shown as its note and in milliseconds, stepped by note value.
        {
            const std::string uri = "urn:peepo:test:beat-delay";
            auto delay = peepo::ModuleDefinition{uri, "Delay", "Delay", "Test delay timed in beats", 0x7bbaed,
                {{"in", peepo::Direction::Input}, {"out", peepo::Direction::Output}},
                {{"time", "Time", peepo::beats_unit, peepo::ParameterKind::Continuous, 0.001, 16, 0.5},
                 {"bpm", "BPM", "", peepo::ParameterKind::Continuous, 30, 300, 120}}};
            delay.tempo_symbol = "bpm";
            peepo::register_definitions({delay});
            peepo::SimulatedEngine sim;
            sim.submit(peepo::LoadGraph{peepo::empty_graph("Beats")}); sim.advance(100);
            sim.submit(peepo::AddModule{uri, 184, 132}); sim.advance(100);
            peepo::Application app(sim); step(app);
            const auto id = app.graph().modules.back().id;
            const auto time = [&] { return peepo::find_module(app.graph(), id)->parameters[0].value; };
            click(app, 184 + 48, 132 + 72 + 48);
            check(app.page() == peepo::Page::Parameters && exact_label(app.screen(), "1/8") && text_label(app.screen(), "250 ms at 120 BPM"),
                  "a time in beats is not shown as its note and in milliseconds");
            app.control({peepo::ControlEvent::Type::Turn, "enc2", 1}); step(app, 256);
            check(std::abs(time() - 2.0 / 3) < 1e-9 && exact_label(app.screen(), "1/4 trip"), "a detent did not step to the next note value");
            app.control({peepo::ControlEvent::Type::Turn, "enc2", 2}); step(app, 256);
            check(std::abs(time() - 1) < 1e-9 && exact_label(app.screen(), "1/4") && text_label(app.screen(), "500 ms"), "encoder 2 did not turn by note values");
            capture(app, output / "58-beat-time.bmp");
        }
        // Drive the personalized Euclidean page with live step telemetry and 4-column track controls.
        {
            struct EuclideanEngine : peepo::Engine {
                peepo::SimulatedEngine backing;
                peepo::EuclideanSteps steps{{2, 3, 8, 2}};
                std::optional<peepo::InstanceId> watched;
                peepo::RequestId submit(peepo::Command c) override { return backing.submit(std::move(c)); }
                void connect() override { backing.connect(); }
                void disconnect() override { backing.disconnect(); }
                std::vector<peepo::EngineEvent> advance(unsigned ms) override { return backing.advance(ms); }
                peepo::EngineState state() const override { return backing.state(); }
                const peepo::Graph& snapshot() const override { return backing.snapshot(); }
                peepo::EngineCapabilities capabilities() const override { return {false, true, false, false, true}; }
                void watch_euclidean(std::optional<peepo::InstanceId> id) override { watched = id; }
                peepo::EuclideanSteps euclidean_steps(peepo::InstanceId id) const override { return watched == id ? steps : peepo::EuclideanSteps{}; }
            } live;
            const std::string uri = "http://polyeffects.com/lv2/euclidean";
            peepo::register_definitions({{uri, "Euclidean", "CV", "Four Euclidean trigger sequencers", 0xe8d44d,
                {{"trigger", peepo::Direction::Input, 1, peepo::PortType::Cv},
                 {"reset", peepo::Direction::Input, 1, peepo::PortType::Cv},
                 {"out_1", peepo::Direction::Output, 1, peepo::PortType::Cv},
                 {"out_2", peepo::Direction::Output, 1, peepo::PortType::Cv},
                 {"out_3", peepo::Direction::Output, 1, peepo::PortType::Cv},
                 {"out_4", peepo::Direction::Output, 1, peepo::PortType::Cv}},
                {{"is_enabled1", "Loop 1", "", peepo::ParameterKind::Toggle, 0, 1, 1},
                 {"steps1", "Steps 1", "", peepo::ParameterKind::Integer, 1, 32, 8},
                 {"beats1", "Beats 1", "", peepo::ParameterKind::Integer, 0, 32, 3},
                 {"shift1", "Shift 1", "", peepo::ParameterKind::Integer, 0, 32, 0},
                 {"is_enabled2", "Loop 2", "", peepo::ParameterKind::Toggle, 0, 1, 1},
                 {"steps2", "Steps 2", "", peepo::ParameterKind::Integer, 1, 32, 12},
                 {"beats2", "Beats 2", "", peepo::ParameterKind::Integer, 0, 32, 5},
                 {"shift2", "Shift 2", "", peepo::ParameterKind::Integer, 0, 32, 1},
                 {"is_enabled3", "Loop 3", "", peepo::ParameterKind::Toggle, 0, 1, 1},
                 {"steps3", "Steps 3", "", peepo::ParameterKind::Integer, 1, 32, 16},
                 {"beats3", "Beats 3", "", peepo::ParameterKind::Integer, 0, 32, 7},
                 {"shift3", "Shift 3", "", peepo::ParameterKind::Integer, 0, 32, 2},
                 {"is_enabled4", "Loop 4", "", peepo::ParameterKind::Toggle, 0, 1, 1},
                 {"steps4", "Steps 4", "", peepo::ParameterKind::Integer, 1, 32, 6},
                 {"beats4", "Beats 4", "", peepo::ParameterKind::Integer, 0, 32, 2},
                 {"shift4", "Shift 4", "", peepo::ParameterKind::Integer, 0, 32, 0}}}});
            live.submit(peepo::LoadGraph{peepo::empty_graph("Rhythm")}); live.advance(100);
            live.submit(peepo::AddModule{uri, 184, 132}); live.advance(100);
            peepo::Application app(live); step(app);
            const auto euc = app.graph().modules.back();
            click(app, euc.x + 16, euc.y + 72 + 28);
            check(app.page() == peepo::Page::Parameters && live.watched == euc.id && exact_label(app.screen(), "RHYTHM") &&
                  exact_label(app.screen(), "LIVE") && exact_label(app.screen(), "L1 3/8") && exact_label(app.screen(), "L2 4/12") &&
                  exact_label(app.screen(), "L3 9/16") && exact_label(app.screen(), "L4 3/6"), "Euclidean did not show live steps and 4 rings");
            capture(app, output / "59-euclidean-live.bmp");
            auto* loop1 = bar(app, "LOOP 1");
            live.steps = {{0, 1, 2, 3}}; step(app);
            check(exact_label(app.screen(), "L1 1/8") && bar(app, "LOOP 1") == loop1, "Euclidean live step update rebuilt controls");
            // Step Encoder 1 from Loop 1 to Steps 1 and turn Encoder 2 up by 2.
            app.control({peepo::ControlEvent::Type::Turn, "enc1", 1}); step(app, 256);
            app.control({peepo::ControlEvent::Type::Turn, "enc2", 2}); step(app, 256);
            check(exact_label(app.screen(), "L1 1/10"), "turning Steps 1 did not update ring and readout to 10 steps");
            // Toggle Loop 4 off by tapping its tile.
            tap_bar(app, "LOOP 4");
            check(exact_label(app.screen(), "L4 OFF"), "toggling Loop 4 off did not mark L4 OFF");
            live.steps = {}; step(app);
            check(exact_label(app.screen(), "WAITING FOR TRIG") && exact_label(app.screen(), "L1 3:10"), "idle Euclidean did not show beats:steps ratio");
            click(app, 70, 456);
            check(!live.watched, "leaving Euclidean did not stop monitoring");
        }
        // Hold a module and then hold another while still holding the first: direct connect or port selection screen.
        {
            static lv_point_t pointer2{};
            static bool pressed2 = false;
            auto* indev2 = lv_indev_create();
            lv_indev_set_type(indev2, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(indev2, [](lv_indev_t*, lv_indev_data_t* d) {
                d->point = pointer2; d->state = pressed2 ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            });
            lv_indev_set_display(indev2, display);
            peepo::ModuleDefinition gain_def{"urn:peepo:test:gain", "Mono utility", "Utility", "Gain", 0x9aa4b2,
                {{"in", peepo::Direction::Input}, {"out", peepo::Direction::Output}}, {}};
            peepo::ModuleDefinition twist_def{"urn:peepo:test:twist", "Twist Wavefolder", "Distortion", "Wavefolder", 0xe57373,
                {{"in", peepo::Direction::Input}, {"sym_mod", peepo::Direction::Input},
                 {"out", peepo::Direction::Output}, {"aux", peepo::Direction::Output}}, {}};
            peepo::register_definitions({gain_def, twist_def});
            peepo::SimulatedEngine hold_engine;
            auto g = peepo::empty_graph("Hold connect");
            g.connections.clear();
            g.modules.push_back({3, gain_def.uri, gain_def.name, 184, 72, false, false, gain_def.parameters, gain_def.ports});
            g.modules.push_back({4, twist_def.uri, twist_def.name, 344, 72, false, false, twist_def.parameters, twist_def.ports});
            g.modules.push_back({5, twist_def.uri, twist_def.name, 504, 72, false, false, twist_def.parameters, twist_def.ports});
            g.modules.push_back({6, gain_def.uri, gain_def.name, 184, 216, false, false, gain_def.parameters, gain_def.ports});
            hold_engine.submit(peepo::LoadGraph{g}); hold_engine.advance(100);
            peepo::Application app(hold_engine); step(app);
            const auto has_conn = [&](const peepo::Connection& c) {
                const auto& list = app.graph().connections;
                return std::find(list.begin(), list.end(), c) != list.end();
            };
            // 1) Multi-pointer hold: hold Mono utility #3 (1 output) and then hold Mono utility #6 (1 input) -> connects immediately.
            pointer = {232, 164}; pressed = true; step(app, 480);
            pointer2 = {232, 308}; pressed2 = true; step(app, 480);
            pressed = false; pressed2 = false; step(app, 256);
            check(app.page() == peepo::Page::Patch && has_conn({3, 6, "out", "in"}), "holding 1-output then 1-input module did not connect immediately");
            // 2) Single-pointer hold-then-hold: hold Mono utility #3 (1 output) and then hold Twist Wavefolder #4 (2 audio inputs) -> opens port screen for inputs.
            pointer = {232, 164}; pressed = true; step(app, 480);
            pointer = {392, 164}; step(app, 480);
            check(app.page() == peepo::Page::CvConnect && text_label(app.screen(), "which input of Twist") != nullptr &&
                  exact_label(app.screen(), "IN") != nullptr && exact_label(app.screen(), "SYM_MOD") != nullptr,
                  "holding 1-output then 2-input module did not open input selection screen");
            pressed = false; step(app);
            check(app.page() == peepo::Page::CvConnect, "releasing hold dismissed port selection screen prematurely");
            click(app, 592, 160); step(app, 256);
            check(app.page() == peepo::Page::Patch && has_conn({3, 4, "out", "sym_mod"}), "choosing second input did not connect to sym_mod");
            // 3) Hold Twist Wavefolder #4 (2 outputs: out, aux) and then hold Mono utility #6 (1 input: in) -> asks for output, then connects immediately.
            pointer = {392, 164}; pressed = true; step(app, 480);
            pointer2 = {232, 308}; pressed2 = true; step(app, 480);
            pressed = false; pressed2 = false; step(app);
            check(app.page() == peepo::Page::CvConnect && text_label(app.screen(), "Which output of Twist Wavefolder?") != nullptr &&
                  exact_label(app.screen(), "OUT") != nullptr && exact_label(app.screen(), "AUX") != nullptr,
                  "holding 2-output then 1-input module did not open output selection screen");
            click(app, 592, 160); step(app, 256);
            check(app.page() == peepo::Page::Patch && has_conn({4, 6, "aux", "in"}), "choosing output when target has 1 input did not connect immediately");
            // 4) Hold Twist Wavefolder #4 (2 outputs) and then hold Twist Wavefolder #5 (2 inputs) -> asks for output, then asks for input.
            pointer = {392, 164}; pressed = true; step(app, 480);
            pointer2 = {552, 164}; pressed2 = true; step(app, 480);
            pressed = false; pressed2 = false; step(app);
            check(app.page() == peepo::Page::CvConnect && text_label(app.screen(), "Which output of Twist Wavefolder?") != nullptr,
                  "holding 2-output then 2-input module did not ask for output first");
            click(app, 592, 160);
            check(app.page() == peepo::Page::CvConnect && text_label(app.screen(), "which input of Twist") != nullptr,
                  "choosing output did not advance to input selection when target has 2 inputs");
            click(app, 592, 160); step(app, 256);
            check(app.page() == peepo::Page::Patch && has_conn({4, 5, "aux", "sym_mod"}),
                  "choosing input did not connect the two multi-port modules");
            lv_indev_delete(indev2);
        }
        // Release the headless devices before the global runtime.
        lv_indev_delete(indev);
        lv_indev_delete(keypad);
        lv_display_delete(display);
        lv_deinit();
        std::cout << "Real LVGL pointer workflows and 800x480 captures passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
