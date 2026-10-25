// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
#include "engine/ingen/turtle.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Ingen's socket protocol as verified in docs/14-ingen-engine-evidence.md:
// LV2 patch messages written as Turtle. Requests end with one newline and no
// NUL; everything the engine sends is NUL-terminated.
namespace peepo::ingen {
namespace uri {
inline constexpr std::string_view ingen = "http://drobilla.net/ns/ingen#";
inline constexpr std::string_view lv2 = "http://lv2plug.in/ns/lv2core#";
inline constexpr std::string_view patch = "http://lv2plug.in/ns/ext/patch#";
inline constexpr std::string_view atom = "http://lv2plug.in/ns/ext/atom#";
inline constexpr std::string_view midi = "http://lv2plug.in/ns/ext/midi#";
inline constexpr std::string_view peepo = "urn:peepo:ns#";
inline constexpr std::string_view rdf_type = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
}
struct Value {
    // Object: a blank node of one type and one number, as Ingen writes a MIDI
    // binding ([ a midi:Controller ; midi:controllerNumber 519 ]): the type's
    // IRI in `text`, the number in `number`.
    enum class Kind { None, Number, Boolean, String, Uri, Object };
    Kind kind = Kind::None;
    double number = 0;
    bool boolean = false;
    std::string text;
    bool operator==(const Value&) const = default;
};
// Predicate IRI and value; a predicate may repeat (`a lv2:AudioPort , lv2:InputPort`).
using Properties = std::vector<std::pair<std::string, Value>>;
const Value* find(const Properties&, std::string_view predicate);
bool has(const Properties&, std::string_view predicate, std::string_view iri);
struct Message {
    enum class Type { BundleStart, BundleEnd, Response, Get, Put, Set, Delete, Copy, Patch, Other };
    Type type = Type::Other;
    // Only meaningful on a Response: the engine numbers its own notifications
    // with the same predicate, so nothing else may be correlated on it.
    int sequence = 0;
    int status = 0;
    std::string subject, property, destination, context;
    Value value;
    // Put: the properties to set. Delete: the arc description, when there is one.
    // Patch: the properties to add; `remove` holds those to drop first.
    Properties body, remove;
};
// Top-level messages of one NUL-terminated engine message, in arrival order.
std::vector<Message> decode(const std::vector<turtle::Triple>&);
// Engine paths compare without a trailing slash: `ingen:/main/` is `ingen:/main`.
std::string normal_path(std::string);
const char* status_text(int status);

// Request writers. Each returns one complete statement ending in " .\n".
std::string prefixes();
std::string get(int sequence, std::string_view subject);
std::string put(int sequence, std::string_view subject, std::string_view body);
std::string set(int sequence, std::string_view subject, std::string_view property, std::string_view value);
// A property of the graph itself. Without the context the engine accepts it,
// then leaves it out of every later description of the graph.
std::string set_on_graph(int sequence, std::string_view graph, std::string_view property, std::string_view value);
std::string remove(int sequence, std::string_view subject);
std::string put_arc(int sequence, std::string_view parent, std::string_view tail, std::string_view head);
std::string remove_arc(int sequence, std::string_view tail, std::string_view head);
std::string copy(int sequence, std::string_view subject, std::string_view destination);
// Opens or closes a group of requests. With --atomic-bundles (and engine patch
// 0012), everything between takes effect on one audio frame.
std::string bundle(int sequence, bool start);
// Drop every value of `property` (a patch:wildcard remove), adding nothing.
std::string remove_property(int sequence, std::string_view subject, std::string_view property);
// Literal forms. A string that cannot be an IRI or symbol must not be sent.
std::string string_literal(std::string_view);
std::string float_literal(double);
bool safe_iri(std::string_view);
bool lv2_symbol(std::string_view);
}
