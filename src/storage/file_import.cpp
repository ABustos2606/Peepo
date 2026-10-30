// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "storage/file_import.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <string_view>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/syscall.h>
#endif

namespace peepo {
namespace fs = std::filesystem;
namespace {
std::string lower(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return text;
}
std::string utf8(const fs::path& path) {
    const auto u8 = path.u8string();
    return std::string(u8.begin(), u8.end());
}
fs::path from_utf8(const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())); }
// As the store lists them: no hidden names, no control characters, no separators.
bool usable_name(const std::string& name) {
    if (name.empty() || name[0] == '.' || name.size() > 255) return false;
    return std::none_of(name.begin(), name.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f || c == '/' || c == '\\'; });
}
std::string megabytes(std::uintmax_t bytes) { return std::to_string(bytes / (1024 * 1024)) + " MB"; }
std::string one_decimal(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f", value);
    return buffer;
}
FileCheck refused(std::string problem) { return {std::move(problem)}; }

// The model's JSON, read as it goes: only the top level is looked at, and
// every value inside is skipped over with its brackets and strings matched.
class ModelReader {
public:
    explicit ModelReader(const std::string& text) : text_(text) {}
    std::string check() {
        space();
        if (!take('{')) return "Not a model: it is not a JSON object";
        bool architecture = false, weights = false;
        space();
        if (!take('}')) {
            for (;;) {
                std::string key;
                if (!string(&key)) return "Not a model: the file is damaged";
                space();
                if (!take(':')) return "Not a model: the file is damaged";
                space();
                if (key == "architecture") {
                    std::string name;
                    if (!string(&name) || name.empty()) return "Not a model: its architecture is not named";
                    architecture = true;
                } else if (key == "weights") {
                    if (at() != '[' || !skip(0)) return "Not a model: its weights are not a list";
                    weights = true;
                } else if (!skip(0)) return "Not a model: the file is damaged";
                space();
                if (take(',')) { space(); continue; }
                if (take('}')) break;
                return "Not a model: the file is damaged";
            }
        }
        space();
        if (pos_ != text_.size()) return "Not a model: the file is damaged";
        if (!architecture) return "Not a model: it names no architecture";
        if (!weights) return "Not a model: it has no weights";
        return {};
    }
private:
    static constexpr unsigned max_depth = 32;
    char at() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    bool take(char c) {
        if (at() != c) return false;
        ++pos_;
        return true;
    }
    void space() { while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r')) ++pos_; }
    // A string, its escapes kept as written: only plain names are compared.
    bool string(std::string* out) {
        if (!take('"')) return false;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c == '\\') {
                if (pos_ >= text_.size()) return false;
                if (out) *out += c;
                ++pos_;
                if (out) *out += text_[pos_ - 1];
                continue;
            }
            if (out) *out += c;
        }
        return false;
    }
    bool skip(unsigned depth) {
        if (depth > max_depth) return false;
        const char c = at();
        if (c == '"') return string(nullptr);
        if (c == '[' || c == '{') {
            const char close = c == '[' ? ']' : '}';
            ++pos_;
            space();
            if (take(close)) return true;
            for (;;) {
                if (close == '}') {
                    if (!string(nullptr)) return false;
                    space();
                    if (!take(':')) return false;
                    space();
                }
                if (!skip(depth + 1)) return false;
                space();
                if (take(',')) { space(); continue; }
                return take(close);
            }
        }
        // A number, true, false or null: a run of their characters.
        const auto start = pos_;
        while (pos_ < text_.size() && std::string_view("0123456789+-.eEtruefalsn").find(text_[pos_]) != std::string_view::npos) ++pos_;
        return pos_ > start;
    }
    const std::string& text_;
    std::size_t pos_ = 0;
};

std::uint32_t le32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
std::uint16_t le16(const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

FileCheck check_length(unsigned channels, double seconds) {
    if (channels < 1 || channels > max_impulse_channels) return refused(std::to_string(channels) + " channels; an impulse response has 1 to 4");
    if (seconds <= 0) return refused("It holds no sound");
    if (seconds > max_impulse_seconds) return refused(one_decimal(seconds) + " s long; an impulse response is under 30 s");
    return {{}, seconds, channels};
}
FileCheck check_wav(std::ifstream& in, std::uintmax_t size) {
    unsigned char head[12];
    if (!in.read(reinterpret_cast<char*>(head), 12) || std::string(reinterpret_cast<char*>(head), 4) != "RIFF" || std::string(reinterpret_cast<char*>(head) + 8, 4) != "WAVE")
        return refused("Not a WAV file");
    unsigned channels = 0, bits = 0, block = 0, format = 0;
    std::uint32_t rate = 0;
    bool have_format = false;
    std::uintmax_t at = 12;
    // Chunks in any order; at most a few dozen before the sound.
    for (int chunks = 0; chunks < 64 && at + 8 <= size; ++chunks) {
        unsigned char chunk[8];
        in.seekg(static_cast<std::streamoff>(at));
        if (!in.read(reinterpret_cast<char*>(chunk), 8)) break;
        const std::string id(reinterpret_cast<char*>(chunk), 4);
        const std::uintmax_t length = le32(chunk + 4);
        if (id == "fmt ") {
            unsigned char f[26] = {};
            if (length < 16 || !in.read(reinterpret_cast<char*>(f), static_cast<std::streamsize>(std::min<std::uintmax_t>(length, sizeof f)))) return refused("The WAV header is damaged");
            format = le16(f);
            channels = le16(f + 2);
            rate = le32(f + 4);
            block = le16(f + 12);
            bits = le16(f + 14);
            // WAVE_FORMAT_EXTENSIBLE: the real format leads the sub-format.
            if (format == 0xFFFE) format = length >= 26 ? le16(f + 24) : 0;
            have_format = true;
        } else if (id == "data") {
            if (!have_format) return refused("The WAV header is damaged");
            const bool pcm = format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
            const bool floating = format == 3 && (bits == 32 || bits == 64);
            if (!pcm && !floating) return refused("A WAV kind a plugin cannot read (format " + std::to_string(format) + ", " + std::to_string(bits) + " bit)");
            if (rate < 1000 || rate > 768000 || channels == 0 || block != channels * bits / 8) return refused("The WAV header is damaged");
            if (at + 8 + length > size) return refused("The file is cut short");
            return check_length(channels, static_cast<double>(length / block) / rate);
        }
        at += 8 + length + (length & 1);
    }
    return refused(have_format ? "The WAV file holds no sound" : "The WAV header is damaged");
}
FileCheck check_flac(std::ifstream& in, std::uintmax_t size) {
    // "fLaC", then STREAMINFO: always the first metadata block, 34 bytes.
    unsigned char h[42];
    if (!in.read(reinterpret_cast<char*>(h), 42) || std::string(reinterpret_cast<char*>(h), 4) != "fLaC") return refused("Not a FLAC file");
    if ((h[4] & 0x7f) != 0 || h[5] != 0 || h[6] != 0 || h[7] != 34) return refused("The FLAC header is damaged");
    const unsigned char* s = h + 8;
    const std::uint32_t rate = (s[10] << 12) | (s[11] << 4) | (s[12] >> 4);
    const unsigned channels = ((s[12] >> 1) & 7) + 1;
    const std::uint64_t samples = (static_cast<std::uint64_t>(s[13] & 0x0f) << 32) | (static_cast<std::uint64_t>(s[14]) << 24) | (s[15] << 16) | (s[16] << 8) | s[17];
    if (rate < 1000 || rate > 768000) return refused("The FLAC header is damaged");
    if (samples == 0) return refused("The FLAC file does not say how long it is");
    if (size <= 42) return refused("The file is cut short");
    return check_length(channels, static_cast<double>(samples) / rate);
}

// Flush a file, or the directory that names it, to the card. Windows (the
// simulator) is left to itself.
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
std::string temporary_name() {
    static std::mt19937_64 random{std::random_device{}()};
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, ".import-%016llx.part", static_cast<unsigned long long>(random()));
    return buffer;
}
bool impulse(FileKind kind) { return kind != FileKind::Model; }
}

const char* store_folder(FileKind kind) {
    switch (kind) {
    case FileKind::Model: return "nam";
    case FileKind::Cabinet: return "ir/cab";
    case FileKind::Reverb: return "ir/reverb";
    case FileKind::Unsorted: break;
    }
    return nullptr;
}

FileCheck check_model(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error) return refused("It cannot be read");
    if (size > max_model_bytes) return refused("Larger than " + megabytes(max_model_bytes) + "; models are smaller");
    std::ifstream in(path, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    if (!in.read(text.data(), static_cast<std::streamsize>(size))) return refused("It cannot be read");
    auto problem = ModelReader(text).check();
    return refused(std::move(problem));
}
FileCheck check_impulse(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error) return refused("It cannot be read");
    if (size > max_impulse_bytes) return refused("Larger than " + megabytes(max_impulse_bytes) + "; impulse responses are smaller");
    std::ifstream in(path, std::ios::binary);
    if (!in) return refused("It cannot be read");
    const auto extension = lower(utf8(path.extension()));
    return extension == ".flac" ? check_flac(in, size) : check_wav(in, size);
}

FoundFiles find_files(const fs::path& root) {
    FoundFiles out;
    std::error_code error;
    std::size_t visited = 0;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error), end;
    for (; !error && it != end; it.increment(error)) {
        // A stick full of photos is looked through only so far.
        if (++visited > max_found * 20 || out.files.size() + out.rejected.size() >= max_found) { out.truncated = true; break; }
        const auto& entry = *it;
        const auto name = utf8(entry.path().filename());
        const auto status = entry.symlink_status(error);
        if (error) { error.clear(); continue; }
        if (fs::is_directory(status)) {
            const auto folder = lower(name);
            if (name.starts_with(".") || folder == "system volume information" || folder == "$recycle.bin") it.disable_recursion_pending();
            else if (it.depth() + 1 >= max_folder_depth) { it.disable_recursion_pending(); out.truncated = true; }
            continue;
        }
        if (!fs::is_regular_file(status) || name.starts_with(".")) continue;
        const auto dot = name.rfind('.');
        const auto extension = dot == std::string::npos ? std::string() : lower(name.substr(dot + 1));
        if (extension != "nam" && extension != "wav" && extension != "flac") continue;
        FoundFile found;
        found.source = entry.path();
        found.name = name;
        const auto relative = entry.path().parent_path().lexically_relative(root);
        if (relative != ".") {
            const auto u8 = relative.generic_u8string();
            found.folder.assign(u8.begin(), u8.end());
        }
        found.size = entry.file_size(error);
        if (error) { error.clear(); found.check = refused("It cannot be read"); }
        if (extension == "nam") found.kind = FileKind::Model;
        else {
            // The nearest folder that says decides.
            found.kind = FileKind::Unsorted;
            std::vector<std::string> folders;
            for (const auto& part : from_utf8(found.folder)) folders.push_back(lower(utf8(part)));
            for (auto folder = folders.rbegin(); folder != folders.rend(); ++folder) {
                if (folder->find("cab") != std::string::npos) { found.kind = FileKind::Cabinet; break; }
                if (folder->find("reverb") != std::string::npos || folder->find("room") != std::string::npos) { found.kind = FileKind::Reverb; break; }
            }
        }
        if (found.check.problem.empty()) {
            if (!usable_name(name)) found.check = refused("Its name cannot be used on Peepo");
            else found.check = found.kind == FileKind::Model ? check_model(found.source) : check_impulse(found.source);
        }
        (found.check.problem.empty() ? out.files : out.rejected).push_back(std::move(found));
    }
    if (error) out.truncated = true;
    const auto by_name = [](const FoundFile& a, const FoundFile& b) { return lower(a.name) != lower(b.name) ? lower(a.name) < lower(b.name) : a.folder < b.folder; };
    std::sort(out.files.begin(), out.files.end(), by_name);
    std::sort(out.rejected.begin(), out.rejected.end(), by_name);
    return out;
}

Imported import_file(const FileStore& store, const FoundFile& file, FileKind kind, const std::atomic<bool>* stop) {
    Imported out;
    out.name = file.name;
    const char* folder = store_folder(kind);
    if (!folder) { out.message = "Cabinet or Reverb has to be chosen first"; return out; }
    if ((kind == FileKind::Model) != (file.kind == FileKind::Model)) { out.message = "A model and an impulse response go to different folders"; return out; }
    if (!store.usable()) { out.message = "The files folder is not available"; return out; }
    if (!file.check.problem.empty() || !usable_name(file.name)) { out.message = file.check.problem.empty() ? "Its name cannot be used on Peepo" : file.check.problem; return out; }
    const auto directory = store.root() / folder;
    std::error_code error;

    // The same name and size is the same file, imported before; another file
    // with that name gets " (2)", " (3)"...
    const auto dot = file.name.rfind('.');
    const auto stem = file.name.substr(0, dot), extension = file.name.substr(dot);
    fs::path target;
    for (int n = 1; n < 100 && target.empty(); ++n) {
        const auto name = n == 1 ? file.name : stem + " (" + std::to_string(n) + ")" + extension;
        if (name.size() > 255) break;
        const auto path = directory / from_utf8(name);
        const auto status = fs::status(path, error);
        if (!fs::exists(status)) { target = path; out.name = name; break; }
        if (fs::is_regular_file(status) && fs::file_size(path, error) == file.size && !error) {
            out.name = name;
            out.result = ImportResult::Skipped;
            out.message = "Already on Peepo";
            return out;
        }
        error.clear();
    }
    if (target.empty()) { out.message = "Too many files named " + file.name; return out; }
    const auto room = fs::space(directory, error);
    if (error || room.available < file.size + import_reserve_bytes) { out.message = "Not enough room on Peepo"; return out; }

    const auto temporary = directory / temporary_name();
    const auto fail = [&](std::string message) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        out.message = std::move(message);
        return out;
    };
    // The streams are closed before anything is removed: Windows keeps an open file.
    const auto problem = [&]() -> std::string {
        std::ifstream in(file.source, std::ios::binary);
        if (!in) return "It cannot be read; was the stick pulled out?";
        std::ofstream copy(temporary, std::ios::binary | std::ios::trunc);
        if (!copy) return "Could not write to Peepo's files folder";
        std::vector<char> block(256 * 1024);
        std::uintmax_t copied = 0;
        while (in) {
            if (stop && stop->load()) return "Stopped";
            in.read(block.data(), static_cast<std::streamsize>(block.size()));
            const auto got = in.gcount();
            if (got > 0 && !copy.write(block.data(), got)) return "Could not write to Peepo's files folder";
            copied += static_cast<std::uintmax_t>(std::max<std::streamsize>(got, 0));
            if (copied > file.size) break;
        }
        if (in.bad() || copied != file.size) return "It cannot be read; was the stick pulled out?";
        if (!copy.flush()) return "Could not write to Peepo's files folder";
        return {};
    }();
    if (!problem.empty()) return fail(problem);
    // What lands in the folder is what was checked, whatever the stick did since.
    const auto again = impulse(kind) ? check_impulse(temporary) : check_model(temporary);
    if (!again.problem.empty()) return fail(again.problem);
    if (!flush(temporary)) return fail("Could not write to Peepo's files folder");
    fs::rename(temporary, target, error);
    if (error) return fail("Could not write to Peepo's files folder");
    flush(directory);
    out.result = ImportResult::Copied;
    out.message = "Copied";
    return out;
}

bool set_idle_io_priority() {
#if defined(__linux__) && defined(SYS_ioprio_set)
    // ioprio_set(IOPRIO_WHO_PROCESS, this thread, IOPRIO_CLASS_IDLE).
    constexpr int who_process = 1, class_idle = 3, class_shift = 13;
    return ::syscall(SYS_ioprio_set, who_process, 0, class_idle << class_shift) == 0;
#else
    return false;
#endif
}
}
