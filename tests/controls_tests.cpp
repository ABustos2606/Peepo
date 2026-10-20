// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// Physical controls without the hardware: gestures from raw events, the
// mapping rules, soft takeover, and assignments kept with the patch.
#include "controls/controls.hpp"
#include "engine/simulated_engine.hpp"
#include "storage/patch_store.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace peepo;
namespace {
void check(bool result, const char* why) { if (!result) throw std::runtime_error(why); }
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
using Type = ControlEvent::Type;
using Kind = Gesture::Kind;

void gestures() {
    ControlRecognizer r;
    // A push released quickly is a tap; one held long is a hold, once.
    r.input({Type::Press, "enc1"});
    check(r.advance(100).empty(), "encoder press reported before it was a gesture");
    r.input({Type::Release, "enc1"});
    auto out = r.advance(0);
    check(out.size() == 1 && out[0].kind == Kind::Tap && out[0].control == "enc1", "tap not recognized");
    r.advance(100);
    r.input({Type::Press, "enc2"});
    check(r.advance(ControlRecognizer::hold_ms - 1).empty(), "hold reported early");
    out = r.advance(1);
    check(out.size() == 1 && out[0].kind == Kind::Hold, "hold not recognized");
    r.input({Type::Release, "enc2"});
    check(r.advance(2000).empty(), "a hold also produced a tap or a second hold");
    // Turning while pushed is fine adjustment, and neither a tap nor a hold.
    r.advance(100);
    r.input({Type::Press, "enc3"});
    r.input({Type::Turn, "enc3", 1});
    out = r.advance(30);
    r.input({Type::Turn, "enc3", -2});
    for (auto& g : r.advance(2000)) out.push_back(g);
    check(out.size() == 2 && out[0].fine && out[1].fine && out[1].steps == -2 && out[1].interval_ms == 30, "fine turns or their timing lost");
    r.input({Type::Release, "enc3"});
    check(r.advance(0).empty(), "turning while pushed ended in a tap");
    r.input({Type::Turn, "enc3", 1});
    out = r.advance(0);
    check(out.size() == 1 && !out[0].fine, "turn after release still fine");
    // Stomps act on the press itself, and a bouncing contact is one press.
    r.input({Type::Press, "stomp1"});
    out = r.advance(0);
    check(out.size() == 1 && out[0].kind == Kind::Press, "stomp press waited for its release");
    r.advance(80);
    r.input({Type::Release, "stomp1"});
    out = r.advance(ControlRecognizer::bounce_ms - 5);
    r.input({Type::Press, "stomp1"});
    r.input({Type::Release, "stomp1"});
    for (auto& g : r.advance(0)) out.push_back(g);
    check(out.size() == 1 && out[0].kind == Kind::Release, "contact bounce produced another press");
    r.advance(100);
    r.input({Type::Press, "stomp1"});
    r.input({Type::Press, "stomp1"});
    out = r.advance(5000);
    check(out.size() == 1 && out[0].kind == Kind::Press, "repeated press or stomp hold produced extra gestures");
    check(ControlLayout{}.ids() == std::vector<std::string>{"enc1", "enc2", "stomp1", "stomp2", "stomp3"}, "default surface is not two encoders and three stomps");
    check(control_label("enc2") == "Encoder 2" && control_label("stomp3") == "Stomp 3", "control names wrong");
}

void mapping() {
    const Parameter level{"level", "Level", "%", ParameterKind::Continuous, 0, 100, 50};
    check(near(turned_value(level, 50, 1, false, 1000), 52) && near(turned_value(level, 50, -1, false, 1000), 48), "one detent is not 2% of the range");
    check(near(turned_value(level, 50, 1, true, 1000), 50.2), "fine step is not 0.2%");
    check(near(turned_value(level, 50, 1, false, 20), 60) && near(turned_value(level, 50, 1, true, 20), 50.2), "acceleration wrong, or applied to fine steps");
    check(acceleration(10, false) == 5 && acceleration(40, false) == 3 && acceleration(80, false) == 2 && acceleration(500, false) == 1, "acceleration not bounded as specified");
    check(near(turned_value(level, 99, 5, false, 1000), 100) && near(turned_value(level, 1, -5, false, 1000), 0), "value left its range");
    // Logarithmic parameters move evenly in their own scale.
    const Parameter tone{"tone", "Tone", "Hz", ParameterKind::Continuous, 200, 8000, 2400, true};
    const double up = turned_value(tone, 1000, 1, false, 1000), down = turned_value(tone, 1000, -1, false, 1000);
    check(up > 1000 && down < 1000 && near(up / 1000, 1000 / down), "logarithmic steps are not even ratios");
    const Parameter pitch{"pitch", "Pitch", "st", ParameterKind::Integer, -24, 24, 0};
    check(turned_value(pitch, 0, 1, false, 1000) == 1 && turned_value(pitch, 0, 1, false, 10) == 5, "integer steps or wide-range acceleration wrong");
    const Parameter small{"voices", "Voices", "", ParameterKind::Integer, 1, 8, 4};
    check(turned_value(small, 4, 1, false, 10) == 5, "a small integer range accelerated");
    const Parameter mode{"mode", "Mode", "", ParameterKind::Enumeration, 0, 2, 0, false, {"a", "b", "c"}};
    check(turned_value(mode, 0, 1, false, 10) == 1 && turned_value(mode, 2, 1, false, 10) == 2 && turned_value(mode, 0, -1, false, 10) == 0, "choices wrapped or skipped");
    const Parameter freeze{"freeze", "Freeze", "", ParameterKind::Toggle, 0, 1, 0};
    check(turned_value(freeze, 0, 1, false, 10) == 1 && turned_value(freeze, 1, -3, false, 10) == 0, "toggle turning wrong");
    // An assignment narrows and may invert the range.
    Assignment narrow; narrow.control = "enc1"; narrow.parameter = "level"; narrow.minimum = 20; narrow.maximum = 40;
    check(near(turned_value(level, 50, -1, false, 1000, &narrow), 39.6), "a value outside the assigned range did not come into it");
    check(near(turned_value(level, 30, 1, false, 1000, &narrow), 30.4) && near(turned_value(level, 40, 3, false, 1000, &narrow), 40), "narrowed range not used");
    narrow.inverted = true;
    check(near(turned_value(level, 30, 1, false, 1000, &narrow), 29.6), "inversion ignored");
    // Stomp switches: toggle between the range's ends, or hold while pressed.
    Assignment flip; flip.control = "stomp1"; flip.parameter = "freeze"; flip.minimum = 0; flip.maximum = 1;
    check(switched_value(freeze, 0, flip, true) == 1.0 && switched_value(freeze, 1, flip, true) == 0.0 && !switched_value(freeze, 1, flip, false), "toggle switching wrong");
    flip.mode = SwitchMode::Momentary;
    check(switched_value(freeze, 0, flip, true) == 1.0 && switched_value(freeze, 1, flip, false) == 0.0, "momentary switching wrong");
    flip.inverted = true;
    check(switched_value(freeze, 1, flip, true) == 0.0 && switched_value(freeze, 0, flip, false) == 1.0, "inverted momentary wrong");
    Assignment boost; boost.control = "stomp2"; boost.parameter = "level"; boost.minimum = 30; boost.maximum = 90;
    check(switched_value(level, 30, boost, true) == 90.0 && switched_value(level, 90, boost, true) == 30.0 && switched_value(level, 50, boost, true) == 90.0, "toggle between range ends wrong");
    // Soft takeover: an absolute control takes over only once it reaches the value.
    Pickup pickup;
    check(!pickup.move(0.5, 0.1) && !pickup.move(0.5, 0.3), "a far control took over at once");
    check(pickup.move(0.5, 0.6) == 0.6 && pickup.move(0.6, 0.2) == 0.2, "crossing the value did not take over, or control was lost again");
    pickup.release();
    check(!pickup.move(0.2, 0.9) && pickup.move(0.2, 0.205) == 0.205, "takeover did not reset, or a matching position was refused");
}

void assignments(const std::filesystem::path& root) {
    Assignment a; a.control = "enc2"; a.module = 4; a.parameter = "mix"; a.minimum = 10; a.maximum = 90; a.inverted = true; a.plugin = "urn:peepo:demo:clouds";
    const auto text = encode_assignment(a, "m4");
    Assignment back; std::string block;
    check(decode_assignment("enc2", text, back, block) && block == "m4" && back.parameter == "mix" && back.minimum == 10 && back.maximum == 90 && back.inverted &&
          back.plugin == a.plugin && back.action == ControlAction::Parameter, "assignment did not survive encoding");
    check(!decode_assignment("enc2", "parameter|m4|x|mix|90|10|0|toggle", back, block) && !decode_assignment("Bad id", text, back, block) && !decode_assignment("enc2", "junk", back, block),
          "malformed assignment accepted");
    // A Loo-pi command on a stomp: Multiply (7) to loop 2 (target 3), held.
    Assignment command; command.control = "stomp2"; command.module = 5; command.action = ControlAction::Looper; command.mode = SwitchMode::Momentary;
    command.parameter = "7"; command.minimum = command.maximum = 3; command.plugin = "https://github.com/ABustos2606/loo-pi#loopi";
    check(decode_assignment("stomp2", encode_assignment(command, "m5"), back, block) && back.action == ControlAction::Looper &&
          looper_assignment_command(back) == 7 && looper_assignment_target(back) == 3, "Loo-pi command did not survive encoding");
    auto scratch = command; scratch.parameter = "4"; auto far = command; far.minimum = far.maximum = 10; auto word = command; word.parameter = "7x";
    check(!looper_assignment_command(scratch) && !looper_assignment_command(far) && !looper_assignment_command(word), "unsupported Loo-pi command or target accepted");

    // Assignments are part of the patch: validated, replaced per control, and gone with their module.
    SimulatedEngine engine;
    auto run = [&](Command c) { engine.submit(std::move(c)); return engine.advance(100).at(0); };
    check(run(Assign{a}).accepted && engine.snapshot().assignments.size() == 1 && engine.snapshot().dirty, "assignment not accepted or not an edit");
    check(engine.snapshot().assignments[0].plugin == "urn:peepo:demo:clouds", "assignment did not record its plugin");
    auto over = a; over.minimum = -5;
    check(!run(Assign{over}).accepted, "out-of-range assignment accepted");
    Assignment bypass; bypass.control = "stomp1"; bypass.module = 3; bypass.action = ControlAction::Bypass;
    check(run(Assign{bypass}).accepted && engine.snapshot().assignments.size() == 2, "bypass assignment refused");
    auto moved = a; moved.parameter = "position"; moved.minimum = 0; moved.maximum = 100;
    check(run(Assign{moved}).accepted && engine.snapshot().assignments.size() == 2 && engine.snapshot().assignments.back().parameter == "position", "reassignment kept the old target");
    check(!run(Assign{Assignment{"enc1", 99, "drive"}}).accepted && !run(Unassign{"enc4"}).accepted, "assignment to nothing, or clearing nothing, accepted");

    // Saved and loaded with the patch, through the same bundle as everything else.
    PatchStore store(root);
    check(store.open().empty(), "store did not open");
    std::string issue;
    auto staging = store.stage("p-controls", issue);
    check(run(SaveGraph{SaveScope::Persistent, false, staging->location, "p-controls"}).accepted && store.publish(*staging).empty(), "patch with assignments not saved");
    run(NewPatch{"Blank"});
    check(engine.snapshot().assignments.empty(), "assignments survived a new patch");
    const auto stored = store.find("p-controls");
    check(run(LoadPatch{stored->location, saved_blocks(stored->bundle), stored->id}).accepted && engine.snapshot().assignments.size() == 2, "assignments not loaded");
    const auto& loaded = engine.snapshot();
    check(assignment_target(loaded, loaded.assignments[0]) && assignment_target(loaded, loaded.assignments[1]), "loaded assignments orphaned");
    // A module that is removed takes its assignments along.
    run(RemoveModule{3});
    check(engine.snapshot().assignments.size() == 1 && engine.snapshot().assignments[0].control == "enc2", "removed module left its assignment behind");
    // A different plugin under the same identity is never controlled.
    auto graph = engine.snapshot();
    graph.assignments[0].plugin = "urn:peepo:demo:delay";
    check(!assignment_target(graph, graph.assignments[0]) && validate_graph(graph).empty(), "orphan treated as live, or refused in a graph");

    // Instrument-wide stomp actions live beside the patches.
    check(store.global_controls().empty() && store.set_global_control("stomp3", "next-patch").empty() && store.set_global_control("stomp2", "previous-patch").empty(), "global action not stored");
    check(store.global_controls().at("stomp3") == "next-patch" && store.global_controls().size() == 2, "global actions not read back");
    check(!store.set_global_control("stomp1", "launch-rockets").empty() && store.set_global_control("stomp2", "").empty() && store.global_controls().size() == 1, "invalid action accepted or clearing failed");
}
}
void tap_tempo() {
    TapTempo t;
    check(!t.tap(1000), "one tap gave a tempo");
    check(t.tap(1500) == 120, "two taps half a second apart are not 120 BPM");
    check(t.tap(2000) == 120 && t.tap(2600) && *t.tap(3100) > 110 && *t.tap(3100 + 500) < 120, "the latest taps are not averaged");
    t.reset();
    check(!t.tap(10000) && t.tap(10400) == 150, "reset did not start again");
    // A long pause starts again, rather than giving a very slow tempo.
    check(!t.tap(10400 + 2001 + 1000) && t.taps() == 1, "a pause longer than the slowest tempo was counted");
    // Bounce-fast taps are not a tempo, and a time going backwards starts again.
    check(!t.tap(13401 + 100) && !t.tap(13000) && t.taps() == 1, "an impossible tap gave a tempo");
}
int main() {
    try {
        tap_tempo();
        const auto root = std::filesystem::temp_directory_path() / "peepo-controls-tests";
        std::filesystem::remove_all(root);
        gestures();
        mapping();
        assignments(root);
        std::filesystem::remove_all(root);
        std::cout << "Control gestures, mapping rules, soft takeover and stored assignments passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
