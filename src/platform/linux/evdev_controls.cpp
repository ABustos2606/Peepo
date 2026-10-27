// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "platform/linux/evdev_controls.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace peepo::linux_input {
namespace {
struct Named { const char* name; unsigned code; };
// The codes the Pi's overlays are given; any other is accepted as a number.
constexpr Named names[] = {
    {"REL_X", REL_X}, {"REL_Y", REL_Y}, {"REL_Z", REL_Z}, {"REL_RX", REL_RX}, {"REL_RY", REL_RY}, {"REL_RZ", REL_RZ},
    {"REL_DIAL", REL_DIAL}, {"REL_WHEEL", REL_WHEEL}, {"REL_HWHEEL", REL_HWHEEL},
    {"KEY_F13", KEY_F13}, {"KEY_F14", KEY_F14}, {"KEY_F15", KEY_F15}, {"KEY_F16", KEY_F16}, {"KEY_F17", KEY_F17}, {"KEY_F18", KEY_F18},
    {"KEY_F19", KEY_F19}, {"KEY_F20", KEY_F20}, {"KEY_F21", KEY_F21}, {"KEY_F22", KEY_F22}, {"KEY_F23", KEY_F23}, {"KEY_F24", KEY_F24},
    {"KEY_ENTER", KEY_ENTER}, {"KEY_SPACE", KEY_SPACE}, {"BTN_0", BTN_0}, {"BTN_1", BTN_1}, {"BTN_2", BTN_2}, {"BTN_3", BTN_3},
    {"BTN_4", BTN_4}, {"BTN_5", BTN_5}, {"BTN_6", BTN_6}, {"BTN_7", BTN_7}};
// A code no higher than `highest`: REL_MAX for a turn, KEY_MAX for a key.
bool code_of(const std::string& text, unsigned highest, unsigned& code) {
    bool found = false;
    for (const auto& named : names) if (text == named.name) { code = named.code; found = true; }
    if (!found) {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), code);
        found = result.ec == std::errc{} && result.ptr == text.data() + text.size();
    }
    return found && code <= highest;
}
template <std::size_t N> bool has_bit(const unsigned long (&bits)[N], unsigned bit) {
    constexpr unsigned width = 8 * sizeof(unsigned long);
    return bit / width < N && ((bits[bit / width] >> (bit % width)) & 1UL);
}
constexpr unsigned long_count(unsigned bits) { return bits / (8 * sizeof(unsigned long)) + 1; }
std::string device_name(int fd) {
    char name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof name - 1), name) < 0) return {};
    return name;
}
std::vector<std::string> event_nodes(const std::string& directory) {
    std::vector<std::string> out;
    if (auto* dir = opendir(directory.c_str())) {
        while (const auto* entry = readdir(dir))
            if (std::string_view(entry->d_name).starts_with("event")) out.push_back(directory + "/" + entry->d_name);
        closedir(dir);
    }
    std::sort(out.begin(), out.end());
    return out;
}
}

std::string parse_bindings(std::string_view text, std::vector<Binding>& out) {
    out.clear();
    std::istringstream lines{std::string(text)};
    int number = 0;
    for (std::string line; std::getline(lines, line);) {
        ++number;
        line = line.substr(0, line.find('#'));
        std::istringstream words(line);
        std::vector<std::string> word;
        for (std::string w; words >> w;) word.push_back(w);
        if (word.empty()) continue;
        const auto where = "line " + std::to_string(number) + ": ";
        if (word.size() < 3) return where + "expected `control turn|key CODE`";
        Binding b;
        b.control = word[0];
        if (!valid_control_id(b.control) || !(is_encoder(b.control) || is_stomp(b.control))) return where + "unknown control " + word[0];
        if (word[1] == "turn") b.kind = Binding::Kind::Turn;
        else if (word[1] != "key") return where + "expected turn or key";
        if (b.kind == Binding::Kind::Turn && !is_encoder(b.control)) return where + "only encoders turn";
        if (!code_of(word[2], b.kind == Binding::Kind::Turn ? REL_MAX : KEY_MAX, b.code)) return where + "unknown code " + word[2] + " for " + word[1];
        for (std::size_t i = 3; i < word.size(); ++i) {
            if (word[i] == "reverse") b.reverse = true;
            else if (word[i].starts_with("device=")) b.device = word[i].substr(7);
            else return where + "unknown option " + word[i];
        }
        out.push_back(std::move(b));
    }
    return {};
}
std::optional<ControlEvent> translate(const std::vector<Binding>& bindings, const std::string& device, unsigned type, unsigned code, int value) {
    for (const auto& b : bindings) {
        if (b.code != code || (!b.device.empty() && b.device != device)) continue;
        if (type == EV_REL && b.kind == Binding::Kind::Turn && value)
            return ControlEvent{ControlEvent::Type::Turn, b.control, b.reverse ? -value : value};
        // Value 2 is the kernel's autorepeat while a key is held: not a new press.
        if (type == EV_KEY && b.kind == Binding::Kind::Key && value != 2)
            return ControlEvent{value ? ControlEvent::Type::Press : ControlEvent::Type::Release, b.control, 0};
    }
    return std::nullopt;
}

EvdevControls::EvdevControls(std::vector<Binding> bindings, std::string directory, std::set<std::string> exclude)
    : bindings_(std::move(bindings)), directory_(std::move(directory)), exclude_(std::move(exclude)) {}
EvdevControls::~EvdevControls() { for (const auto& d : devices_) close(d.fd); }
std::vector<std::string> EvdevControls::devices() const {
    std::vector<std::string> out;
    for (const auto& d : devices_) out.push_back(d.name);
    return out;
}
void EvdevControls::scan() {
    since_scan_ = 0;
    for (const auto& path : event_nodes(directory_)) {
        if (exclude_.contains(path) || std::any_of(devices_.begin(), devices_.end(), [&](const Device& d) { return d.path == path; })) continue;
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        // Open only devices that report something bound, so a keyboard or
        // the touch screen is never held for nothing.
        unsigned long rel[long_count(REL_MAX)] = {}, key[long_count(KEY_MAX)] = {};
        if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof rel), rel) < 0 || ioctl(fd, EVIOCGBIT(EV_KEY, sizeof key), key) < 0) { close(fd); continue; }
        const auto name = device_name(fd);
        const bool wanted = std::any_of(bindings_.begin(), bindings_.end(), [&](const Binding& b) {
            return (b.device.empty() || b.device == name) && (b.kind == Binding::Kind::Turn ? has_bit(rel, b.code) : has_bit(key, b.code));
        });
        if (wanted) devices_.push_back({fd, path, name});
        else close(fd);
    }
}
std::vector<ControlEvent> EvdevControls::poll(unsigned milliseconds) {
    since_scan_ += milliseconds;
    if (since_scan_ >= rescan_ms) scan();
    std::vector<ControlEvent> out;
    for (auto it = devices_.begin(); it != devices_.end();) {
        input_event events[64];
        bool gone = false;
        for (;;) {
            const auto count = read(it->fd, events, sizeof events);
            if (count < 0) { gone = errno != EAGAIN && errno != EINTR; break; }
            if (count == 0) { gone = true; break; }
            for (std::size_t i = 0; i < static_cast<std::size_t>(count) / sizeof(input_event); ++i)
                if (auto event = translate(bindings_, it->name, events[i].type, events[i].code, events[i].value)) out.push_back(std::move(*event));
        }
        // Unplugged, or its driver went away: find it again on a later scan.
        if (gone) { close(it->fd); it = devices_.erase(it); }
        else ++it;
    }
    return out;
}
std::optional<std::string> find_touchscreen(const std::string& directory) {
    for (const auto& path : event_nodes(directory)) {
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        unsigned long abs[long_count(ABS_MAX)] = {}, key[long_count(KEY_MAX)] = {};
        const bool read = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof abs), abs) >= 0 && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof key), key) >= 0;
        close(fd);
        if (!read) continue;
        const bool positions = has_bit(abs, ABS_MT_POSITION_X) || (has_bit(abs, ABS_X) && has_bit(abs, ABS_Y));
        if (positions && has_bit(key, BTN_TOUCH)) return path;
    }
    return std::nullopt;
}
}
