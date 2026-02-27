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

    struct RookData {
        std::array<RookSquareData, Squares::kCount> data;
        u32 tableSize;
    };

    struct BishopSquareData {
        Bitboard mask;
        u32 offset;
    };

    struct BishopData {
        std::array<BishopSquareData, Squares::kCount> data;
        u32 tableSize;
    };

    constexpr auto kRookData = [] {
        RookData dst{};

        for (u32 i = 0; i < Squares::kCount; ++i) {
            const auto sq = Square::fromRaw(i);

            for (const auto dir : {offsets::kUp, offsets::kDown, offsets::kLeft, offsets::kRight}) {
                const auto attacks = internal::generateSlidingAttacks(sq, dir, 0);

                dst.data[i].srcMask |= attacks & ~internal::edges(dir);
                dst.data[i].dstMask |= attacks;
            }

            dst.data[i].offset = dst.tableSize;
            dst.tableSize += 1 << dst.data[i].srcMask.popcount();
        }

        return dst;
    }();

    constexpr auto kBishopData = [] {
        BishopData dst{};

        for (u32 i = 0; i < Squares::kCount; ++i) {
            const auto sq = Square::fromRaw(i);

            for (const auto dir : {offsets::kUpLeft, offsets::kUpRight, offsets::kDownLeft, offsets::kDownRight}) {
                const auto attacks = internal::generateSlidingAttacks(sq, dir, 0);
                dst.data[i].mask |= attacks & ~internal::edges(dir);
            }

            dst.data[i].offset = dst.tableSize;
            dst.tableSize += 1 << dst.data[i].mask.popcount();
        }

        return dst;
    }();
} // namespace stormphrax::attacks::bmi2
