// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "storage/patch_store.hpp"
#include "engine/ingen/turtle.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace peepo {
namespace fs = std::filesystem;
namespace {
std::string revision_name(unsigned revision) {
    char buffer[24];
    std::snprintf(buffer, sizeof buffer, "r%06u.ingen", revision);
    return buffer;
}
// r000007.ingen → 7; anything else, including staging, → 0.
unsigned revision_of(const std::string& name) {
    if (name.size() != 13 || name[0] != 'r' || !name.ends_with(".ingen")) return 0;
    unsigned value = 0;
    for (std::size_t i = 1; i < 7; ++i) {
        if (name[i] < '0' || name[i] > '9') return 0;
        value = value * 10 + static_cast<unsigned>(name[i] - '0');
    }
    return value;
}
std::optional<std::string> slurp(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}
// A file IRI's path, with its escapes undone: state names a saved model
// `<A1%20standard.nam>`. A malformed escape is kept as it is.
std::string unescaped(const std::string& iri) {
    std::string out;
    for (std::size_t i = 0; i < iri.size(); ++i) {
        auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        if (iri[i] == '%' && i + 2 < iri.size() && hex(iri[i + 1]) >= 0 && hex(iri[i + 2]) >= 0) {
            out += static_cast<char>(hex(iri[i + 1]) * 16 + hex(iri[i + 2]));
            i += 2;
        } else out += iri[i];
    }
    return out;
}
// Pull the power right after a save and the card may not have it yet: a
// rename is atomic, but only what was flushed survives. Flush each file, then
// the directory that names it. Windows (the simulator) is left to itself.
// False when the card may not have it.
bool flush(const fs::path& path) {
#ifndef _WIN32
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    return ::close(fd) == 0 && synced;
#else
    (void)path;
    return true;
#endif
}
bool flush_tree(const fs::path& directory) {
    std::error_code error;
    bool all = true;
    for (fs::recursive_directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) all = flush(it->path()) && all;
    return !error && flush(directory) && all;
}
std::string lower(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return text;
}
}

bool engine_path(const std::string& path) {
    // POSIX /a/b, or a Windows drive C:/a/b for the simulator.
    const bool drive = path.size() > 2 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':' && path[2] == '/';
    if (path.empty() || (path[0] != '/' && !drive) || path.size() > 512) return false;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' || c == '-';
        if (!plain && !(drive && i == 1)) return false;
    }
    return path.find("/../") == std::string::npos && !path.ends_with("/..");
}
std::string wsl_path(const fs::path& path) {
    // Backslashes by hand: only a Windows build treats them as separators.
    auto text = path.generic_string();
    std::replace(text.begin(), text.end(), '\\', '/');
    if (text.size() > 2 && text[1] == ':' && text[2] == '/') {
        const char drive = static_cast<char>(text[0] >= 'A' && text[0] <= 'Z' ? text[0] - 'A' + 'a' : text[0]);
        return std::string("/mnt/") + drive + text.substr(2);
    }
    return text;
}

PatchStore::PatchStore(fs::path root, std::string engine_root) : root_(std::move(root)), engine_root_(std::move(engine_root)) {
    if (engine_root_.empty()) engine_root_ = root_.generic_string();
    while (engine_root_.size() > 1 && engine_root_.back() == '/') engine_root_.pop_back();
}
std::string PatchStore::open() {
    usable_ = false;
    if (!engine_path(engine_root_))
        return "Patch folder " + engine_root_ + " cannot be given to the engine: use letters, digits, '.', '_' and '-' only";
    std::error_code error;
    fs::create_directories(root_, error);
    if (error || !fs::is_directory(root_)) return "Patch folder " + root_.generic_string() + " is not available: " + error.message();
    // Anything still in staging belongs to a save that never finished.
    for (const auto& entry : fs::directory_iterator(root_, error)) {
        const auto name = entry.path().filename().string();
        if (name.starts_with(".trash-")) fs::remove_all(entry.path(), error);
        if (!entry.is_directory() || !valid_patch_id(name)) continue;
        for (const auto& inner : fs::directory_iterator(entry.path(), error))
            if (inner.path().filename().string().starts_with(".staging-")) fs::remove_all(inner.path(), error);
    }
    usable_ = true;
    return {};
}
std::string PatchStore::open_to_read() {
    usable_ = false;
    if (!engine_path(engine_root_))
        return "Patch folder " + engine_root_ + " cannot be given to the engine: use letters, digits, '.', '_' and '-' only";
    std::error_code error;
    if (!fs::is_directory(root_, error)) return "Patch folder " + root_.generic_string() + " is not available";
    usable_ = true;
    return {};
}
std::vector<unsigned> PatchStore::revisions(const fs::path& patch) const {
    std::vector<unsigned> found;
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(patch, error))
        if (const auto revision = revision_of(entry.path().filename().string()); revision && entry.is_directory()) found.push_back(revision);
    std::sort(found.rbegin(), found.rend());
    return found;
}
std::string PatchStore::location(const std::string& id, const std::string& directory) const {
    const auto path = engine_root_ + "/" + id + "/" + directory;
    return "file://" + std::string(path.starts_with('/') ? "" : "/") + path;
}
std::optional<StoredPatch> PatchStore::read(const std::string& id) const {
    if (!valid_patch_id(id)) return std::nullopt;
    const auto directory = root_ / id;
    std::error_code error;
    if (!fs::is_directory(directory, error)) return std::nullopt;
    StoredPatch patch;
    patch.id = id;
    patch.name = id;
    std::string newest_issue;
    const auto found = revisions(directory);
    // Without any revision this is a first save still being written.
    if (found.empty()) return std::nullopt;
    for (const auto revision : found) {
        const auto path = directory / revision_name(revision);
        std::string issue;
        Bundle bundle;
        if (const auto text = slurp(path / "main.ttl"); !text) issue = "main.ttl is missing";
        else if (issue = read_bundle(*text, bundle); !issue.empty()) {}
        // Without its project properties a bundle is still a usable graph,
        // and its folder says whose it is. Another identity is a misplaced copy.
        else if (!bundle.id.empty() && bundle.id != id) issue = "main.ttl belongs to another patch (" + bundle.id + ")";
        else if (bundle.schema > bundle_schema_version) issue = "saved by a newer Peepo (schema " + std::to_string(bundle.schema) + ")";
        if (issue.empty()) {
            patch.revision = revision;
            patch.path = path;
            patch.location = location(id, revision_name(revision));
            patch.name = validate_patch_name(bundle.name).empty() ? bundle.name : id;
            patch.missing_files = missing_files(path, bundle);
            patch.bundle = std::move(bundle);
            if (!newest_issue.empty()) patch.issue = "Newest revision r" + std::to_string(found.front()) + " is unreadable (" + newest_issue + "); offering r" + std::to_string(revision);
            return patch;
        }
        if (newest_issue.empty()) newest_issue = issue;
    }
    patch.issue = "No readable revision: " + newest_issue;
    return patch;
}
std::vector<std::string> PatchStore::missing_files(const fs::path& revision, const Bundle& bundle) const {
    // Plugin state names its files as LV2 state does: relative to the state
    // file when the file was saved with the bundle, absolute when it was only
    // referenced. Absolute paths are the engine's, so only checked when the
    // engine shares this filesystem under the same names.
    std::vector<std::string> missing;
    const bool shared = engine_root_ == root_.generic_string();
    for (const auto& block : bundle.blocks) {
        if (block.state.empty()) continue;
        const auto label = block.name.empty() ? block.symbol : block.name;
        const auto state = revision / fs::path(std::u8string(block.state.begin(), block.state.end()));
        const auto text = slurp(state);
        if (!text) { missing.push_back(label + ": " + block.state); continue; }
        turtle::Reader reader;
        std::vector<turtle::Triple> triples;
        // A state file this reader cannot follow is the plugin's business.
        if (!reader.parse(*text, triples).empty()) continue;
        for (const auto& triple : triples) {
            if (triple.object.kind != turtle::Term::Kind::Iri || triple.predicate.value == "http://www.w3.org/1999/02/22-rdf-syntax-ns#type" ||
                triple.predicate.value == "http://lv2plug.in/ns/lv2core#appliesTo") continue;
            const auto& reference = triple.object.value;
            fs::path file;
            if (reference.starts_with("file://")) {
                if (!shared) continue;
                const auto path = unescaped(reference.substr(7));
                file = fs::path(std::u8string(path.begin(), path.end()));
            } else if (!reference.empty() && reference.find(':') == std::string::npos) {
                const auto path = unescaped(reference);
                file = state.parent_path() / fs::path(std::u8string(path.begin(), path.end()));
            } else continue;
            std::error_code error;
            if (!fs::exists(file, error)) missing.push_back(label + ": " + reference);
        }
    }
    return missing;
}
std::vector<StoredPatch> PatchStore::list() const {
    std::vector<StoredPatch> out;
    if (!usable_) return out;
    const auto first = startup();
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(root_, error)) {
        if (!entry.is_directory()) continue;
        if (auto patch = read(entry.path().filename().string())) { patch->startup = patch->id == first; out.push_back(std::move(*patch)); }
    }
    std::sort(out.begin(), out.end(), [](const StoredPatch& a, const StoredPatch& b) {
        const auto x = lower(a.name), y = lower(b.name);
        return x != y ? x < y : a.id < b.id;
    });
    return out;
}
std::optional<StoredPatch> PatchStore::find(const std::string& id) const {
    if (!usable_) return std::nullopt;
    auto patch = read(id);
    if (patch) patch->startup = patch->id == startup();
    return patch;
}
std::string PatchStore::new_id() const {
    static std::mt19937 random{std::random_device{}()};
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm parts{};
#ifdef _WIN32
    gmtime_s(&parts, &now);
#else
    gmtime_r(&now, &parts);
#endif
    for (;;) {
        char buffer[40];
        std::snprintf(buffer, sizeof buffer, "p%04d%02d%02d-%02d%02d%02d-%04x", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
                      parts.tm_hour, parts.tm_min, parts.tm_sec, static_cast<unsigned>(random() & 0xffff));
        std::error_code error;
        if (!fs::exists(root_ / buffer, error)) return buffer;
    }
}
std::optional<PatchStore::Staging> PatchStore::stage(const std::string& id, std::string& issue) {
    if (!usable_) { issue = "Patch storage is not available"; return std::nullopt; }
    if (!valid_patch_id(id)) { issue = "Invalid patch identity"; return std::nullopt; }
    const auto directory = root_ / id;
    std::error_code error;
    fs::create_directories(directory, error);
    if (error) { issue = "Could not create " + directory.generic_string() + ": " + error.message(); return std::nullopt; }
    const auto found = revisions(directory);
    Staging staging{id, found.empty() ? 1 : found.front() + 1, {}, {}};
    const auto name = ".staging-" + revision_name(staging.revision);
    staging.path = directory / name;
    fs::remove_all(staging.path, error);
    staging.location = location(id, name);
    return staging;
}
std::string PatchStore::publish(const Staging& staging) {
    std::error_code error;
    if (!fs::is_regular_file(staging.path / "manifest.ttl", error)) return "The engine wrote no manifest.ttl";
    const auto text = slurp(staging.path / "main.ttl");
    if (!text) return "The engine wrote no main.ttl";
    Bundle bundle;
    if (auto issue = read_bundle(*text, bundle); !issue.empty()) return "The saved bundle does not read back: " + issue;
    if (bundle.id != staging.id) return "The saved bundle carries another identity (" + bundle.id + ")";
    const auto directory = staging.path.parent_path();
    const auto final_path = directory / revision_name(staging.revision);
    if (fs::exists(final_path, error)) return "Revision r" + std::to_string(staging.revision) + " already exists";
    if (!flush_tree(staging.path)) return "Could not write the saved revision to the card";
    fs::rename(staging.path, final_path, error);
    if (error) return "Could not publish the saved revision: " + error.message();
    // Published, but a power cut could still lose its name: then every older
    // revision stays, to fall back on.
    if (!flush(directory) || !flush(directory.parent_path())) return {};
    // The new revision is complete; older ones beyond the fallback can go.
    const auto found = revisions(directory);
    for (std::size_t i = kept_revisions; i < found.size(); ++i) fs::remove_all(directory / revision_name(found[i]), error);
    return {};
}
void PatchStore::discard(const Staging& staging) {
    std::error_code error;
    fs::remove_all(staging.path, error);
    // A first save that failed leaves nothing behind.
    if (fs::is_empty(staging.path.parent_path(), error)) fs::remove(staging.path.parent_path(), error);
}
LoadPatch load_command(const StoredPatch& patch, const std::optional<StoredPatch>& playing) {
    LoadPatch load{patch.location, saved_blocks(patch.bundle), patch.id, patch.bundle.tempo};
    if (playing && playing->revision && playing->location != patch.location)
        load.fallback = std::make_shared<const LoadPatch>(LoadPatch{playing->location, saved_blocks(playing->bundle), playing->id, playing->bundle.tempo});
    return load;
}
std::string PatchStore::remove(const std::string& id) {
    if (!usable_ || !valid_patch_id(id)) return "Invalid patch identity";
    std::error_code error;
    const auto directory = root_ / id;
    if (!fs::is_directory(directory, error)) return "Patch is not stored";
    // Out of the list in one step, then deleted at leisure.
    const auto trash = root_ / (".trash-" + id);
    fs::remove_all(trash, error);
    fs::rename(directory, trash, error);
    if (error) return "Could not delete the patch: " + error.message();
    flush(root_);
    fs::remove_all(trash, error);
    if (startup() == id) set_startup({});
    return {};
}
std::map<std::string, std::string> PatchStore::global_controls() const {
    std::map<std::string, std::string> out;
    const auto text = slurp(root_ / "controls");
    if (!text) return out;
    std::istringstream lines(*text);
    for (std::string control, action; lines >> control >> action;)
        if (valid_control_id(control) && (action == "next-patch" || action == "previous-patch" || action == "tap-tempo")) out[control] = action;
    return out;
}
std::string PatchStore::set_global_control(const std::string& control, const std::string& action) {
    if (!usable_) return "Patch storage is not available";
    if (!valid_control_id(control) || (!action.empty() && action != "next-patch" && action != "previous-patch" && action != "tap-tempo")) return "Invalid control action";
    auto all = global_controls();
    if (action.empty()) all.erase(control); else all[control] = action;
    const auto temporary = root_ / ".controls.tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        for (const auto& [id, what] : all) out << id << " " << what << "\n";
        if (!out) return "Could not write the control settings";
    }
    std::error_code error;
    if (!flush(temporary)) { fs::remove(temporary, error); return "Could not write the settings to the card"; }
    fs::rename(temporary, root_ / "controls", error);
    flush(root_);
    return error ? "Could not record the control settings: " + error.message() : std::string();
}
std::string PatchStore::startup() const {
    const auto text = slurp(root_ / "startup");
    if (!text) return {};
    auto id = text->substr(0, text->find_first_of("\r\n"));
    return valid_patch_id(id) ? id : std::string();
}
std::string PatchStore::set_startup(const std::string& id) {
    if (!usable_) return "Patch storage is not available";
    std::error_code error;
    if (id.empty()) { fs::remove(root_ / "startup", error); flush(root_); return error ? error.message() : std::string(); }
    if (!valid_patch_id(id)) return "Invalid patch identity";
    const auto temporary = root_ / ".startup.tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << id << "\n";
        if (!out) return "Could not write the startup choice";
    }
    if (!flush(temporary)) { fs::remove(temporary, error); return "Could not write the settings to the card"; }
    fs::rename(temporary, root_ / "startup", error);
    flush(root_);
    return error ? "Could not record the startup choice: " + error.message() : std::string();
}
}
