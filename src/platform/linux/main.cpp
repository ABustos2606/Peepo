// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
//
// The interface on Linux: the Pi's screen through DRM/KMS or the framebuffer
// with evdev touch, an SDL window on a desktop or under WSL, or an offscreen
// display for checks. The physical controls come from the kernel's input
// devices; under SDL, keys stand in for them as on Windows.
#include "catalogue/catalogue.hpp"
#include "engine/ingen/ingen_engine.hpp"
#include "engine/simulated_engine.hpp"
#include "platform/linux/evdev_controls.hpp"
#include "platform/linux/network_status.hpp"
#include "platform/linux/system_status.hpp"
#include "ui/application.hpp"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unistd.h>
#ifdef PEEPO_LVGL_DRM
#include <fcntl.h>
#include <xf86drm.h>
#endif
#ifdef PEEPO_LVGL_SDL
#include <SDL2/SDL.h>
#endif

namespace {
volatile std::sig_atomic_t stopping = 0;
std::uint32_t now_ms() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<std::uint32_t>(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}
std::filesystem::path data_home() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) return xdg;
    if (const char* home = std::getenv("HOME"); home && *home) return std::filesystem::path(home) / ".local" / "share";
    return "/var/lib";
}
void usage() {
    std::cout <<
        "peepo [options]\n"
        "  --engine ADDRESS        unix:///run/peepo/ingen.sock or tcp://host:port; without it, the simulation\n"
        "  --display NAME          drm, fbdev, sdl or offscreen (default: sdl with a desktop, else drm, else fbdev)\n"
        "  --drm-device PATH       DRM card (default: the first with a connected display)\n"
        "  --fbdev PATH            framebuffer device (default /dev/fb0)\n"
        "  --touch PATH|none       touch screen input device (default: the first that reports touches)\n"
        "  --rotate 0|90|180|270   rotate the display\n"
        "  --controls FILE         physical control bindings (default /etc/peepo/controls.conf if present)\n"
        "  --patches DIR           saved patches; --engine-patches PATH if the engine sees them elsewhere\n"
        "  --files DIR             models and impulse responses to offer (default beside the patches)\n"
        "  --midi FILE             MIDI settings (default midi.conf beside the patches)\n"
        "  --midi-status DIR       where peepo-midi says what it hears, such as /run/peepo-midi\n"
        "  --catalogue FILE        modules on offer (default catalogue.json beside the program)\n"
        "  --smoke                 run one second, check that something was drawn, and exit\n"
        "  --screenshot FILE       with --smoke and --display offscreen: save what was drawn as BMP\n";
}
#ifdef PEEPO_LVGL_DRM
// The interface is laid out for 800 x 480. Small HDMI panels often prefer
// another mode while also offering that one, so take it when it is there
// (at its highest refresh), and the panel's own preference otherwise.
std::size_t pick_mode(lv_display_t*, const lv_linux_drm_mode_t* modes, std::size_t count) {
    std::size_t chosen = count;
    for (std::size_t i = 0; i < count; ++i)
        if (modes[i].hdisplay == 800 && modes[i].vdisplay == 480 && (chosen == count || modes[i].vrefresh > modes[chosen].vrefresh)) chosen = i;
    if (chosen != count) return chosen;
    for (std::size_t i = 0; i < count; ++i) if (modes[i].type & DRM_MODE_TYPE_PREFERRED) return i;
    return 0;
}
// Only one program at a time may draw on a DRM card. If another still holds
// it (the boot splash, say), LVGL would wait forever for a frame that is
// never shown, so wait here until the card is free.
bool wait_for_screen(const std::string& card) {
    for (bool told = false; !stopping; usleep(100000)) {
        const int fd = open(card.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) return false;
        const bool free = drmIsMaster(fd);
        close(fd);
        if (free) return true;
        if (!told) std::cerr << "waiting for " << card << ": another program is drawing on it\n";
        told = true;
    }
    return false;
}
#endif
#ifdef PEEPO_LVGL_SDL
// Keys standing in for the controls, as in the Windows build.
int sdl_keys(void* events, SDL_Event* e) {
    if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return 1;
    auto& out = *static_cast<std::vector<peepo::ControlEvent>*>(events);
    const bool down = e->type == SDL_KEYDOWN, repeat = e->key.repeat != 0;
    const auto key = e->key.keysym.sym;
    static const std::pair<SDL_Keycode, std::pair<int, int>> turns[] = {
        {SDLK_q, {1, 1}}, {SDLK_a, {1, -1}}, {SDLK_w, {2, 1}}, {SDLK_s, {2, -1}}, {SDLK_e, {3, 1}}, {SDLK_d, {3, -1}}, {SDLK_r, {4, 1}}, {SDLK_f, {4, -1}}};
    for (const auto& [code, turn] : turns)
        if (down && key == code) out.push_back({peepo::ControlEvent::Type::Turn, "enc" + std::to_string(turn.first), turn.second});
    auto press = [&](const std::string& control) {
        if (!repeat) out.push_back({down ? peepo::ControlEvent::Type::Press : peepo::ControlEvent::Type::Release, control, 0});
    };
    if (key >= SDLK_1 && key <= SDLK_4) press("enc" + std::to_string(key - SDLK_0));
    if (key == SDLK_z) press("stomp1");
    if (key == SDLK_x) press("stomp2");
    if (key == SDLK_c) press("stomp3");
    return 1;
}
#endif
void write_bmp(lv_display_t* display, const std::string& path) {
    auto* buffer = lv_display_get_buf_active(display);
    if (!buffer || !buffer->data) return;
    std::ofstream bitmap(path, std::ios::binary);
    auto word = [&](std::uint32_t value, unsigned bytes) { for (unsigned i = 0; i < bytes; ++i) bitmap.put(static_cast<char>((value >> (i * 8)) & 255)); };
    const auto w = buffer->header.w, h = buffer->header.h;
    bitmap.put('B'); bitmap.put('M'); word(54 + w * h * 4, 4); word(0, 4); word(54, 4);
    word(40, 4); word(w, 4); word(h, 4); word(1, 2); word(32, 2);
    word(0, 4); word(w * h * 4, 4); word(2835, 4); word(2835, 4); word(0, 4); word(0, 4);
    for (int y = static_cast<int>(h) - 1; y >= 0; --y) bitmap.write(reinterpret_cast<const char*>(buffer->data + y * buffer->header.stride), w * 4);
}
}

int main(int argc, char** argv) {
    std::string address, catalogue_path, overlay_path, patches_path, engine_patches, files_path, display_name, drm_device, fbdev_path = "/dev/fb0",
        touch_path, controls_path, screenshot, midi_path, midi_status;
    int rotation = 0;
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << option << " needs a value\n"; std::exit(2); }
            return argv[++i];
        };
        if (option == "--engine") address = value();
        else if (option == "--catalogue") catalogue_path = value();
        else if (option == "--patches") patches_path = value();
        else if (option == "--engine-patches") engine_patches = value();
        else if (option == "--files") files_path = value();
        else if (option == "--midi") midi_path = value();
        else if (option == "--midi-status") midi_status = value();
        else if (option == "--display") display_name = value();
        else if (option == "--drm-device") drm_device = value();
        else if (option == "--fbdev") fbdev_path = value();
        else if (option == "--touch") touch_path = value();
        else if (option == "--rotate") rotation = std::atoi(value().c_str());
        else if (option == "--controls") controls_path = value();
        else if (option == "--screenshot") screenshot = value();
        else if (option == "--smoke") smoke = true;
        else if (option == "--help" || option == "-h") { usage(); return 0; }
        else { std::cerr << "unknown option " << option << "\n"; usage(); return 2; }
    }
    std::signal(SIGTERM, [](int) { stopping = 1; });
    std::signal(SIGINT, [](int) { stopping = 1; });

    // The modules on offer, as on Windows: an exported catalogue and its overlay beside the program.
    if (catalogue_path.empty()) {
        const auto folder = std::filesystem::read_symlink("/proc/self/exe").parent_path();
        catalogue_path = (folder / "catalogue.json").string();
        overlay_path = (folder / "catalogue-overlay.json").string();
        // An installed program finds them in its share directory.
        if (!std::filesystem::exists(catalogue_path) && std::filesystem::exists(folder / ".." / "share" / "peepo" / "catalogue.json")) {
            catalogue_path = (folder / ".." / "share" / "peepo" / "catalogue.json").string();
            overlay_path = (folder / ".." / "share" / "peepo" / "catalogue-overlay.json").string();
        }
    }
    std::vector<peepo::ModuleDefinition> offered;
    const auto catalogue_issue = peepo::load_catalogue(catalogue_path, overlay_path, offered);
    if (!address.empty()) peepo::replace_catalogue(std::move(offered));
    else if (catalogue_issue.empty()) {
        std::vector<peepo::ModuleDefinition> both;
        for (const auto& d : peepo::demo_catalogue()) if (d.category != "Endpoint") both.push_back(d);
        both.insert(both.end(), offered.begin(), offered.end());
        peepo::replace_catalogue(std::move(both));
    }
    if (!catalogue_issue.empty() && !address.empty()) std::cerr << "catalogue: " << catalogue_issue << "\n";

    if (display_name.empty()) {
#ifdef PEEPO_LVGL_SDL
        if (std::getenv("DISPLAY") || std::getenv("WAYLAND_DISPLAY")) display_name = "sdl";
#endif
#ifdef PEEPO_LVGL_DRM
        if (display_name.empty()) display_name = "drm";
#endif
        if (display_name.empty()) display_name = "fbdev";
    }

    lv_init();
    lv_tick_set_cb(now_ms);
    lv_display_t* display = nullptr;
    std::vector<std::uint32_t> offscreen;
    std::vector<peepo::ControlEvent> key_events;
    auto* group = lv_group_create();
    lv_group_set_default(group);
    if (display_name == "offscreen") {
        display = lv_display_create(800, 480);
        offscreen.resize(800 * 480);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_buffers(display, offscreen.data(), nullptr, static_cast<std::uint32_t>(offscreen.size() * 4), LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, std::uint8_t*) { lv_display_flush_ready(d); });
    }
#ifdef PEEPO_LVGL_SDL
    else if (display_name == "sdl") {
        display = lv_sdl_window_create(800, 480);
        if (display) {
            lv_sdl_window_set_title(display, address.empty() ? "Peepo - UI simulator" : "Peepo - Ingen engine");
            lv_sdl_mouse_create();
            lv_indev_set_group(lv_sdl_keyboard_create(), group);
            SDL_AddEventWatch(sdl_keys, &key_events);
        }
    }
#endif
#ifdef PEEPO_LVGL_DRM
    else if (display_name == "drm") {
        display = lv_linux_drm_create();
        lv_linux_drm_set_mode_cb(display, pick_mode);
        std::string card = drm_device;
        if (card.empty()) if (char* found = lv_linux_drm_find_device_path()) { card = found; lv_free(found); }
        if (!card.empty() && !wait_for_screen(card)) return stopping ? 0 : 1;
        if (card.empty() || lv_linux_drm_set_file(display, card.c_str(), -1) != LV_RESULT_OK) {
            std::cerr << "no usable DRM device" << (card.empty() ? "" : " at " + card) << "\n";
            return 1;
        }
    }
#endif
    else if (display_name == "fbdev") {
        display = lv_linux_fbdev_create();
        if (lv_linux_fbdev_set_file(display, fbdev_path.c_str()) != LV_RESULT_OK) { std::cerr << "cannot open " << fbdev_path << "\n"; return 1; }
    } else {
        std::cerr << "display " << display_name << " is not available in this build\n";
        return 2;
    }
    if (!display) { std::cerr << "could not create the " << display_name << " display\n"; return 1; }
    if (rotation == 90) lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
    if (rotation == 180) lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);
    if (rotation == 270) lv_display_set_rotation(display, LV_DISPLAY_ROTATION_270);

    // The touch screen, on the Pi's own displays.
    std::set<std::string> touch_devices;
    if ((display_name == "drm" || display_name == "fbdev") && touch_path != "none") {
        if (touch_path.empty()) touch_path = peepo::linux_input::find_touchscreen().value_or("");
        if (touch_path.empty()) std::cerr << "no touch screen found; use --touch PATH\n";
        else if (auto* touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, touch_path.c_str())) {
            lv_indev_set_display(touch, display);
            touch_devices.insert(touch_path);
        } else std::cerr << "cannot open touch screen " << touch_path << "\n";
    }

    // Physical controls from the kernel's input devices.
    std::unique_ptr<peepo::linux_input::EvdevControls> controls;
    if (controls_path.empty() && std::filesystem::exists("/etc/peepo/controls.conf")) controls_path = "/etc/peepo/controls.conf";
    if (!controls_path.empty()) {
        std::vector<peepo::linux_input::Binding> bindings;
        if (const auto issue = peepo::linux_input::parse_bindings(read_file(controls_path), bindings); !issue.empty())
            std::cerr << controls_path << ": " << issue << "\n";
        else controls = std::make_unique<peepo::linux_input::EvdevControls>(std::move(bindings), "/dev/input", touch_devices);
    }

    std::unique_ptr<peepo::Engine> engine;
    if (address.empty()) engine = std::make_unique<peepo::SimulatedEngine>();
    else {
        engine = std::make_unique<peepo::IngenEngine>(peepo::ingen::Options{address});
        engine->connect();
    }
    std::unique_ptr<peepo::PatchStore> store;
    if (!smoke) {
        const auto root = !patches_path.empty() ? std::filesystem::path(patches_path) : data_home() / "peepo" / (address.empty() ? "simulator-patches" : "patches");
        store = std::make_unique<peepo::PatchStore>(root, engine_patches);
        if (const auto issue = store->open(); !issue.empty()) std::cerr << "patches: " << issue << "\n";
    }
    // Models and impulse responses, on the same machine as the engine here.
    std::unique_ptr<peepo::FileStore> files;
    if (!smoke) {
        files = std::make_unique<peepo::FileStore>(!files_path.empty() ? std::filesystem::path(files_path) : data_home() / "peepo" / "files");
        if (const auto issue = files->open(); !issue.empty()) std::cerr << "files: " << issue << "\n";
    }
    // Instrument-wide MIDI, shared with peepo-midi and peepo-route.
    std::unique_ptr<peepo::midi::Link> midi;
    if (!smoke) midi = std::make_unique<peepo::midi::Link>(!midi_path.empty() ? std::filesystem::path(midi_path) : store->root().parent_path() / (address.empty() ? "simulator-midi.conf" : "midi.conf"), midi_status);
    peepo::Application app(*engine, store.get());
    app.set_files(files.get());
    app.set_midi(midi.get());
    // Only the instrument, drawing on its own screen, may power the machine off or restart it.
    if (display_name == "drm" || display_name == "fbdev") {
        app.set_power_off([] { return std::system("systemctl poweroff") == 0 ? std::string() : std::string("Could not shut down: the system refused."); });
        app.set_restart([] { return std::system("systemctl reboot") == 0 ? std::string() : std::string("Could not restart: the system refused."); });
    }
    app.set_network([] {
        const auto links = peepo::linux_network::links();
        const bool connected = std::any_of(links.begin(), links.end(), [](const auto& link) { return !link.address.empty(); });
        return peepo::Application::Network{connected, peepo::linux_network::describe(links)};
    });
    app.set_system([] {
        const auto r = peepo::linux_system::read();
        return peepo::Application::System{r.temperature_c, r.memory_total, r.memory_available, r.under_voltage, r.uptime_s};
    });

    const auto started = now_ms();
    auto last = started;
    while (!stopping) {
        const auto now = now_ms();
        for (auto& event : key_events) app.control(event);
        key_events.clear();
        if (controls) for (const auto& event : controls->poll(now - last)) app.control(event);
        app.tick(now - last);
        last = now;
        const auto wait = lv_timer_handler();
        usleep(std::clamp<std::uint32_t>(wait, 1, 5) * 1000);
        if (smoke && now - started > 1000) break;
    }
    if (smoke) {
        // Something was drawn: the background and the panels of the patch page.
        auto* buffer = lv_display_get_buf_active(display);
        unsigned dark = 0, cards = 0;
        if (buffer && buffer->data && lv_color_format_get_size(static_cast<lv_color_format_t>(buffer->header.cf)) == 4)
            for (unsigned y = 0; y < buffer->header.h; ++y) {
                const auto* row = reinterpret_cast<const std::uint32_t*>(buffer->data + y * buffer->header.stride);
                for (unsigned x = 0; x < buffer->header.w; ++x) { dark += (row[x] & 0xffffff) == 0x000000; cards += (row[x] & 0xffffff) == 0x0f0f0f; }
            }
        std::cout << "display=" << display_name << " resolution=" << lv_display_get_horizontal_resolution(display) << "x" << lv_display_get_vertical_resolution(display)
                  << " background_pixels=" << dark << " card_pixels=" << cards << "\n";
        if (!screenshot.empty()) write_bmp(display, screenshot);
        if (display_name == "offscreen" && (dark < 100000 || cards < 10000)) return 3;
    }
    engine->disconnect();
    return 0;
}
