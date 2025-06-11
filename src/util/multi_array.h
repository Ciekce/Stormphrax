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

#include <array>

namespace stormphrax::util {
    namespace internal {
        template <typename T, usize kN, usize... kNs>
        struct MultiArrayImpl {
            using Type = std::array<typename MultiArrayImpl<T, kNs...>::Type, kN>;
        };

        template <typename T, usize kN>
        struct MultiArrayImpl<T, kN> {
            using Type = std::array<T, kN>;
        };
    } // namespace internal

    template <typename T, usize... kNs>
    using MultiArray = typename internal::MultiArrayImpl<T, kNs...>::Type;
} // namespace stormphrax::util
