// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "core/model.hpp"
#include <map>
#include <string_view>
#include <utility>

// The module catalogue as a file: exported from installed LV2 metadata on
// Linux, read everywhere. Windows uses an exported file as a fixture, so it
// needs no LV2 binaries. Presentation choices live in a separate overlay the
// project owns, and survive a new export.
namespace peepo {
// 2: an atom port is "midi" only when it takes MIDI, "atom" otherwise.
inline constexpr int catalogue_version = 2;
struct CatalogueDocument {
    std::vector<ModuleDefinition> modules;
    // What produced the file and from where, as the exporter recorded it.
    std::vector<std::pair<std::string, std::string>> provenance;
    // By module URI: the bundle and plugin class its metadata came from.
    std::map<std::string, std::string> origins;
};
// Deterministic text: the same modules always give the same bytes.
std::string write_catalogue(const CatalogueDocument&);
// Each returns an empty string, or what was wrong. A file with one invalid
// module is refused whole: it is generated, so a fault means a bad export.
std::string read_catalogue(std::string_view text, CatalogueDocument& out);
// Rules apply in order to the modules they select; later rules win. A rule
// selects by "uri" (exact) or "match" (part of the URI) and may set "name",
// "strip_prefix", "category", "color", "description", a stricter
// "availability" with its "reason", module "hidden" (retain identity, omit
// from Add), and "parameters": per-control "name",
// "unit", "kind" (continuous, integer or toggle), "default" and "hidden",
// listed controls first. After the rules, "families" lists plugins offered
// as one card in Add: a "name", an optional "description" and "variants",
// each a "uri" and the "label" of its pill, all of one category.
std::string apply_catalogue_overlay(std::string_view text, std::vector<ModuleDefinition>& modules);
// Reads `path` and, when given, `overlay_path` (both UTF-8).
std::string load_catalogue(const std::string& path, const std::string& overlay_path, std::vector<ModuleDefinition>& out);
std::uint32_t category_color(std::string_view category);
}
