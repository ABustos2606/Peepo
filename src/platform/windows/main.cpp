// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "ui/application.hpp"
#include "catalogue/catalogue.hpp"
#include "engine/ingen/ingen_engine.hpp"
#include "engine/simulated_engine.hpp"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {
// Keys stand in for the physical controls until the Pi's GPIO adapter exists.
// They reach the window on the LVGL driver's thread; the events are queued for
// the interface thread. The keys still go on to LVGL, which uses them as text
// on the name page; the interface ignores controls there.
std::mutex control_mutex;
std::vector<peepo::ControlEvent> control_events;
WNDPROC original_procedure = nullptr;
LRESULT CALLBACK control_keys(HWND window, UINT message, WPARAM key, LPARAM details) {
    if (message == WM_KEYDOWN || message == WM_KEYUP) {
        const bool down = message == WM_KEYDOWN, repeat = down && (details & (1 << 30));
        std::vector<peepo::ControlEvent> found;
        static const std::pair<char, std::pair<int, int>> turns[] = {
            // Upper row clockwise, the key below it anticlockwise: Q/A, W/S.
            {'Q', {1, 1}}, {'A', {1, -1}}, {'W', {2, 1}}, {'S', {2, -1}}};
        for (const auto& [letter, turn] : turns)
            if (down && key == static_cast<WPARAM>(letter)) found.push_back({peepo::ControlEvent::Type::Turn, "enc" + std::to_string(turn.first), turn.second});
        auto press = [&](const std::string& control) {
            if (!repeat) found.push_back({down ? peepo::ControlEvent::Type::Press : peepo::ControlEvent::Type::Release, control, 0});
        };
        if (key == '1' || key == '2') press("enc" + std::to_string(key - '0'));
        if (key == 'Z') press("stomp1");
        if (key == 'X') press("stomp2");
        if (key == 'C') press("stomp3");
        if (key == 'V') press("stomp4");
        // Two stomps at once: B is A+B, N is B+C, M is C+D.
        if (key == 'B') { press("stomp1"); press("stomp2"); }
        if (key == 'N') { press("stomp2"); press("stomp3"); }
        if (key == 'M') { press("stomp3"); press("stomp4"); }
        if (!found.empty()) {
            const std::lock_guard lock(control_mutex);
            control_events.insert(control_events.end(), found.begin(), found.end());
        }
    }
    return CallWindowProcW(original_procedure, window, message, key, details);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command_line, int) {
    bool smoke = command_line && wcscmp(command_line, L"--smoke") == 0;
    // --engine tcp://host:port drives a running Ingen instead of the simulation,
    // for example one started under WSL, which Windows reaches on 127.0.0.1.
    // --catalogue file.json names the modules on offer; by default the
    // exported catalogue.json beside this program, with its overlay. The
    // simulation offers them too, beside its own demo modules.
    // --patches DIR keeps saved patches there (by default %LOCALAPPDATA%/Peepo/patches,
    // or simulator-patches for the simulation). --engine-patches PATH is the same
    // folder as the engine sees it; for an engine in WSL that is /mnt/c/... by default.
    std::string address, catalogue_path, overlay_path, patches_path, engine_patches;
    auto utf8 = [](const wchar_t* wide) {
        std::string text(static_cast<std::size_t>(WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr)), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, text.data(), static_cast<int>(text.size()), nullptr, nullptr);
        text.resize(text.empty() ? 0 : text.size() - 1);
        return text;
    };
    int count = 0;
    if (auto* arguments = CommandLineToArgvW(GetCommandLineW(), &count)) {
        for (int i = 1; i < count; ++i) smoke = smoke || wcscmp(arguments[i], L"--smoke") == 0;
        for (int i = 1; i + 1 < count; ++i) {
            if (wcscmp(arguments[i], L"--engine") == 0) address = utf8(arguments[i + 1]);
            if (wcscmp(arguments[i], L"--catalogue") == 0) catalogue_path = utf8(arguments[i + 1]);
            if (wcscmp(arguments[i], L"--patches") == 0) patches_path = utf8(arguments[i + 1]);
            if (wcscmp(arguments[i], L"--engine-patches") == 0) engine_patches = utf8(arguments[i + 1]);
        }
        LocalFree(arguments);
    }
    const bool named = !catalogue_path.empty();
    if (!named) {
        wchar_t program[MAX_PATH]{};
        GetModuleFileNameW(nullptr, program, MAX_PATH);
        auto folder = utf8(program);
        folder.resize(folder.find_last_of("\\/") + 1);
        catalogue_path = folder + "catalogue.json";
        overlay_path = folder + "catalogue-overlay.json";
    }
    std::vector<peepo::ModuleDefinition> offered;
    const auto catalogue_issue = peepo::load_catalogue(catalogue_path, overlay_path, offered);
    if (!address.empty()) {
        // Without a readable catalogue the engine's graph is still shown; its
        // modules are placeholders and nothing can be added.
        peepo::replace_catalogue(std::move(offered));
    } else if (catalogue_issue.empty()) {
        // The simulation keeps its demo modules: its sample patches and custom
        // pages are built around them. The exported entries take the place of
        // the built-in reference ones, which describe the same plugins.
        std::vector<peepo::ModuleDefinition> both;
        for (const auto& d : peepo::demo_catalogue()) if (d.category != "Endpoint") both.push_back(d);
        both.insert(both.end(), offered.begin(), offered.end());
        peepo::replace_catalogue(std::move(both));
    }
    // A simulation without the default file simply keeps its built-in list.
    const bool expected = !address.empty() || named || std::filesystem::exists(std::filesystem::path(std::u8string(catalogue_path.begin(), catalogue_path.end())));
    if (!catalogue_issue.empty() && expected && !smoke)
        MessageBoxW(nullptr, std::wstring(catalogue_issue.begin(), catalogue_issue.end()).c_str(), L"Peepo - catalogue", MB_OK | MB_ICONWARNING);
    lv_init();
    auto* display = lv_windows_create_display(address.empty() ? L"Peepo - UI simulator" : L"Peepo - Ingen engine", 800, 480, 100, true, true);
    if (!display) return 1;
    auto* group = lv_group_create(); lv_group_set_default(group);
    lv_windows_acquire_pointer_indev(display);
    lv_indev_set_group(lv_windows_acquire_keypad_indev(display), group);
    std::unique_ptr<peepo::Engine> owned;
    if (address.empty()) owned = std::make_unique<peepo::SimulatedEngine>();
    else {
        owned = std::make_unique<peepo::IngenEngine>(peepo::ingen::Options{address});
        owned->connect();
    }
    auto& engine = *owned;
    // Saved patches. The smoke check runs without them, touching no user folder.
    std::unique_ptr<peepo::PatchStore> store;
    if (!smoke) {
        std::filesystem::path root;
        if (!patches_path.empty()) root = std::filesystem::path(std::u8string(patches_path.begin(), patches_path.end()));
        else if (const auto* local = _wgetenv(L"LOCALAPPDATA")) root = std::filesystem::path(local) / L"Peepo" / (address.empty() ? L"simulator-patches" : L"patches");
        if (!root.empty()) {
            // The engine in WSL reaches this Windows folder through /mnt/<drive>.
            store = std::make_unique<peepo::PatchStore>(root, !engine_patches.empty() ? engine_patches : address.empty() ? std::string() : peepo::wsl_path(root));
            if (const auto issue = store->open(); !issue.empty())
                MessageBoxW(nullptr, std::wstring(issue.begin(), issue.end()).c_str(), L"Peepo - patches", MB_OK | MB_ICONWARNING);
        }
    }
    // Models and impulse responses beside the patches; the engine in WSL sees
    // them through /mnt/<drive> as well.
    std::unique_ptr<peepo::FileStore> files;
    if (store) {
        const auto root = store->root().parent_path() / L"files";
        files = std::make_unique<peepo::FileStore>(root, address.empty() ? std::string() : peepo::wsl_path(root));
        files->open();
    }
    // MIDI settings beside the patches. This computer runs no peepo-midi, so
    // no devices are listed; the instrument lists its own.
    std::unique_ptr<peepo::midi::Link> midi;
    if (store) midi = std::make_unique<peepo::midi::Link>(store->root().parent_path() / (address.empty() ? L"simulator-midi.conf" : L"midi.conf"), std::filesystem::path());
    lv_lock();
    bool display_alive = true;
    unsigned completed_flushes = 0;
    lv_display_add_event_cb(display, [](lv_event_t* e) {
        *static_cast<bool*>(lv_event_get_user_data(e)) = false;
    }, LV_EVENT_DELETE, &display_alive);
    lv_display_add_event_cb(display, [](lv_event_t* e) {
        ++*static_cast<unsigned*>(lv_event_get_user_data(e));
    }, LV_EVENT_FLUSH_FINISH, &completed_flushes);
    peepo::Application app(engine, store.get());
    app.set_files(files.get());
    app.set_midi(midi.get());
    lv_unlock();
    const auto window = lv_windows_get_display_window_handle(display);
    original_procedure = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(control_keys)));
    if (smoke) ShowWindow(window, SW_HIDE);
    const auto started = GetTickCount64();
    auto last = GetTickCount64();
    bool framebuffer_ready = false;
    while (true) {
        const auto now = GetTickCount64();
        lv_lock();
        if (!display_alive || !IsWindow(window)) { lv_unlock(); break; }
        if (!framebuffer_ready) {
            // The Win32 driver provisions its buffer on a deferred timer.
            // An earlier refresh can pause without drawing; the queued full
            // invalidation then prevents another refresh request. Resume it
            // explicitly once the buffer exists, without requiring a click.
            auto* buffer = lv_display_get_buf_active(display);
            if (buffer && buffer->data && buffer->data_size) {
                lv_obj_invalidate(app.screen());
                auto* refresh = lv_display_get_refr_timer(display);
                lv_timer_resume(refresh);
                lv_timer_ready(refresh);
                framebuffer_ready = true;
            }
        }
        {
            std::vector<peepo::ControlEvent> events;
            { const std::lock_guard guard(control_mutex); events.swap(control_events); }
            for (const auto& event : events) app.control(event);
        }
        app.tick(static_cast<unsigned>(now - last));
        lv_unlock();
        last = now;
        const auto wait = lv_timer_handler();
        Sleep(std::clamp<unsigned>(wait, 1, 10));
        if (smoke && now - started > 1000) {
            lv_lock();
            if (!display_alive) { lv_unlock(); break; }
            auto* buffer = lv_display_get_buf_active(display);
            unsigned dark = 0, cards = 0;
            if (buffer && buffer->data) {
                for (unsigned y = 0; y < buffer->header.h; ++y) {
                    const auto* row = reinterpret_cast<const std::uint32_t*>(buffer->data + y * buffer->header.stride);
                    for (unsigned x = 0; x < buffer->header.w; ++x) {
                        const auto pixel = row[x] & 0xffffff;
                        if (pixel == 0x000000) ++dark;
                        if (pixel == 0x0f0f0f) ++cards;
                    }
                }
            }
            const bool rendered = buffer && buffer->header.w == 800 && buffer->header.h == 480
                && completed_flushes > 0 && dark > 100000 && cards > 10000;
            std::ofstream log("windows-render-check.txt");
            log << "resolution=" << lv_display_get_horizontal_resolution(display) << "x" << lv_display_get_vertical_resolution(display)
                << " buffer=" << (buffer != nullptr) << " flushes=" << completed_flushes
                << " background_pixels=" << dark << " card_pixels=" << cards << "\n";
            log.close();
            if (rendered) {
                // Capture the real Win32 display buffer, not an LVGL snapshot.
                std::ofstream bitmap("windows-render-check.bmp", std::ios::binary);
                auto word = [&](std::uint32_t value, unsigned bytes) {
                    for (unsigned i = 0; i < bytes; ++i) bitmap.put(static_cast<char>((value >> (i * 8)) & 255));
                };
                bitmap.put('B'); bitmap.put('M'); word(54 + 800 * 480 * 4, 4); word(0, 4); word(54, 4);
                word(40, 4); word(800, 4); word(480, 4); word(1, 2); word(32, 2);
                word(0, 4); word(800 * 480 * 4, 4); word(2835, 4); word(2835, 4); word(0, 4); word(0, 4);
                for (int y = 479; y >= 0; --y)
                    bitmap.write(reinterpret_cast<const char*>(buffer->data + y * buffer->header.stride), 800 * 4);
            }
            lv_unlock();
            if (!rendered) ExitProcess(2);
            PostMessageW(window, WM_CLOSE, 0, 0);
        }
    }
    // The Windows driver deletes its display (and screen) on window close.
    // Exit the process without touching objects owned by that deleted display.
    ExitProcess(0);
}
