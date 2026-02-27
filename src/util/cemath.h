/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include <concepts>

namespace stormphrax::util {
    template <typename T>
    constexpr T abs(T v) {
        return v < T{0} ? -v : v;
    }

    template <std::integral auto kOne>
    constexpr decltype(kOne) ilerp(decltype(kOne) a, decltype(kOne) b, decltype(kOne) t) {
        return (a * (kOne - t) + b * t) / kOne;
    }

    template <std::integral T>
    constexpr T ceilDiv(T a, T b) {
        return (a + b - 1) / b;
    }

    template <std::unsigned_integral auto kBlock>
    [[nodiscard]] inline decltype(kBlock) pad(decltype(kBlock) v) {
        return ceilDiv(v, kBlock) * kBlock;
    }
} // namespace stormphrax::util
