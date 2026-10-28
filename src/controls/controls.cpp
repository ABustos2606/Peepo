// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "controls/controls.hpp"
#include <algorithm>
#include <cmath>

namespace peepo {
std::optional<double> TapTempo::tap(unsigned long long ms) {
    const auto longest = static_cast<unsigned long long>(60000 / minimum_tempo);
    if (!taps_.empty() && (ms <= taps_.back() || ms - taps_.back() > longest)) taps_.clear();
    taps_.push_back(ms);
    if (taps_.size() > 5) taps_.erase(taps_.begin());
    if (taps_.size() < 2) return std::nullopt;
    const double beat = static_cast<double>(taps_.back() - taps_.front()) / static_cast<double>(taps_.size() - 1);
    const double bpm = 60000 / beat;
    if (bpm < minimum_tempo || bpm > maximum_tempo) return std::nullopt;
    return std::round(bpm * 10) / 10;
}
std::vector<std::string> ControlLayout::ids() const {
    std::vector<std::string> out;
    for (unsigned i = 1; i <= encoders; ++i) out.push_back("enc" + std::to_string(i));
    // The physical stomps, then one for each two neighbours pressed together.
    for (unsigned i = 1; i <= stomps; ++i) out.push_back("stomp" + std::to_string(i));
    for (unsigned i = 1; i < stomps; ++i) out.push_back(chord_id(i));
    return out;
}
std::string chord_id(unsigned first) { return "stomp" + std::to_string(first) + std::to_string(first + 1); }
namespace {
// The prefix, then a number: enc1, stomp3.
bool numbered(const std::string& id, std::string_view prefix) {
    return id.size() > prefix.size() && id.size() <= prefix.size() + 3 && id.starts_with(prefix) &&
           std::all_of(id.begin() + static_cast<std::ptrdiff_t>(prefix.size()), id.end(), [](char c) { return c >= '0' && c <= '9'; });
}
}
bool is_encoder(const std::string& id) { return numbered(id, "enc"); }
bool is_stomp(const std::string& id) { return numbered(id, "stomp"); }
unsigned chord_first(const std::string& id) {
    if (!is_stomp(id) || id.size() != 7) return 0;
    const unsigned first = static_cast<unsigned>(id[5] - '0'), second = static_cast<unsigned>(id[6] - '0');
    return first >= 1 && second == first + 1 ? first : 0;
}
bool is_chord(const std::string& id) { return chord_first(id) != 0; }
std::string stomp_letter(const std::string& id) {
    if (!is_stomp(id)) return {};
    const auto letter = [](unsigned n) { return std::string(1, static_cast<char>('A' + n - 1)); };
    if (const auto first = chord_first(id)) return letter(first) + "+" + letter(first + 1);
    const auto n = std::stoul(id.substr(5));
    return n >= 1 && n <= 26 ? letter(static_cast<unsigned>(n)) : id.substr(5);
}
std::string control_label(const std::string& id) {
    if (is_encoder(id)) return "Encoder " + id.substr(3);
    if (is_stomp(id)) return "Stomp " + stomp_letter(id);
    return id;
}

void ControlRecognizer::reset() { states_.clear(); pending_.clear(); }
unsigned ControlRecognizer::physical(const std::string& id) const {
    if (!is_stomp(id)) return 0;
    const auto n = std::stoul(id.substr(5));
    return n >= 1 && n <= stomps_ ? static_cast<unsigned>(n) : 0;
}
void ControlRecognizer::input(const ControlEvent& event) {
    auto& state = states_[event.control];
    const auto stomp = physical(event.control);
    switch (event.type) {
    case ControlEvent::Type::Turn: {
        if (!event.steps) return;
        const auto interval = state.turned ? static_cast<unsigned>(std::min<unsigned long long>(clock_ - state.turned_at, 60000)) : 60000u;
        state.turned = true; state.turned_at = clock_;
        // Turning while pushed is fine adjustment, and spends the push.
        if (state.pressed) state.used = true;
        pending_.push_back({Gesture::Kind::Turn, event.control, event.steps, state.pressed, interval});
        break;
    }
    case ControlEvent::Type::Press: {
        if (state.pressed) return;
        // Contacts that close again just after opening are bouncing, not pressed.
        if (state.released && clock_ - state.released_at < bounce_ms) return;
        state.pressed = true; state.used = false; state.pressed_at = clock_;
        if (!is_stomp(event.control)) break;
        // Any other stomp, such as a chord sent by itself, acts at once.
        if (!stomp) { pending_.push_back({Gesture::Kind::Press, event.control}); break; }
        if (!chords_) { pending_.push_back({Gesture::Kind::Press, event.control}); break; }
        // A neighbour still waiting makes the two one switch, the chord of
        // both (A+B is stomp12).
        for (const auto other : {stomp - 1, stomp + 1}) {
            if (other < 1 || other > stomps_) continue;
            auto& neighbour = states_["stomp" + std::to_string(other)];
            if (!neighbour.waiting) continue;
            neighbour.waiting = false;
            // Spent: neither says anything more when released.
            state.used = neighbour.used = true;
            state.chord = neighbour.chord = chord_id(std::min(stomp, other));
            auto& chord = states_[state.chord];
            chord.pressed = true; chord.used = false; chord.pressed_at = clock_;
            pending_.push_back({Gesture::Kind::Press, state.chord});
            return;
        }
        state.waiting = true;
        break;
    }
    case ControlEvent::Type::Release:
        if (!state.pressed) return;
        state.pressed = false; state.released = true; state.released_at = clock_;
        if (!is_stomp(event.control)) { if (!state.used) pending_.push_back({Gesture::Kind::Tap, event.control}); break; }
        if (state.waiting) {
            // Released before its wait was over: a quick press, played now.
            state.waiting = false;
            pending_.push_back({Gesture::Kind::Press, event.control});
            pending_.push_back({Gesture::Kind::Release, event.control});
        } else if (!state.chord.empty()) {
            // The first of the two released ends the chord; the other's
            // release then says nothing.
            const auto chord = state.chord;
            for (auto& entry : states_) if (entry.second.chord == chord) entry.second.chord.clear();
            end_chord(chord);
        } else if (is_chord(event.control)) end_chord(event.control);
        else if (!stomp || !state.used) pending_.push_back({Gesture::Kind::Release, event.control});
        break;
    }
}
void ControlRecognizer::end_chord(const std::string& chord) {
    auto& state = states_[chord];
    // Held long, it gave Hold already; otherwise it was a tap.
    if (!state.used) pending_.push_back({Gesture::Kind::Tap, chord});
    state.used = true; state.pressed = false; state.released = true; state.released_at = clock_;
    pending_.push_back({Gesture::Kind::Release, chord});
}
std::vector<Gesture> ControlRecognizer::advance(unsigned milliseconds) {
    clock_ += milliseconds;
    for (auto& [control, state] : states_) {
        if (state.pressed && !state.used && (is_encoder(control) || is_chord(control)) &&
            clock_ - state.pressed_at >= (is_chord(control) ? chord_hold_ms : hold_ms)) {
            state.used = true;
            pending_.push_back({Gesture::Kind::Hold, control});
        }
        // No neighbour joined in time: the stomp alone.
        if (state.waiting && clock_ - state.pressed_at >= chord_ms) {
            state.waiting = false;
            pending_.push_back({Gesture::Kind::Press, control});
        }
    }
    auto out = std::move(pending_);
    pending_.clear();
    return out;
}

double acceleration(unsigned interval_ms, bool fine) {
    if (fine) return 1;
    // Detents this close together are a deliberate spin; bounded so one flick
    // never crosses the whole range.
    if (interval_ms <= 25) return 5;
    if (interval_ms <= 50) return 3;
    if (interval_ms <= 100) return 2;
    return 1;
}
namespace {
// The parameter narrowed to an assignment's range, ends swapped when inverted.
struct Span { Parameter parameter; bool inverted = false; };
Span span(const Parameter& p, const Assignment* range) {
    Span s{p, false};
    if (range) {
        s.parameter.minimum = std::max(p.minimum, std::min(range->minimum, range->maximum));
        s.parameter.maximum = std::min(p.maximum, std::max(range->minimum, range->maximum));
        if (s.parameter.minimum > s.parameter.maximum) s.parameter.minimum = s.parameter.maximum;
        s.inverted = range->inverted;
    }
    return s;
}
}
double turned_value(const Parameter& p, double current, int steps, bool fine, unsigned interval_ms, const Assignment* range) {
    auto narrowed = span(p, range);
    auto& q = narrowed.parameter;
    if (narrowed.inverted) steps = -steps;
    current = std::clamp(current, q.minimum, q.maximum);
    if (!steps) return current;
    switch (q.kind) {
    case ParameterKind::Toggle: return steps > 0 ? q.maximum : q.minimum;
    case ParameterKind::Enumeration: return std::clamp(std::round(current) + steps, q.minimum, q.maximum);
    case ParameterKind::Integer: {
        // Small ranges move one value per detent; wide ones may accelerate.
        const double rate = q.maximum - q.minimum > 24 ? acceleration(interval_ms, fine) : 1;
        return std::clamp(std::round(current) + steps * rate, q.minimum, q.maximum);
    }
    case ParameterKind::Continuous: break;
    }
    if (q.maximum <= q.minimum) return q.minimum;
    q.value = current;
    const auto moved = normalized_value(q) + steps * (fine ? fine_step : turn_step * acceleration(interval_ms, fine));
    return std::clamp(value_from_normalized(q, std::clamp(moved, 0.0, 1.0)), q.minimum, q.maximum);
}
std::optional<double> switched_value(const Parameter& p, double current, const Assignment& a, bool pressed) {
    const auto [q, inverted] = span(p, &a);
    const double off = inverted ? q.maximum : q.minimum, on = inverted ? q.minimum : q.maximum;
    if (a.mode == SwitchMode::Momentary) return pressed ? on : off;
    if (!pressed) return std::nullopt;
    // Whichever end the value is nearer, go to the other.
    return std::abs(current - on) < std::abs(current - off) ? off : on;
}
std::optional<double> Pickup::move(double current, double position) {
    constexpr double near = 0.01;
    if (!engaged_) {
        const bool crossed = previous_ && (*previous_ - current) * (position - current) <= 0;
        previous_ = position;
        if (!crossed && std::abs(position - current) >= near) return std::nullopt;
        engaged_ = true;
    }
    previous_ = position;
    return position;
}
}
