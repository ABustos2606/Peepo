// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "core/model.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

// Physical controls, independent of where their events come from: the
// simulator's keyboard now, the Pi's GPIO through Linux input later. Every
// source produces the same raw events; everything after that is shared.
// See docs/18-physical-controls.md.
namespace peepo {
// The instrument's surface: push encoders enc1..encN, stomp switches
// stomp1..stompN, and after them one more switch for each two neighbours
// pressed together, as on Beebo: with three stomps A, B and C, stomp4 is D
// (A+B) and stomp5 is E (B+C).
struct ControlLayout {
    unsigned encoders = 2, stomps = 3;
    std::vector<std::string> ids() const;
};
// "Encoder 2", "Stomp A" to "Stomp E".
std::string control_label(const std::string& id);
// The stomp's letter, A for stomp1; empty for anything else.
std::string stomp_letter(const std::string& id);
bool is_encoder(const std::string& id);
bool is_stomp(const std::string& id);

// What a control reports. An encoder's push is a Press and Release on the
// encoder itself; a Turn carries detents, positive clockwise.
struct ControlEvent {
    enum class Type { Turn, Press, Release };
    Type type = Type::Turn;
    std::string control;
    int steps = 0;
};
// What it means. Encoders: Turn (fine while the push is held), Tap (pushed and
// released without turning), Hold (pushed long without turning). Stomps: Press
// and Release, never waiting for a gesture to end; a press only waits
// chord_ms for its neighbour, which together make the stomp after the
// physical ones (A+B is D).
struct Gesture {
    enum class Kind { Turn, Tap, Hold, Press, Release };
    Kind kind = Kind::Turn;
    std::string control;
    int steps = 0;
    bool fine = false;
    // Since this encoder's previous detent; drives acceleration.
    unsigned interval_ms = 0;
};
// Tap tempo: the beats per minute of the latest taps. Two taps give a tempo;
// up to four intervals are averaged. A pause longer than the slowest tempo
// starts again, and a tempo outside the patch range is not given.
class TapTempo {
public:
    std::optional<double> tap(unsigned long long milliseconds);
    void reset() { taps_.clear(); }
    std::size_t taps() const { return taps_.size(); }
private:
    std::vector<unsigned long long> taps_;
};
class ControlRecognizer {
public:
    static constexpr unsigned hold_ms = 700;
    // A switch that closes again this soon after opening is bouncing.
    static constexpr unsigned bounce_ms = 25;
    // A stomp acts this long after it is pressed, unless its neighbour joins
    // it first: then the two are the one switch between them.
    static constexpr unsigned chord_ms = 40;
    // `stomps` physical stomps, stomp1 to stompN.
    explicit ControlRecognizer(unsigned stomps = 3) : stomps_(stomps) {}
    void input(const ControlEvent&);
    std::vector<Gesture> advance(unsigned milliseconds);
    // Forget every pressed control, as when the control source reconnects.
    void reset();
private:
    struct State {
        bool pressed = false, used = false;
        unsigned long long pressed_at = 0, released_at = 0, turned_at = 0;
        bool turned = false, released = false;
        // A stomp pressed and waiting chord_ms for its neighbour.
        bool waiting = false;
        // The two-stomp switch it is part of while held, as D; empty for none.
        // Ended when the first of the two is released.
        std::string chord;
    };
    // The physical stomp's number, or 0 for any other control.
    unsigned physical(const std::string& id) const;
    unsigned stomps_;
    std::map<std::string, State> states_;
    std::vector<Gesture> pending_;
    unsigned long long clock_ = 0;
};

// Mapping rules. Encoders are relative, so a patch change never makes a value
// jump. Continuous values move in the parameter's own (linear or logarithmic)
// space, within the assignment's range.
inline constexpr double turn_step = 1.0 / 50, fine_step = 1.0 / 500;
// 1 for slow turning, rising to a bounded 5 for fast spins; never when fine.
double acceleration(unsigned interval_ms, bool fine);
// The value after `steps` detents. `range` narrows the parameter (and may
// invert it); without one the whole range is used.
double turned_value(const Parameter&, double current, int steps, bool fine, unsigned interval_ms, const Assignment* range = nullptr);
// A stomp switch on a parameter: Toggle flips between the range's ends on each
// press; Momentary holds the far end while pressed. Nothing when the event
// does nothing in that mode.
std::optional<double> switched_value(const Parameter&, double current, const Assignment&, bool pressed);
// Soft takeover for an absolute control (a potentiometer or an expression
// pedal): its position takes effect only once it reaches or crosses the
// current value, so a patch change never makes the sound jump. Positions and
// value are normalized 0..1.
class Pickup {
public:
    std::optional<double> move(double current, double position);
    void release() { previous_.reset(); engaged_ = false; }
private:
    std::optional<double> previous_;
    bool engaged_ = false;
};
}
