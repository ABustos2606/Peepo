// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peepo contributors
#pragma once
// Reel, the keyframe mixer (https://github.com/ABustos2606/reel), and its
// page's telemetry. The contract is Reel's docs/peepo-page.md.
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace peepo {
inline bool reel_uri(const std::string& uri) { return uri == "https://github.com/ABustos2606/reel#reel"; }
inline constexpr std::size_t reel_channels = 4, reel_max_keyframes = 64;
// The control outputs the page watches, in ReelStatus::values order, and the
// atom output that carries the keyframe list.
inline constexpr std::array<const char*, 5> reel_outputs{"t_frame", "t_count", "t_active", "t_nearest", "t_step"};
inline constexpr const char* reel_list_port = "t_keyframes";
enum class ReelValue { Frame, Count, Active, Nearest, Step };
struct ReelKeyframe {
    double position = 0;  // 0 to 1
    int id = 0;           // picks the colour, id % 8
    std::array<double, reel_channels> levels{};
    bool operator==(const ReelKeyframe&) const = default;
};
struct ReelStatus {
    std::array<std::optional<double>, reel_outputs.size()> values{};
    std::optional<std::vector<ReelKeyframe>> keyframes;
    std::optional<double> value(ReelValue v) const { return values[static_cast<std::size_t>(v)]; }
};
// "1:" then 24 lowercase hex digits a keyframe: position, id, levels 1-4,
// four digits each, in position order. Anything else is not a list.
inline std::optional<std::vector<ReelKeyframe>> parse_reel_list(std::string_view text) {
    if (text.substr(0, 2) != "1:" || (text.size() - 2) % 24 != 0 || (text.size() - 2) / 24 > reel_max_keyframes) return std::nullopt;
    std::vector<ReelKeyframe> list;
    for (std::size_t at = 2; at < text.size(); at += 24) {
        unsigned words[6]{};
        for (std::size_t w = 0; w < 6; ++w)
            for (std::size_t d = 0; d < 4; ++d) {
                const char c = text[at + w * 4 + d];
                const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
                if (digit < 0) return std::nullopt;
                words[w] = words[w] * 16 + static_cast<unsigned>(digit);
            }
        ReelKeyframe k{words[0] / 65535.0, static_cast<int>(words[1]), {}};
        for (std::size_t c = 0; c < reel_channels; ++c) k.levels[c] = words[2 + c] / 65535.0;
        if (!list.empty() && k.position <= list.back().position) return std::nullopt;
        list.push_back(k);
    }
    return list;
}
// Keyframer::Easing's curves (Step 0, Linear 1, In quartic 2, Out quartic 3,
// Sine 4, Bounce 5), from upstream's lookup_tables.py.
inline double reel_easing(int curve, double t) {
    switch (curve) {
    case 0: return t < 0.5 ? 0 : 1;
    case 2: return t * t * t * t;
    case 3: return 1 - std::pow(1 - t, 4);
    case 4: return (1 - std::cos(t * 3.14159265358979323846)) / 2;
    case 5:
        if (t < 1 / 2.75) return 7.5625 * t * t;
        if (t < 2 / 2.75) { t -= 1.5 / 2.75; return 7.5625 * t * t + 0.75; }
        if (t < 2.5 / 2.75) { t -= 2.25 / 2.75; return 7.5625 * t * t + 0.9375; }
        t -= 2.625 / 2.75; return 7.5625 * t * t + 0.984375;
    default: return t;
    }
}
// A channel's level at Frame `x`, as Keyframer::Evaluate gives it: held
// before the first and after the last keyframe, eased between two.
inline double reel_level(const std::vector<ReelKeyframe>& list, std::size_t channel, int curve, double x) {
    if (list.empty()) return 0;
    if (x <= list.front().position) return list.front().levels[channel];
    if (x >= list.back().position) return list.back().levels[channel];
    std::size_t i = 1;
    while (list[i].position < x) ++i;
    const auto& a = list[i - 1];
    const auto& b = list[i];
    const double t = (x - a.position) / (b.position - a.position);
    return a.levels[channel] + (b.levels[channel] - a.levels[channel]) * reel_easing(curve, t);
}
// The keyframe colours (Keyframer::palette_, by id % 8).
inline constexpr std::array<std::uint32_t, 8> reel_palette{0xff0000, 0xff4000, 0xffff00, 0x40ff00, 0x00ff40, 0x0000ff, 0xff00ff, 0xff0040};
// The editing target is within 1/32 of the Frame control (ui.cc's 2048).
inline constexpr double reel_tolerance = 2048.0 / 65535.0;
}
