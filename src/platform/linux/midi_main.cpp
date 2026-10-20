// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// peepo-midi: the instrument's own MIDI, apart from the interface, which can
// restart while it plays. For now, clock out: 24 messages per beat at the
// patch tempo on the JACK MIDI port peepo-midi:clock_out. The tempo is read
// from the engine as a second client, so a tap, the Tempo page and a patch
// change all reach it, and nothing the interface does can interrupt it.
//   peepo-midi [--engine unix:///run/peepo/ingen.sock] [--name peepo-midi]
#include "engine/ingen/ingen_engine.hpp"
#include "midi/clock.hpp"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {
std::atomic<bool> quit{false}, server_gone{false};
struct Shared {
    jack_port_t* port = nullptr;
    peepo::midi::ClockOut* clock = nullptr;
    // From the engine thread to the audio thread.
    std::atomic<double> tempo{peepo::default_tempo};
    std::atomic<bool> start{false}, stop{false}, stopped{false};
    std::atomic<unsigned> lost{0};
};
int process(jack_nframes_t frames, void* data) {
    auto& s = *static_cast<Shared*>(data);
    void* buffer = jack_port_get_buffer(s.port, frames);
    jack_midi_clear_buffer(buffer);
    s.clock->set_tempo(s.tempo.load(std::memory_order_relaxed));
    if (s.start.exchange(false)) s.clock->start();
    if (s.stop.exchange(false)) s.clock->stop();
    const bool was_running = s.clock->running();
    s.clock->process(frames, [&](std::uint32_t offset, std::uint8_t status) {
        if (jack_midi_event_write(buffer, offset, &status, 1) != 0) s.lost.fetch_add(1, std::memory_order_relaxed);
    });
    if (was_running && !s.clock->running()) s.stopped = true;
    return 0;
}
void on_shutdown(void*) { server_gone = true; }
}
int main(int argc, char** argv) {
    std::string address = "unix:///run/peepo/ingen.sock", name = "peepo-midi";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--engine" && i + 1 < argc) address = argv[++i];
        else if (arg == "--name" && i + 1 < argc) name = argv[++i];
        else { std::cerr << "usage: peepo-midi [--engine ADDRESS] [--name JACK_NAME]\n"; return 2; }
    }
    std::signal(SIGTERM, [](int) { quit = true; });
    std::signal(SIGINT, [](int) { quit = true; });

    jack_status_t status{};
    jack_client_t* client = jack_client_open(name.c_str(), JackNoStartServer, &status);
    if (!client) { std::cerr << "peepo-midi: JACK is not running (status " << status << ")\n"; return 1; }
    peepo::midi::ClockOut clock(jack_get_sample_rate(client));
    Shared shared;
    shared.clock = &clock;
    shared.port = jack_port_register(client, "clock_out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    if (!shared.port) { std::cerr << "peepo-midi: cannot register clock_out\n"; jack_client_close(client); return 1; }
    jack_set_process_callback(client, process, &shared);
    jack_on_shutdown(client, on_shutdown, nullptr);
    if (jack_activate(client) != 0) { std::cerr << "peepo-midi: cannot activate\n"; jack_client_close(client); return 1; }
    std::cout << "peepo-midi: " << jack_get_client_name(client) << ":clock_out at " << jack_get_sample_rate(client) << " Hz; engine " << address << std::endl;

    // The clock begins once the engine has said what the tempo is, so a
    // follower never hears a guess. When the engine goes away the clock keeps
    // the last tempo and goes on.
    peepo::IngenEngine engine(peepo::ingen::Options{address});
    engine.connect();
    bool started = false;
    double told = 0;
    auto last = std::chrono::steady_clock::now();
    while (!quit && !server_gone) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count());
        if (elapsed) last = now;
        engine.advance(elapsed);
        if (engine.state() != peepo::EngineState::Ready) continue;
        const double tempo = engine.snapshot().tempo;
        shared.tempo = tempo;
        if (!started) { shared.start = true; started = true; }
        if (tempo != told) { std::cout << "peepo-midi: clock at " << tempo << " BPM" << std::endl; told = tempo; }
        if (const auto lost = shared.lost.exchange(0)) std::cerr << "peepo-midi: " << lost << " clock messages did not fit their period" << std::endl;
    }
    if (server_gone) { std::cerr << "peepo-midi: JACK went away" << std::endl; return 1; }
    // Stop, so a follower is not left waiting for the next clock.
    if (started) {
        shared.stop = true;
        for (int i = 0; i < 250 && !shared.stopped; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    jack_deactivate(client);
    jack_client_close(client);
    std::cout << "peepo-midi: stopped" << std::endl;
    return 0;
}
