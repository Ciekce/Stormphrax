/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
 *
 * Stormphrax is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stormphrax is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stormphrax. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../types.h"

#include <optional>
#include <string>

namespace stormphrax::util {
    [[nodiscard]] inline std::optional<u32> tryParseDigit(char c) {
        if (c >= '0' && c <= '9') {
            return static_cast<u32>(c - '0');
        } else {
            return {};
        }
    }

    [[nodiscard]] inline std::optional<u32> tryParseU32(const std::string& value, u32 radix = 10) {
        try {
            return std::stoul(value, nullptr, static_cast<i32>(radix));
        } catch (...) {
            return {};
        }
    }

    inline bool tryParseU32(u32& dst, const std::string& value, u32 radix = 10) {
        if (const auto parsed = tryParseU32(value, radix)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] inline std::optional<i32> tryParseI32(const std::string& value, u32 radix = 10) {
        try {
            return std::stol(value, nullptr, static_cast<i32>(radix));
        } catch (...) {
            return {};
        }
    }

    inline bool tryParseI32(i32& dst, const std::string& value, u32 radix = 10) {
        if (const auto parsed = tryParseI32(value, radix)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] inline std::optional<u64> tryParseU64(const std::string& value, u32 radix = 10) {
        try {
            return std::stoull(value, nullptr, static_cast<i32>(radix));
        } catch (...) {
            return {};
        }
    }

    inline bool tryParseU64(u64& dst, const std::string& value, u32 radix = 10) {
        if (const auto parsed = tryParseU64(value, radix)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] inline std::optional<i64> tryParseI64(const std::string& value, u32 radix = 10) {
        try {
            return std::stoll(value, nullptr, static_cast<i32>(radix));
        } catch (...) {
            return {};
        }
    }

    inline bool tryParseI64(i64& dst, const std::string& value, u32 radix = 10) {
        if (const auto parsed = tryParseI64(value, radix)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] inline std::optional<usize> tryParseSize(const std::string& value, u32 radix = 10) {
        if constexpr (sizeof(usize) == 8) {
            return tryParseU64(value, radix);
        } else {
            return tryParseU32(value, radix);
        }
    }

    inline bool tryParseSize(usize& dst, const std::string& value, u32 radix = 10) {
        if (const auto parsed = tryParseSize(value, radix)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] inline std::optional<bool> tryParseBool(const std::string& value) {
        if (value == "true") {
            return true;
        } else if (value == "false") {
            return false;
        } else {
            return {};
        }
    }

    inline bool tryParseBool(bool& dst, const std::string& value) {
        if (const auto parsed = tryParseBool(value)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] inline std::optional<f32> tryParseF32(const std::string& value) {
        try {
            return std::stof(value, nullptr);
        } catch (...) {
            return {};
        }
    }

    inline bool tryParseF32(f32& dst, const std::string& value) {
        if (const auto parsed = tryParseF32(value)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] inline std::optional<f64> tryParseF64(const std::string& value) {
        try {
            return std::stod(value, nullptr);
        } catch (...) {
            return {};
        }
    }

    inline bool tryParseF64(f64& dst, const std::string& value) {
        if (const auto parsed = tryParseF64(value)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }
} // namespace stormphrax::util
