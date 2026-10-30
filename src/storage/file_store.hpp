// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace peepo {
// Files plugins read, kept on the instrument in one folder per kind:
//
//   <root>/nam/          amp models (.nam)
//   <root>/ir/cab/       cabinet impulse responses (.wav, .flac)
//   <root>/ir/reverb/    reverb impulse responses (.wav, .flac)
//
// They arrive from a USB stick (file_import.hpp) or by hand over SSH; the
// interface lists them and gives a plugin the chosen one's path.
struct StoredFile {
    std::string name;
    std::filesystem::path path;
    // The same file as the engine sees it.
    std::string engine_path;
};
class FileStore {
public:
    // `engine_root` names the same directory on the engine's machine (/mnt/c/…
    // for Ingen in WSL); empty means the same path.
    explicit FileStore(std::filesystem::path root, std::string engine_root = {});
    // Create the root and the known folders. Returns what is wrong, or "".
    std::string open();
    bool usable() const { return usable_; }
    const std::filesystem::path& root() const { return root_; }
    // The files of one folder (`nam`, `ir/cab`) with one of the extensions
    // (lowercase, without the dot), by name. Hidden files and names a path
    // cannot carry are left out.
    std::vector<StoredFile> list(const std::string& folder, const std::vector<std::string>& extensions) const;
    static constexpr std::size_t max_listed = 500;
    // Delete one file `list` gave. A saved patch keeps its own copy, so none
    // stops loading. Returns what went wrong, or "".
    std::string remove(const StoredFile& file) const;
private:
    std::filesystem::path root_;
    std::string engine_root_;
    bool usable_ = false;
};
// The file name at the end of a path, either separator.
std::string file_name(const std::string& path);
}
