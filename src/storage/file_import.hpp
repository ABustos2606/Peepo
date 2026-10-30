// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "storage/file_store.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
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
}
