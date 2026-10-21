// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#include "engine/ingen/protocol.hpp"
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <map>

namespace peepo::ingen {
namespace {
constexpr std::string_view xsd = "http://www.w3.org/2001/XMLSchema#";
std::string name(std::string_view space, std::string_view local) { return std::string(space) + std::string(local); }
Value value_of(const turtle::Term& term) {
    using Kind = turtle::Term::Kind;
    if (term.kind == Kind::Iri) return {Value::Kind::Uri, 0, false, term.value};
    if (term.kind == Kind::Blank) return {};
    if (term.datatype.empty()) return {Value::Kind::String, 0, false, term.value};
    if (term.datatype == name(xsd, "boolean")) return {Value::Kind::Boolean, 0, term.value == "true" || term.value == "1", {}};
    if (term.datatype.starts_with(xsd)) {
        // from_chars is locale-independent; a comma decimal separator never applies.
        double number = 0;
        const auto* begin = term.value.data() + (term.value.starts_with('+') ? 1 : 0);
        const auto* end = term.value.data() + term.value.size();
        const auto result = std::from_chars(begin, end, number);
        if (result.ec == std::errc{} && result.ptr == end) return {Value::Kind::Number, number, false, {}};
    }
    return {Value::Kind::String, 0, false, term.value};
}
std::string header(int sequence, std::string_view type) {
    return "[]\n\ta patch:" + std::string(type) + " ;\n\tpatch:sequenceNumber \"" + std::to_string(sequence) + "\"^^xsd:int ;\n";
}
std::string reference(std::string_view iri) { return "<" + std::string(iri) + ">"; }
}
const Value* find(const Properties& properties, std::string_view predicate) {
    for (const auto& [key, value] : properties) if (key == predicate) return &value;
    return nullptr;
}
bool has(const Properties& properties, std::string_view predicate, std::string_view iri) {
    for (const auto& [key, value] : properties) if (key == predicate && value.kind == Value::Kind::Uri && value.text == iri) return true;
    return false;
}
std::string normal_path(std::string path) {
    while (path.size() > 1 && path.back() == '/' && path != "ingen:/") path.pop_back();
    return path;
}
std::vector<Message> decode(const std::vector<turtle::Triple>& triples) {
    // Blank nodes are local to a message, so one index covers subjects and bodies.
    std::map<std::string, std::vector<const turtle::Triple*>> blank;
    std::vector<std::string> order;
    for (const auto& triple : triples) {
        if (triple.subject.kind != turtle::Term::Kind::Blank) continue;
        auto& list = blank[triple.subject.value];
        if (list.empty()) order.push_back(triple.subject.value);
        list.push_back(&triple);
    }
    // A blank-node value: its type and its first number.
    auto value = [&](const turtle::Term& term) {
        if (term.kind != turtle::Term::Kind::Blank) return value_of(term);
        Value object{Value::Kind::Object, 0, false, {}};
        bool numbered = false;
        if (const auto it = blank.find(term.value); it != blank.end())
            for (const auto* triple : it->second) {
                if (triple->predicate.value == uri::rdf_type && triple->object.kind == turtle::Term::Kind::Iri) object.text = triple->object.value;
                else if (const auto number = value_of(triple->object); number.kind == Value::Kind::Number && !numbered) { object.number = number.number; numbered = true; }
            }
        return object.text.empty() || !numbered ? Value{} : object;
    };
    auto properties = [&](const turtle::Term& node) {
        Properties result;
        if (node.kind != turtle::Term::Kind::Blank) return result;
        if (const auto it = blank.find(node.value); it != blank.end())
            for (const auto* triple : it->second) result.emplace_back(triple->predicate.value, value(triple->object));
        return result;
    };
    static const std::pair<std::string_view, Message::Type> types[] = {
        {"BundleStart", Message::Type::BundleStart}, {"BundleEnd", Message::Type::BundleEnd}, {"Response", Message::Type::Response},
        {"Get", Message::Type::Get}, {"Put", Message::Type::Put}, {"Set", Message::Type::Set}, {"Delete", Message::Type::Delete},
        {"Copy", Message::Type::Copy}, {"Patch", Message::Type::Patch}};
    std::vector<Message> messages;
    for (const auto& label : order) {
        Message message;
        bool typed = false;
        for (const auto* triple : blank[label]) {
            const auto& predicate = triple->predicate.value;
            const auto& object = triple->object;
            if (predicate == uri::rdf_type) {
                for (const auto& [local, type] : types)
                    if (object.value == name(local.starts_with("Bundle") ? uri::ingen : uri::patch, local)) { message.type = type; typed = true; }
            } else if (predicate == name(uri::patch, "sequenceNumber")) message.sequence = static_cast<int>(value_of(object).number);
            else if (predicate == name(uri::patch, "subject")) message.subject = normal_path(object.value);
            else if (predicate == name(uri::patch, "destination")) message.destination = normal_path(object.value);
            else if (predicate == name(uri::patch, "property")) message.property = object.value;
            else if (predicate == name(uri::patch, "context")) message.context = object.value;
            else if (predicate == name(uri::patch, "value")) message.value = value(object);
            else if (predicate == name(uri::patch, "add")) message.body = properties(object);
            else if (predicate == name(uri::patch, "remove")) message.remove = properties(object);
            else if (predicate == name(uri::patch, "body")) {
                // A Response carries its status here; everything else, a description.
                if (object.kind == turtle::Term::Kind::Blank) message.body = properties(object);
                else message.status = static_cast<int>(value_of(object).number);
            }
        }
        // Untyped blank nodes are the bodies collected above.
        if (typed) messages.push_back(std::move(message));
    }
    return messages;
}
const char* status_text(int status) {
    static const char* const names[] = {
        "Success", "Engine reported a failure", "Invalid index", "Wrong object type", "Engine refused a malformed request", "Invalid address",
        "Wrong value type", "Invalid value", "Engine does not know this client", "Engine could not create the object", "Port directions do not match",
        "Already exists in the engine", "Internal engine error", "Invalid parent", "Invalid polyphony", "Engine cannot delete this", "Not found in the engine",
        "Engine cannot move this", "Engine was not ready", "Engine is out of space", "Parents differ", "Parent not found in the engine",
        "Plugin is not installed in the engine", "Port not found in the engine", "Port types do not match", "Unknown port type", "Engine could not compile the graph"};
    return status >= 0 && status < static_cast<int>(std::size(names)) ? names[status] : "Unknown engine error";
}
std::string prefixes() {
    std::string text;
    for (const auto& [prefix, space] : std::initializer_list<std::pair<std::string_view, std::string_view>>{
             {"atom", uri::atom}, {"ingen", uri::ingen}, {"lv2", uri::lv2}, {"midi", uri::midi}, {"patch", uri::patch}, {"peepo", uri::peepo}, {"xsd", xsd}})
        text += "@prefix " + std::string(prefix) + ": <" + std::string(space) + "> .\n";
    return text;
}
std::string get(int sequence, std::string_view subject) {
    return header(sequence, "Get") + "\tpatch:subject " + reference(subject) + " .\n";
}
std::string put(int sequence, std::string_view subject, std::string_view body) {
    return header(sequence, "Put") + "\tpatch:subject " + reference(subject) + " ;\n\tpatch:body [\n\t\t" + std::string(body) + "\n\t] .\n";
}
std::string set(int sequence, std::string_view subject, std::string_view property, std::string_view value) {
    return header(sequence, "Set") + "\tpatch:subject " + reference(subject) + " ;\n\tpatch:property " + std::string(property) +
           " ;\n\tpatch:value " + std::string(value) + " .\n";
}
std::string set_on_graph(int sequence, std::string_view graph, std::string_view property, std::string_view value) {
    return header(sequence, "Set") + "\tpatch:context ingen:internalContext ;\n\tpatch:subject " + reference(graph) + " ;\n\tpatch:property " +
           std::string(property) + " ;\n\tpatch:value " + std::string(value) + " .\n";
}
std::string remove(int sequence, std::string_view subject) {
    return header(sequence, "Delete") + "\tpatch:subject " + reference(subject) + " .\n";
}
std::string put_arc(int sequence, std::string_view parent, std::string_view tail, std::string_view head) {
    return put(sequence, parent, "a ingen:Arc ;\n\t\tingen:tail " + reference(tail) + " ;\n\t\tingen:head " + reference(head));
}
std::string remove_arc(int sequence, std::string_view tail, std::string_view head) {
    return header(sequence, "Delete") + "\tpatch:body [\n\t\ta ingen:Arc ;\n\t\tingen:tail " + reference(tail) + " ;\n\t\tingen:head " +
           reference(head) + "\n\t] .\n";
}
std::string copy(int sequence, std::string_view subject, std::string_view destination) {
    return header(sequence, "Copy") + "\tpatch:subject " + reference(subject) + " ;\n\tpatch:destination " + reference(destination) + " .\n";
}
std::string remove_property(int sequence, std::string_view subject, std::string_view property) {
    return header(sequence, "Patch") + "\tpatch:subject " + reference(subject) + " ;\n\tpatch:remove [\n\t\t" + std::string(property) +
           " patch:wildcard\n\t] ;\n\tpatch:add [ ] .\n";
}
std::string string_literal(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
                static const char digits[] = "0123456789ABCDEF";
                out += "\\u00"; out += digits[(c >> 4) & 15]; out += digits[c & 15];
            } else out += c;
        }
    }
    return out + "\"";
}
std::string float_literal(double value) {
    // LV2 control ports are 32-bit floats; nine digits round-trip one exactly.
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, static_cast<float>(value), std::chars_format::general, 9);
    return "\"" + std::string(buffer, result.ptr) + "\"^^xsd:float";
}
bool safe_iri(std::string_view iri) {
    if (iri.empty() || iri.size() > 1024 || iri.find(':') == std::string_view::npos) return false;
    for (const char c : iri)
        if (static_cast<unsigned char>(c) <= 0x20 || c == 0x7f || c == '<' || c == '>' || c == '"' || c == '{' || c == '}' || c == '|' || c == '^' || c == '`' || c == '\\') return false;
    return true;
}
bool lv2_symbol(std::string_view symbol) {
    if (symbol.empty() || symbol.size() > 256 || (symbol[0] >= '0' && symbol[0] <= '9')) return false;
    for (const char c : symbol) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
}
