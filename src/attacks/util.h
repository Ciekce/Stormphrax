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

#include "../bitboard.h"
#include "../core.h"
#include "../util/cemath.h"

namespace stormphrax::attacks {
    namespace internal {
        constexpr Bitboard edges(i32 dir) {
            switch (dir) {
                case offsets::kUp:
                    return boards::kRank8;
                case offsets::kDown:
                    return boards::kRank1;
                case offsets::kLeft:
                    return boards::kFileA;
                case offsets::kRight:
                    return boards::kFileH;
                case offsets::kUpLeft:
                    return boards::kFileA | boards::kRank8;
                case offsets::kUpRight:
                    return boards::kFileH | boards::kRank8;
                case offsets::kDownLeft:
                    return boards::kFileA | boards::kRank1;
                case offsets::kDownRight:
                    return boards::kFileH | boards::kRank1;
                default:
                    __builtin_unreachable(); // don't
            }
        }

        constexpr Bitboard generateSlidingAttacks(Square src, i32 dir, Bitboard occupancy) {
            Bitboard dst{};

            auto blockers = edges(dir);

            const bool right = dir < 0;
            const auto shift = util::abs(dir);

            auto bit = src.bit();

            if (!(blockers & bit).empty()) {
                return dst;
            }

            blockers |= occupancy;

            do {
                if (right) {
                    dst |= bit >>= shift;
                } else {
                    dst |= bit <<= shift;
                }
            } while (!(bit & blockers));

            return dst;
        }
    } // namespace internal

    template <i32... Dirs>
    consteval std::array<Bitboard, Squares::kCount> generateEmptyBoardAttacks() {
        std::array<Bitboard, Squares::kCount> dst{};

        for (i32 square = 0; square < Squares::kCount; ++square) {
            for (const auto dir : {Dirs...}) {
                const auto attacks = internal::generateSlidingAttacks(Square::fromRaw(square), dir, 0);
                dst[square] |= attacks;
            }
        }

        return dst;
    }

    constexpr auto kEmptyBoardRooks =
        generateEmptyBoardAttacks<offsets::kUp, offsets::kDown, offsets::kLeft, offsets::kRight>();
    constexpr auto kEmptyBoardBishops =
        generateEmptyBoardAttacks<offsets::kUpLeft, offsets::kUpRight, offsets::kDownLeft, offsets::kDownRight>();

    template <i32... kDirs>
    consteval Bitboard genAllSlidingAttacks(Square src, Bitboard occupancy) {
        Bitboard dst{};

        for (const auto dir : {kDirs...}) {
            dst |= internal::generateSlidingAttacks(src, dir, occupancy);
        }

        return dst;
    }

    consteval Bitboard genRookAttacks(Square src, Bitboard occupancy) {
        return genAllSlidingAttacks<offsets::kUp, offsets::kDown, offsets::kLeft, offsets::kRight>(src, occupancy);
    }

    consteval Bitboard genBishopAttacks(Square src, Bitboard occupancy) {
        return genAllSlidingAttacks<offsets::kUpLeft, offsets::kUpRight, offsets::kDownLeft, offsets::kDownRight>(
            src,
            occupancy
        );
    }
} // namespace stormphrax::attacks
