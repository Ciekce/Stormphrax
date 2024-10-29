/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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

#include "../../types.h"

#include <array>

#include "../../bitboard.h"
#include "../../core.h"
#include "../util.h"

// ignore the duplication pls ty :3
namespace stormphrax::attacks::bmi2 {
    struct RookSquareData {
        Bitboard srcMask;
        Bitboard dstMask;
        u32 offset;
    };

    struct RookData_ {
        std::array<RookSquareData, 64> data;
        u32 tableSize;
    };

    struct BishopSquareData {
        Bitboard mask;
        u32 offset;
    };

    struct BishopData_ {
        std::array<BishopSquareData, 64> data;
        u32 tableSize;
    };

    constexpr auto RookData = [] {
        RookData_ dst{};

        for (u32 i = 0; i < 64; ++i) {
            const auto square = static_cast<Square>(i);

            for (const auto dir: {offsets::Up, offsets::Down, offsets::Left, offsets::Right}) {
                const auto attacks = internal::generateSlidingAttacks(square, dir, 0);

                dst.data[i].srcMask |= attacks & ~internal::edges(dir);
                dst.data[i].dstMask |= attacks;
            }

            dst.data[i].offset = dst.tableSize;
            dst.tableSize += 1 << dst.data[i].srcMask.popcount();
        }

        return dst;
    }();

    constexpr auto BishopData = [] {
        BishopData_ dst{};

        for (u32 i = 0; i < 64; ++i) {
            const auto square = static_cast<Square>(i);

            for (const auto dir:
                 {offsets::UpLeft, offsets::UpRight, offsets::DownLeft, offsets::DownRight})
            {
                const auto attacks = internal::generateSlidingAttacks(square, dir, 0);
                dst.data[i].mask |= attacks & ~internal::edges(dir);
            }

            dst.data[i].offset = dst.tableSize;
            dst.tableSize += 1 << dst.data[i].mask.popcount();
        }

        return dst;
    }();
} // namespace stormphrax::attacks::bmi2
