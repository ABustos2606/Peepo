// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// The physical controls on Linux: bindings, event translation, and, where the
// kernel offers uinput and evdev, a virtual device read through the real path.
#include "platform/linux/evdev_controls.hpp"
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using namespace peepo;
using namespace peepo::linux_input;
namespace {
void check(bool result, const char* why) { if (!result) throw std::runtime_error(why); }
std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream text; text << in.rdbuf();
    return text.str();
}

void bindings(const std::string& shipped) {
    std::vector<Binding> b;
    check(parse_bindings(read_file(shipped), b).empty() && b.size() == 7, "the shipped bindings do not read as two encoders with pushes and three stomps");
    check(b[0].control == "enc1" && b[0].kind == Binding::Kind::Turn && b[0].code == REL_X, "encoder 1 is not REL_X");
    auto turn = translate(b, "rotary@5", EV_REL, REL_Y, -1);
    check(turn && turn->type == ControlEvent::Type::Turn && turn->control == "enc2" && turn->steps == -1, "a turn of encoder 2 was not translated");
    auto press = translate(b, "button@c", EV_KEY, KEY_F13, 1), release = translate(b, "button@c", EV_KEY, KEY_F13, 0);
    check(press && press->type == ControlEvent::Type::Press && press->control == "enc1" && release && release->type == ControlEvent::Type::Release, "a push was not translated");
    check(!translate(b, "x", EV_KEY, KEY_F13, 2), "the kernel's autorepeat became a press");
    check(!translate(b, "x", EV_KEY, KEY_A, 1) && !translate(b, "x", EV_REL, REL_X, 0) && !translate(b, "x", EV_ABS, REL_X, 3), "an unbound event was translated");
    check(translate(b, "x", EV_KEY, KEY_F19, 1)->control == "stomp3", "stomp 3 not bound");

    check(parse_bindings("enc1 turn REL_X reverse device=rotary@5\n# comment\n\nstomp1 key 189\n", b).empty() && b.size() == 2 && b[1].code == 189, "options or numeric codes not read");
    check(translate(b, "rotary@5", EV_REL, REL_X, 2)->steps == -2 && !translate(b, "rotary@d", EV_REL, REL_X, 2), "reverse or device filter ignored");
    check(parse_bindings("enc1 turn 15\nenc2 key REL_DIAL\nstomp1 key 767\n", b).empty() && b.size() == 3, "codes at the edge of their range refused");
    for (const char* bad : {"enc9x turn REL_X", "stomp1 turn REL_X", "enc1 spin REL_X", "enc1 turn REL_NOPE", "enc1 key KEY_F13 sideways", "enc1",
                            // A code beyond its event type's range: a turn is REL_, a key up to KEY_MAX.
                            "enc1 turn 64", "enc1 turn 16", "enc1 turn KEY_ENTER", "stomp1 key 768", "stomp1 key 99999999999"})
        check(!parse_bindings(bad, b).empty(), "a malformed binding was accepted");
}

// A virtual input device, as a rotary-encoder or gpio-key overlay would make.
struct Virtual {
    int fd = -1;
    Virtual(const char* name, bool touch) {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) return;
        if (touch) {
            ioctl(fd, UI_SET_EVBIT, EV_ABS); ioctl(fd, UI_SET_EVBIT, EV_KEY);
            ioctl(fd, UI_SET_ABSBIT, ABS_X); ioctl(fd, UI_SET_ABSBIT, ABS_Y); ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
            for (int axis : {ABS_X, ABS_Y}) { uinput_abs_setup abs{}; abs.code = static_cast<__u16>(axis); abs.absinfo.maximum = 4095; ioctl(fd, UI_ABS_SETUP, &abs); }
        } else {
            ioctl(fd, UI_SET_EVBIT, EV_REL); ioctl(fd, UI_SET_EVBIT, EV_KEY);
            ioctl(fd, UI_SET_RELBIT, REL_X); ioctl(fd, UI_SET_KEYBIT, KEY_F13); ioctl(fd, UI_SET_KEYBIT, KEY_F17);
        }
        uinput_setup setup{};
        std::strncpy(setup.name, name, UINPUT_MAX_NAME_SIZE - 1);
        setup.id.bustype = BUS_VIRTUAL;
        if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) { close(fd); fd = -1; }
    }
    ~Virtual() { if (fd >= 0) { ioctl(fd, UI_DEV_DESTROY); close(fd); } }
    void send(unsigned type, unsigned code, int value) {
        input_event e{}; e.type = static_cast<__u16>(type); e.code = static_cast<__u16>(code); e.value = value;
        (void)!write(fd, &e, sizeof e);
        input_event sync{}; sync.type = EV_SYN; sync.code = SYN_REPORT;
        (void)!write(fd, &sync, sizeof sync);
    }
};
// True when it ran; false when this kernel cannot provide the devices.
bool real_devices(const std::string& shipped) {
    Virtual knob("peepo-test-controls", false);
    if (knob.fd < 0) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (!std::filesystem::exists("/dev/input")) return false;
    std::vector<Binding> b;
    parse_bindings(read_file(shipped), b);
    EvdevControls controls(b);
    controls.poll(0);
    bool found = false;
    for (const auto& name : controls.devices()) found = found || name == "peepo-test-controls";
    check(found, "the virtual control device was not opened");
    knob.send(EV_REL, REL_X, 1); knob.send(EV_REL, REL_X, 1); knob.send(EV_KEY, KEY_F17, 1); knob.send(EV_KEY, KEY_F17, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto events = controls.poll(10);
    check(events.size() == 4 && events[0].control == "enc1" && events[0].steps == 1 && events[2].control == "stomp1" &&
          events[2].type == ControlEvent::Type::Press && events[3].type == ControlEvent::Type::Release, "events from the virtual device were not read");
    {
        Virtual touch("peepo-test-touch", true);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        const auto found_touch = find_touchscreen();
        check(found_touch && controls.poll(EvdevControls::rescan_ms).empty(), "touch screen not found, or taken for a control");
    }
    return true;
}
}
int main(int argc, char** argv) {
    try {
        const std::string shipped = argc > 1 ? argv[1] : "config/pi/controls.conf";
        bindings(shipped);
        const bool live = real_devices(shipped);
        std::cout << "Control bindings and translation passed; " << (live ? "virtual devices read through evdev." : "evdev devices not available here, skipped.") << "\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
