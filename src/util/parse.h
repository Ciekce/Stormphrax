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

#include <charconv>
#include <concepts>
#include <optional>
#include <string_view>

namespace stormphrax::util {
    namespace detail {
        template <typename T>
        concept NonBoolInt = std::integral<T> && !std::same_as<T, bool>;
    }

    template <typename T = u32>
    [[nodiscard]] constexpr std::optional<T> tryParseDigit(char c) {
        if (c >= '0' && c <= '9') {
            return static_cast<T>(c - '0');
        } else {
            return {};
        }
    }

    template <detail::NonBoolInt T>
    [[nodiscard]] inline std::optional<T> tryParse(std::string_view value, u32 radix = 10) {
        T result{};

        const auto [ptr, err] =
            std::from_chars(value.data(), value.data() + value.size(), result, static_cast<i32>(radix));

        if (err == std::errc{}) {
            return result;
        } else {
            return {};
        }
    }

    template <detail::NonBoolInt T>
    inline bool tryParse(T& dst, std::string_view value, u32 radix = 10) {
        if (const auto parsed = tryParse<T>(value, radix)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    template <std::floating_point T>
    [[nodiscard]] inline std::optional<T> tryParse(std::string_view value) {
        T result{};

        const auto [ptr, err] = std::from_chars(value.data(), value.data() + value.size(), result);

        if (err == std::errc{}) {
            return result;
        } else {
            return {};
        }
    }

    template <std::floating_point T>
    inline bool tryParse(T& dst, std::string_view value) {
        if (const auto parsed = tryParse<T>(value)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }

    [[nodiscard]] constexpr std::optional<bool> tryParseBool(std::string_view value) {
        if (value == "true") {
            return true;
        } else if (value == "false") {
            return false;
        } else {
            return {};
        }
    }

    constexpr bool tryParseBool(bool& dst, std::string_view value) {
        if (const auto parsed = tryParseBool(value)) {
            dst = *parsed;
            return true;
        } else {
            return false;
        }
    }
} // namespace stormphrax::util
