// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2026 Alejandro Bustos
#pragma once
#include <array>
#include <optional>
#include <string>
namespace peepo {
inline bool looper_uri(const std::string& uri) { return uri == "https://github.com/ABustos2606/loo-pi#loopi"; }
inline constexpr std::size_t looper_loops = 8, looper_fields = 6, looper_outputs = 51;
using LooperStatus = std::array<std::optional<double>, looper_outputs>;
inline std::string looper_symbol(std::size_t i) {
    if (i == 48) return "selected_out";
    if (i == 49) return "soloed";
    if (i == 50) return "free_mem";
    constexpr const char* suffix[]{"state", "next", "waiting", "len", "cycle", "pos"};
    return "l" + std::to_string(i / 6 + 1) + "_" + suffix[i % 6];
}
// The commands Loo-pi accepts (Milestone 3): SooperLooper's Event::command_t
// numbers, except Scratch (4) and the toggles 11 to 13, plus Loo-pi's Next
// (64) and Previous (65) loop.
inline bool looper_command_supported(int command) {
    switch(command) {
    case 0: case 1: case 2: case 3: case 5: case 6: case 7: case 8: case 9: case 10: case 14: case 15: case 16:
    case 17: case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 30: case 31: case 32: case 33:
    case 64: case 65: return true;
    default: return false;
    }
}
// The name a command shows in messages and on the Controls page.
inline std::string looper_command_name(int command) {
    switch(command) {
    case 0:return "Undo"; case 1:return "Redo"; case 2:return "Replace"; case 3:return "Reverse"; case 5:return "Record";
    case 6:return "Overdub"; case 7:return "Multiply"; case 8:return "Insert"; case 9:return "Mute"; case 10:return "Delay";
    case 14:return "Once"; case 15:return "Trigger"; case 16:return "Substitute"; case 17:return "Clear"; case 18:return "Redo all";
    case 21:return "Pause"; case 24:return "Solo"; case 33:return "Rec/Dub"; case 64:return "Next loop"; case 65:return "Previous loop";
    default:return "Command " + std::to_string(command);
    }
}
// Where a command goes: 0 the selected loop, 1 all loops, 1 + N loop N.
inline std::string looper_target_name(int target) {
    return target <= 0 ? "selected loop" : target == 1 ? "all loops" : "loop " + std::to_string(target - 1);
}
inline const char* looper_state(int state) {
    switch(state) {
    case -2:return "NO MEMORY"; case 0:return "EMPTY"; case 1:return "WAIT START";
    case 2:return "RECORDING"; case 3:return "WAIT STOP"; case 4:return "PLAYING";
    case 5:return "OVERDUB"; case 6:return "MULTIPLY"; case 7:return "INSERT"; case 8:return "REPLACE";
    case 9:return "DELAY"; case 12:return "ONCE"; case 13:return "SUBSTITUTE"; case 14:return "PAUSED";
    case 16:return "PLAYING"; case 10:return "MUTED"; case 17:return "UNDO";
    case 18:return "REDO"; case 19:return "REDO ALL"; case 15:return "UNDO ALL";
    case 20:return "MUTED"; default:return "WAITING";
    }
}
}
