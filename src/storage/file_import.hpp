// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "storage/file_store.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace peepo {
// Files brought to the instrument from a USB stick (later the upload page):
// what each one is, whether a plugin can read it, and a copy into the file
// store that is never seen half written.
//
// Models go to nam/. An impulse response goes to ir/cab/ when a folder above
// it on the stick has "cab" in its name, to ir/reverb/ for "reverb" or
// "room"; any other is Unsorted, and the player says which for the lot.
enum class FileKind { Model, Cabinet, Reverb, Unsorted };
// The store's folder for a kind; nullptr for Unsorted.
const char* store_folder(FileKind kind);

struct FileCheck {
    // Why a plugin could not use it, said for the screen; "" when it can.
    std::string problem;
    // An impulse response's length and channels.
    double seconds = 0;
    unsigned channels = 0;
};
constexpr std::uintmax_t max_model_bytes = 10u * 1024 * 1024;
constexpr std::uintmax_t max_impulse_bytes = 128u * 1024 * 1024;
constexpr double max_impulse_seconds = 30;
constexpr unsigned max_impulse_channels = 4;
// A model: JSON with "architecture" and "weights", at most 10 MB. Read
// without building the tree: a large model's weights would take ten times
// their size in memory.
FileCheck check_model(const std::filesystem::path& path);
// A WAV (PCM or float) or FLAC impulse response: a readable header, 1-4
// channels, under 30 s. Only the header is read.
FileCheck check_impulse(const std::filesystem::path& path);
// Under half a second sounds like a cabinet; the page may suggest it, the
// player decides.
inline bool sounds_like_cabinet(double seconds) { return seconds > 0 && seconds < 0.5; }

struct FoundFile {
    std::filesystem::path source;
    // The file's own name; subfolders are flattened into the store's folder.
    std::string name;
    // The folder it was in, relative to the stick, "" at the top.
    std::string folder;
    FileKind kind = FileKind::Model;
    std::uintmax_t size = 0;
    FileCheck check;
};
struct FoundFiles {
    // Usable files by name, then the refused ones with check.problem.
    std::vector<FoundFile> files;
    std::vector<FoundFile> rejected;
    // The stick held more than was looked at.
    bool truncated = false;
};
constexpr std::size_t max_found = 1000;
constexpr int max_folder_depth = 8;
// Every .nam, .wav and .flac under `root`, checked. Hidden files and folders
// (macOS's ._ files among them), the Windows system folders and links are
// passed over; other files are not mentioned.
FoundFiles find_files(const std::filesystem::path& root);

enum class ImportResult { Copied, Skipped, Failed };
struct Imported {
    ImportResult result = ImportResult::Failed;
    // The name it has on Peepo: its own, or with " (2)" when a different file
    // already has that name.
    std::string name;
    std::string message;
};
// Room left on the card after an import, for patches and the system.
constexpr std::uintmax_t import_reserve_bytes = 64u * 1024 * 1024;
// Copy one found file into the store as `kind` (the player's answer for an
// Unsorted one). A file with the same name and size is already there and is
// skipped. The copy goes to a hidden temporary name, is checked again,
// flushed and renamed, so a pulled stick or a power cut leaves nothing in the
// folder. `stop`, when set, ends the copy between blocks.
Imported import_file(const FileStore& store, const FoundFile& file, FileKind kind, const std::atomic<bool>* stop = nullptr);
// The calling thread reads and writes only when nothing else does (Linux's
// idle I/O class), so an import never holds up the engine loading a patch.
// False where it is not available.
bool set_idle_io_priority();

// A USB stick as peepo-usb left it: `<run>/<device>.info` says how it went,
// and a mounted one is read at `<run>/<device>/`.
struct UsbStick {
    enum class State { Mounted, Unreadable, Refused };
    std::string device, label, type, message;
    State state = State::Unreadable;
    std::filesystem::path root;
    bool operator==(const UsbStick&) const = default;
};
// The sticks in `run` (/run/peepo-usb), by device name; none when it is missing.
std::vector<UsbStick> list_usb_sticks(const std::filesystem::path& run);

// A stick looked through, then imported, on a thread of its own at idle I/O
// priority, so the screen never waits on a slow stick. One job at a time; the
// interface asks how it is going.
class ImportWorker {
public:
    ImportWorker() = default;
    ImportWorker(const ImportWorker&) = delete;
    ImportWorker& operator=(const ImportWorker&) = delete;
    // Stops a job in progress and waits for it.
    ~ImportWorker();
    struct Item { FoundFile file; FileKind kind; };
    struct Status {
        enum class Job { None, Scan, Import };
        Job job = Job::None;
        bool busy = false;
        // The stick looked through, and what was found on it.
        std::filesystem::path root;
        FoundFiles found;
        // An import: how far it is, the file being copied, and how it went.
        std::size_t done = 0, total = 0;
        std::string current;
        std::size_t copied = 0, skipped = 0;
        std::vector<Imported> failed;
        bool stopped = false;
    };
    // Look through a stick, forgetting the last import.
    void scan(std::filesystem::path root);
    // Copy these files into the store, each as its kind.
    void import(FileStore store, std::vector<Item> items);
    // End the job in progress between files, or within a copy.
    void stop();
    // Changes whenever the status does, so a caller reads it only then.
    unsigned serial() const { return serial_.load(); }
    Status status() const;
private:
    // The job before ends first, so its last word never lands on the next.
    void finish();
    void start(std::function<void()> job);
    mutable std::mutex mutex_;
    Status status_;
    std::atomic<unsigned> serial_{0};
    std::atomic<bool> stop_{false};
    std::thread thread_;
};
}
