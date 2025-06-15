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

#include <cstddef>
#include <cstdint>
#include <exception>

#include <fmt/format.h>

namespace stormphrax {
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using u128 = unsigned __int128;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using i128 = __int128;

    using f32 = float;
    using f64 = double;

    using usize = std::size_t;

    [[noreturn]] inline void unimplemented() {
        std::terminate();
    }
} // namespace stormphrax

#define I64(V) INT64_C(V)
#define U64(V) UINT64_C(V)

#define SP_STRINGIFY_(S) #S
#define SP_STRINGIFY(S) SP_STRINGIFY_(S)

#ifndef NDEBUG
    #define SP_ALWAYS_INLINE_NDEBUG
#else
    #define SP_ALWAYS_INLINE_NDEBUG __attribute__((always_inline))
#endif
