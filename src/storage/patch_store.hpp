// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "storage/bundle.hpp"
#include <filesystem>
#include <map>
#include <optional>

namespace peepo {
// Saved patches on disk, one directory per patch identity:
//
//   <root>/<id>/r000007.ingen/           the newest complete revision
//   <root>/<id>/r000006.ingen/           the one before it, kept as a fallback
//   <root>/<id>/.staging-r000008.ingen/  being written; never read as a patch
//   <root>/startup                       the identity to load at startup
//
// The engine writes each revision into staging. Only after it has answered,
// and main.ttl reads back with the right identity, is the directory renamed
// into place: a rename to a new name, which is atomic, so an interrupted save
// leaves the previous revision as it was. See docs/17-patch-storage.md.
struct StoredPatch {
    std::string id, name;
    unsigned revision = 0;
    std::filesystem::path path;
    // The same revision as the engine sees it: file:///…/r000007.ingen
    std::string location;
    Bundle bundle;
    // Set when the newest revision could not be read; then `revision` is
    // the older one actually offered, or 0 when none is readable.
    std::string issue;
    // Files the saved plugin state refers to that are not where it says.
    std::vector<std::string> missing_files;
    bool startup = false;
};
// The command that loads `patch`, falling back on `playing`, the patch
// playing as last stored, when the engine cannot load it.
LoadPatch load_command(const StoredPatch& patch, const std::optional<StoredPatch>& playing);
class PatchStore {
public:
    // `root` as this program sees it; `engine_root` as an absolute path on the
    // engine's machine, the same directory under another name when the engine
    // runs elsewhere (/mnt/c/… for Ingen in WSL). Empty means the same path.
    explicit PatchStore(std::filesystem::path root, std::string engine_root = {});
    // Create the root and clear interrupted saves. Returns what is wrong, or "".
    std::string open();
    // For a program beside the interface, such as peepo-midi: reads only, and
    // clears nothing, since the interface may be saving.
    std::string open_to_read();
    bool usable() const { return usable_; }
    const std::filesystem::path& root() const { return root_; }
    // Every stored patch, by name.
    std::vector<StoredPatch> list() const;
    std::optional<StoredPatch> find(const std::string& id) const;
    // A new identity no stored patch has.
    std::string new_id() const;
    struct Staging { std::string id; unsigned revision = 0; std::filesystem::path path; std::string location; };
    // Where the engine should write the next revision of `id`.
    std::optional<Staging> stage(const std::string& id, std::string& issue);
    // Check what the engine wrote and make it the newest revision.
    std::string publish(const Staging&);
    void discard(const Staging&);
    std::string remove(const std::string& id);
    std::string startup() const;
    // An empty identity clears the startup patch.
    std::string set_startup(const std::string& id);
    // Instrument-wide control actions, kept beside the patches rather than in
    // one: next-patch, previous-patch and tap-tempo, by control identity. A patch's own
    // assignment of the same control takes precedence while that patch plays.
    std::map<std::string, std::string> global_controls() const;
    // An empty action clears the control.
    std::string set_global_control(const std::string& control, const std::string& action);
    // How many complete revisions a patch keeps.
    static constexpr unsigned kept_revisions = 2;
private:
    std::vector<unsigned> revisions(const std::filesystem::path& patch) const;
    std::optional<StoredPatch> read(const std::string& id) const;
    std::vector<std::string> missing_files(const std::filesystem::path& revision, const Bundle&) const;
    std::string location(const std::string& id, const std::string& directory) const;
    std::filesystem::path root_;
    std::string engine_root_;
    bool usable_ = false;
};
// A Windows path as WSL mounts it: C:\Users\me → /mnt/c/Users/me.
std::string wsl_path(const std::filesystem::path&);
// Paths the engine can be told about: absolute, and nothing a file URI would
// have to escape (Ingen does not decode an escaped one consistently).
bool engine_path(const std::string&);
}
