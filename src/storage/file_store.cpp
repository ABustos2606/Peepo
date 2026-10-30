// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "storage/file_store.hpp"
#include <algorithm>
#include <cctype>

namespace peepo {
namespace fs = std::filesystem;
namespace {
std::string lower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
bool listable(const std::string& name) {
    if (name.empty() || name[0] == '.' || name.size() > 255) return false;
    return std::none_of(name.begin(), name.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f || c == '/' || c == '\\'; });
}
}
FileStore::FileStore(fs::path root, std::string engine_root) : root_(std::move(root)), engine_root_(std::move(engine_root)) {
    if (engine_root_.empty()) engine_root_ = root_.generic_string();
    while (engine_root_.size() > 1 && engine_root_.back() == '/') engine_root_.pop_back();
}
std::string FileStore::open() {
    std::error_code error;
    for (const char* folder : {"nam", "ir/cab", "ir/reverb"}) {
        fs::create_directories(root_ / folder, error);
        if (error) break;
        // What is left of an import that never finished (file_import.cpp).
        std::error_code ignored;
        for (const auto& entry : fs::directory_iterator(root_ / folder, ignored)) {
            const auto name = entry.path().filename().string();
            if (name.starts_with(".import-") && name.ends_with(".part")) fs::remove(entry.path(), ignored);
        }
    }
    usable_ = !error && fs::is_directory(root_, error);
    return usable_ ? std::string() : "Files folder unavailable: " + root_.generic_string();
}
std::vector<StoredFile> FileStore::list(const std::string& folder, const std::vector<std::string>& extensions) const {
    std::vector<StoredFile> out;
    std::error_code error;
    // Plain names, at most one level deep: nothing outside the store is listed.
    const auto slash = folder.find('/');
    const auto first = folder.substr(0, slash), second = slash == std::string::npos ? std::string() : folder.substr(slash + 1);
    if (!usable_ || !listable(first) || (slash != std::string::npos && !listable(second)) || folder.find("..") != std::string::npos) return out;
    for (const auto& entry : fs::directory_iterator(root_ / first / second, error)) {
        if (out.size() >= max_listed) break;
        if (!entry.is_regular_file(error)) continue;
        const auto u8 = entry.path().filename().u8string();
        const std::string name(u8.begin(), u8.end());
        const auto dot = name.rfind('.');
        if (!listable(name) || dot == std::string::npos) continue;
        if (std::find(extensions.begin(), extensions.end(), lower(name.substr(dot + 1))) == extensions.end()) continue;
        out.push_back({name, entry.path(), engine_root_ + "/" + folder + "/" + name});
    }
    std::sort(out.begin(), out.end(), [](const StoredFile& a, const StoredFile& b) { return lower(a.name) < lower(b.name); });
    return out;
}
std::string FileStore::remove(const StoredFile& file) const {
    std::error_code error;
    // Only a file in one of the store's folders, never anything outside them.
    const auto folder = fs::weakly_canonical(file.path.parent_path(), error);
    bool inside = false;
    for (const char* known : {"nam", "ir/cab", "ir/reverb"})
        if (!error && folder == fs::weakly_canonical(root_ / known, error)) inside = true;
    const auto u8 = file.path.filename().u8string();
    if (!usable_ || !inside || !listable(std::string(u8.begin(), u8.end())) || !fs::is_regular_file(fs::symlink_status(file.path, error)))
        return "That file is no longer on Peepo.";
    if (!fs::remove(file.path, error) || error) return "Could not delete " + std::string(u8.begin(), u8.end()) + ".";
    return {};
}
std::string file_name(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
}
