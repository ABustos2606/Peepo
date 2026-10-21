// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
// peepo-midi: the instrument's own MIDI, apart from the interface, which can
// restart while it plays.
//   - Clock out: 24 messages per beat at the patch tempo on the JACK MIDI port
//     peepo-midi:clock_out, with Start and Stop unless they are switched off.
//   - Clock in: with the tempo taken from incoming clock, the patch tempo
//     follows the clock heard on peepo-midi:midi_in, and the clock out stops.
//   - Program change on the chosen channel loads stored patch number N, in
//     the order Patches lists them, until the set list exists.
//   - A patch's own control changes go out on clock_out as it loads, and
//     again when they are edited.
// It is a second engine client: a tap, the Tempo page and a patch change all
// reach it, and nothing the interface does can interrupt it. Settings come
// from the file the interface writes (Settings > MIDI), read again when they
// change; what it hears goes to its runtime folder for the interface.
//   peepo-midi [--engine unix:///run/peepo/ingen.sock] [--name peepo-midi]
//              [--settings /var/lib/peepo/midi.conf] [--status /run/peepo-midi]
//              [--patches /var/lib/peepo/patches] [--catalogue FILE]
#include "catalogue/catalogue.hpp"
#include "engine/ingen/ingen_engine.hpp"
#include "midi/clock.hpp"
#include "midi/settings.hpp"
#include "storage/patch_store.hpp"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>

namespace {
std::atomic<bool> quit{false}, server_gone{false};
constexpr double no_tempo = std::numeric_limits<double>::quiet_NaN();
struct Shared {
    jack_client_t* client = nullptr;
    jack_port_t *out = nullptr, *in = nullptr;
    peepo::midi::ClockOut* clock = nullptr;
    peepo::midi::ClockIn* heard = nullptr;
    // JACK's frame time, which wraps after a day, carried on in 64 bits. A
    // period this program misses still counts, so incoming clock keeps time.
    std::uint64_t frames = 0;
    std::optional<jack_nframes_t> period_start;
    bool restarting = false;
    // From the main thread to the audio thread.
    std::atomic<double> tempo{peepo::default_tempo};
    std::atomic<bool> start{false}, stop{false}, restart{false}, transport{true};
    // Control changes to send: the main thread writes ahead, the audio thread follows.
    std::array<std::array<std::uint8_t, 3>, 64> outgoing{};
    std::atomic<unsigned> written{0}, sent{0};
    std::atomic<int> program_channel{peepo::midi::program_off};
    // From the audio thread to the main thread.
    std::atomic<bool> stopped{false};
    std::atomic<double> clock_in{no_tempo};
    std::atomic<int> program{-1};
    std::atomic<unsigned> lost{0};
};
int process(jack_nframes_t frames, void* data) {
    auto& s = *static_cast<Shared*>(data);
    const auto start = jack_last_frame_time(s.client);
    if (s.period_start) s.frames += static_cast<jack_nframes_t>(start - *s.period_start);
    s.period_start = start;
    // What came in: clock for the tempo, and program changes.
    void* input = jack_port_get_buffer(s.in, frames);
    const auto channel = s.program_channel.load(std::memory_order_relaxed);
    for (std::uint32_t i = 0, count = jack_midi_get_event_count(input); i < count; ++i) {
        jack_midi_event_t event;
        if (jack_midi_event_get(&event, input, i) != 0 || event.size == 0) continue;
        s.heard->message(s.frames + event.time, event.buffer[0]);
        if (const auto program = peepo::midi::program_change(event.buffer, event.size, channel)) s.program = *program;
    }
    s.clock_in.store(s.heard->tempo(s.frames + frames).value_or(no_tempo), std::memory_order_relaxed);
    // What goes out: the clock.
    void* buffer = jack_port_get_buffer(s.out, frames);
    jack_midi_clear_buffer(buffer);
    // A patch's control changes first, at the start of the period.
    for (auto at = s.sent.load(std::memory_order_relaxed); at != s.written.load(std::memory_order_acquire); ++at) {
        if (jack_midi_event_write(buffer, 0, s.outgoing[at % s.outgoing.size()].data(), 3) != 0) s.lost.fetch_add(1, std::memory_order_relaxed);
        s.sent.store(at + 1, std::memory_order_release);
    }
    s.clock->set_tempo(s.tempo.load(std::memory_order_relaxed));
    s.clock->set_transport(s.transport.load(std::memory_order_relaxed));
    if (s.start.exchange(false)) s.clock->start();
    if (s.stop.exchange(false)) { s.clock->stop(); s.restarting = false; }
    // Restart: Stop in this period, Start in the next.
    if (s.restart.exchange(false) && s.clock->running()) { s.clock->stop(); s.restarting = true; }
    else if (s.restarting && !s.clock->running()) { s.clock->start(); s.restarting = false; }
    const bool was_running = s.clock->running();
    s.clock->process(frames, [&](std::uint32_t offset, std::uint8_t status) {
        if (jack_midi_event_write(buffer, offset, &status, 1) != 0) s.lost.fetch_add(1, std::memory_order_relaxed);
    });
    if (was_running && !s.clock->running() && !s.restarting) s.stopped = true;
    return 0;
}
void on_shutdown(void*) { server_gone = true; }
// The MIDI devices a2jmidid exports: those that send (a JACK output) and those that receive.
peepo::midi::Status devices(jack_client_t* client) {
    peepo::midi::Status status;
    if (const char** ports = jack_get_ports(client, "^a2j:", JACK_DEFAULT_MIDI_TYPE, 0)) {
        for (const char** p = ports; *p; ++p) {
            if (std::strstr(*p, "Midi Through")) continue;
            const auto* port = jack_port_by_name(client, *p);
            if (!port) continue;
            (jack_port_flags(port) & JackPortIsOutput ? status.inputs : status.outputs).push_back(peepo::midi::device_name(*p));
        }
        jack_free(ports);
    }
    return status;
}
std::filesystem::path beside_program(const std::string& name) {
    const auto folder = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    if (std::filesystem::exists(folder / name)) return folder / name;
    return folder / ".." / "share" / "peepo" / name;
}
}
int main(int argc, char** argv) {
    std::string address = "unix:///run/peepo/ingen.sock", name = "peepo-midi", settings_path, status_path, patches_path, engine_patches, catalogue_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (arg == "--engine") address = value();
        else if (arg == "--name") name = value();
        else if (arg == "--settings") settings_path = value();
        else if (arg == "--status") status_path = value();
        else if (arg == "--patches") patches_path = value();
        else if (arg == "--engine-patches") engine_patches = value();
        else if (arg == "--catalogue") catalogue_path = value();
        else {
            std::cerr << "usage: peepo-midi [--engine ADDRESS] [--name JACK_NAME] [--settings FILE] [--status DIR] [--patches DIR [--engine-patches PATH]] [--catalogue FILE]\n";
            return 2;
        }
    }
    std::signal(SIGTERM, [](int) { quit = true; });
    std::signal(SIGINT, [](int) { quit = true; });

    // The modules, so a tempo from incoming clock reaches every module that
    // follows the tempo, as one set on the Tempo page does.
    if (catalogue_path.empty()) catalogue_path = beside_program("catalogue.json").string();
    std::vector<peepo::ModuleDefinition> offered;
    const auto overlay = std::filesystem::path(catalogue_path).parent_path() / "catalogue-overlay.json";
    if (const auto issue = peepo::load_catalogue(catalogue_path, std::filesystem::exists(overlay) ? overlay.string() : std::string(), offered); issue.empty())
        peepo::replace_catalogue(std::move(offered));
    else std::cerr << "peepo-midi: catalogue: " << issue << "; modules will not follow incoming clock\n";
    const peepo::midi::Link link(settings_path, status_path);
    std::unique_ptr<peepo::PatchStore> store;
    if (!patches_path.empty()) {
        store = std::make_unique<peepo::PatchStore>(patches_path, engine_patches);
        if (const auto issue = store->open_to_read(); !issue.empty()) std::cerr << "peepo-midi: patches: " << issue << "\n";
    }

    jack_status_t status{};
    jack_client_t* client = jack_client_open(name.c_str(), JackNoStartServer, &status);
    if (!client) { std::cerr << "peepo-midi: JACK is not running (status " << status << ")\n"; return 1; }
    peepo::midi::ClockOut clock(jack_get_sample_rate(client));
    peepo::midi::ClockIn heard(jack_get_sample_rate(client));
    Shared shared;
    shared.client = client;
    shared.clock = &clock;
    shared.heard = &heard;
    shared.out = jack_port_register(client, "clock_out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    shared.in = jack_port_register(client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    if (!shared.out || !shared.in) { std::cerr << "peepo-midi: cannot register its ports\n"; jack_client_close(client); return 1; }
    jack_set_process_callback(client, process, &shared);
    jack_on_shutdown(client, on_shutdown, nullptr);
    if (jack_activate(client) != 0) { std::cerr << "peepo-midi: cannot activate\n"; jack_client_close(client); return 1; }
    const std::string self = jack_get_client_name(client);
    std::cout << "peepo-midi: " << self << ":clock_out and " << self << ":midi_in at " << jack_get_sample_rate(client) << " Hz; engine " << address << std::endl;

    peepo::IngenEngine engine(peepo::ingen::Options{address});
    engine.connect();
    // The clock begins once the engine has said what the tempo is, so a
    // follower never hears a guess. When the engine goes away the clock keeps
    // the last tempo and goes on.
    bool told_tempo = false, clocking = false;
    double told = 0;
    peepo::midi::Settings settings = link.settings();
    std::string settings_text = peepo::midi::format_settings(settings);
    peepo::midi::Status written;
    bool status_written = false;
    unsigned restarts = link.restart_requests();
    peepo::RequestId following = 0, loading = 0;
    // The patch and control changes last sent, so a patch's are sent once as it loads.
    std::optional<std::string> sent_for;
    auto last = std::chrono::steady_clock::now(), checked = last - std::chrono::seconds(1), followed = last;
    std::cout << "peepo-midi: settings " << (settings_path.empty() ? "(defaults)" : settings_path) << std::endl;
    while (!quit && !server_gone) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count());
        if (elapsed) last = now;
        for (const auto& event : engine.advance(elapsed)) {
            if (event.request && (event.request == following || event.request == loading) && !event.accepted)
                std::cerr << "peepo-midi: " << (event.request == loading ? "program change: " : "tempo from clock: ") << event.message << std::endl;
            if (event.request == following) following = 0;
            if (event.request == loading) loading = 0;
        }
        if (const auto lost = shared.lost.exchange(0)) std::cerr << "peepo-midi: " << lost << " clock messages did not fit their period" << std::endl;
        const bool ready = engine.state() == peepo::EngineState::Ready;
        if (ready) {
            const double tempo = engine.snapshot().tempo;
            shared.tempo = tempo;
            told_tempo = true;
            if (tempo != told) { std::cout << "peepo-midi: tempo " << tempo << " BPM" << std::endl; told = tempo; }
        }
        // A patch's control changes, as it loads or when they change.
        if (ready) {
            const auto& graph = engine.snapshot();
            const auto key = graph.patch_id + " " + peepo::encode_control_changes(graph.load_messages);
            if (key != sent_for) {
                sent_for = key;
                for (const auto& m : graph.load_messages) {
                    const auto at = shared.written.load(std::memory_order_relaxed);
                    if (at - shared.sent.load(std::memory_order_acquire) >= shared.outgoing.size()) break;
                    shared.outgoing[at % shared.outgoing.size()] = {static_cast<std::uint8_t>(0xb0 | (m.channel - 1)), static_cast<std::uint8_t>(m.number), static_cast<std::uint8_t>(m.value)};
                    shared.written.store(at + 1, std::memory_order_release);
                }
                if (!graph.load_messages.empty()) std::cout << "peepo-midi: sent " << graph.load_messages.size() << " control changes for " << graph.name << std::endl;
            }
        }
        // Several times a second: the settings, the devices, a restart asked for.
        if (now - checked >= std::chrono::milliseconds(250)) {
            checked = now;
            const auto latest = link.settings();
            if (const auto text = peepo::midi::format_settings(latest); text != settings_text) {
                settings = latest; settings_text = text;
                std::cout << "peepo-midi: settings changed" << std::endl;
            }
            peepo::midi::Status heard_now = devices(client);
            if (const double in = shared.clock_in.load(); std::isfinite(in)) heard_now.clock_in = std::round(in * 10) / 10;
            if (!status_written || !(heard_now == written)) { link.write_status(heard_now); written = heard_now; status_written = true; }
            if (const auto asked = link.restart_requests(); asked != restarts) {
                restarts = asked;
                if (clocking) { shared.restart = true; std::cout << "peepo-midi: clock restarted" << std::endl; }
            }
        }
        shared.transport = settings.start_stop;
        shared.program_channel = settings.program_channel;
        // Clock out only when the tempo is Peepo's own: following another
        // clock and sending one could make a loop.
        const bool want = told_tempo && settings.clock_out && settings.tempo_source == peepo::midi::TempoSource::Patch;
        if (want && !clocking) { shared.start = true; clocking = true; std::cout << "peepo-midi: clock out on" << std::endl; }
        if (!want && clocking) { shared.stop = true; clocking = false; std::cout << "peepo-midi: clock out off" << std::endl; }
        // Following incoming clock, in tenths of a BPM, a few times a second.
        if (const double in = shared.clock_in.load(); ready && settings.tempo_source == peepo::midi::TempoSource::Clock && std::isfinite(in) && !following &&
            now - followed >= std::chrono::milliseconds(200)) {
            const double bpm = std::clamp(std::round(in * 10) / 10, peepo::minimum_tempo, peepo::maximum_tempo);
            if (std::abs(bpm - engine.snapshot().tempo) >= 0.2) { following = engine.submit(peepo::SetTempo{bpm, true}); followed = now; }
        }
        // Program change: the newest one wins; a patch already playing unedited stays.
        if (const int program = shared.program.exchange(-1); program >= 0 && ready) {
            std::vector<peepo::StoredPatch> loadable;
            if (store && store->usable()) for (auto& p : store->list()) if (p.revision) loadable.push_back(std::move(p));
            if (static_cast<std::size_t>(program) >= loadable.size()) std::cerr << "peepo-midi: program " << program + 1 << ": only " << loadable.size() << " patches are stored" << std::endl;
            else if (const auto& p = loadable[program]; p.id == engine.snapshot().patch_id && !engine.snapshot().dirty) {}
            else {
                std::cout << "peepo-midi: program " << program + 1 << " loads " << p.name << std::endl;
                loading = engine.submit(peepo::LoadPatch{p.location, peepo::saved_blocks(p.bundle), p.id, p.bundle.tempo});
            }
        }
    }
    if (server_gone) { std::cerr << "peepo-midi: JACK went away" << std::endl; return 1; }
    // Stop, so a follower is not left waiting for the next clock.
    if (clocking) {
        shared.stop = true;
        for (int i = 0; i < 250 && !shared.stopped; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    link.write_status({});
    jack_deactivate(client);
    jack_client_close(client);
    std::cout << "peepo-midi: stopped" << std::endl;
    return 0;
}
