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

#include "types.h"

#include <array>

#include "attacks/util.h"
#include "bitboard.h"
#include "core.h"
#include "util/multi_array.h"

namespace stormphrax {
    namespace detail {
        consteval util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> generateBetweenRays() {
            util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> dst{};

            for (i32 from = 0; from < Squares::kCount; ++from) {
                const auto srcSquare = Square::fromRaw(from);
                const auto srcMask = srcSquare.bit();

                const auto rookAttacks = attacks::kEmptyBoardRooks[from];
                const auto bishopAttacks = attacks::kEmptyBoardBishops[from];

                for (i32 to = 0; to < Squares::kCount; ++to) {
                    if (from == to) {
                        continue;
                    }

                    const auto dstSquare = Square::fromRaw(to);
                    const auto dstMask = dstSquare.bit();

                    if (rookAttacks[dstSquare]) {
                        dst[from][to] =
                            attacks::genRookAttacks(srcSquare, dstMask) & attacks::genRookAttacks(dstSquare, srcMask);
                    } else if (bishopAttacks[dstSquare]) {
                        dst[from][to] = attacks::genBishopAttacks(srcSquare, dstMask)
                                      & attacks::genBishopAttacks(dstSquare, srcMask);
                    }
                }
            }

            return dst;
        }

        consteval util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> generateIntersectingRays() {
            util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> dst{};

            for (i32 from = 0; from < Squares::kCount; ++from) {
                const auto srcSquare = Square::fromRaw(from);
                const auto srcMask = srcSquare.bit();

                const auto rookAttacks = attacks::kEmptyBoardRooks[from];
                const auto bishopAttacks = attacks::kEmptyBoardBishops[from];

                for (i32 to = 0; to < Squares::kCount; ++to) {
                    if (from == to) {
                        continue;
                    }

                    const auto dstSquare = Square::fromRaw(to);
                    const auto dstMask = dstSquare.bit();

                    if (rookAttacks[dstSquare]) {
                        dst[from][to] = (srcMask | attacks::genRookAttacks(srcSquare, Bitboard{}))
                                      & (dstMask | attacks::genRookAttacks(dstSquare, Bitboard{}));
                    } else if (bishopAttacks[dstSquare]) {
                        dst[from][to] = (srcMask | attacks::genBishopAttacks(srcSquare, Bitboard{}))
                                      & (dstMask | attacks::genBishopAttacks(dstSquare, Bitboard{}));
                    }
                }
            }

            return dst;
        }

        consteval util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> generatePassingRays() {
            util::MultiArray<Bitboard, Squares::kCount, Squares::kCount> dst{};

            for (i32 from = 0; from < Squares::kCount; ++from) {
                const auto srcSquare = Square::fromRaw(from);
                const auto srcMask = srcSquare.bit();

                const auto rookAttacks = attacks::kEmptyBoardRooks[from];
                const auto bishopAttacks = attacks::kEmptyBoardBishops[from];

                for (i32 to = 0; to < Squares::kCount; ++to) {
                    if (from == to) {
                        continue;
                    }

                    const auto dstSquare = Square::fromRaw(to);
                    const auto dstMask = dstSquare.bit();

                    if (rookAttacks[dstSquare]) {
                        dst[from][to] = attacks::genRookAttacks(srcSquare, Bitboard{})
                                      & (attacks::genRookAttacks(dstSquare, srcMask) | dstMask);
                    } else if (bishopAttacks[dstSquare]) {
                        dst[from][to] = attacks::genBishopAttacks(srcSquare, Bitboard{})
                                      & (attacks::genBishopAttacks(dstSquare, srcMask) | dstMask);
                    }
                }
            }

            return dst;
        }

        constexpr auto kBetweenRays = generateBetweenRays();
        constexpr auto kIntersectingRays = generateIntersectingRays();
        constexpr auto kPassingRays = generatePassingRays();
    } // namespace detail

    constexpr Bitboard rayBetween(Square src, Square dst) {
        return detail::kBetweenRays[src.idx()][dst.idx()];
    }

    constexpr Bitboard rayIntersecting(Square src, Square dst) {
        return detail::kIntersectingRays[src.idx()][dst.idx()];
    }

    constexpr Bitboard rayPast(Square src, Square target) {
        return detail::kPassingRays[src.idx()][target.idx()];
    }
} // namespace stormphrax
