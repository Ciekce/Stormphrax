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

#include "tunable.h"

#include <algorithm>
#include <cmath>

namespace stormphrax::tunable {
    namespace {
        inline i32 lmrReduction(f64 base, f64 divisor, i32 depth, i32 moves) {
            const auto lnDepth = std::log(static_cast<f64>(depth));
            const auto lnMoves = std::log(static_cast<f64>(moves));
            return static_cast<i32>(1024.0 * (base + lnDepth * lnMoves / divisor));
        }
    } // namespace

    util::MultiArray<i32, 2, 256, 256> g_lmrTable{};
    std::array<i32, 13> g_seeValues{};
    std::array<PieceType, PieceTypes::kCount> g_seeOrderedPts{};

    void updateQuietLmrTable() {
        const auto base = static_cast<f64>(quietLmrBase()) / 100.0;
        const auto divisor = static_cast<f64>(quietLmrDivisor()) / 100.0;

        for (i32 depth = 1; depth < 256; ++depth) {
            for (i32 moves = 1; moves < 256; ++moves) {
                g_lmrTable[0][depth][moves] = lmrReduction(base, divisor, depth, moves);
            }
        }
    }

    void updateNoisyLmrTable() {
        const auto base = static_cast<f64>(noisyLmrBase()) / 100.0;
        const auto divisor = static_cast<f64>(noisyLmrDivisor()) / 100.0;

        for (i32 depth = 1; depth < 256; ++depth) {
            for (i32 moves = 1; moves < 256; ++moves) {
                g_lmrTable[1][depth][moves] = lmrReduction(base, divisor, depth, moves);
            }
        }
    }

    void updateSeeTables() {
        g_seeValues.fill(0);

        const std::array values = {
            seeValuePawn(),
            seeValueKnight(),
            seeValueBishop(),
            seeValueRook(),
            seeValueQueen(),
        };

        for (usize i = 0; i < values.size(); ++i) {
            g_seeValues[i * 2 + 0] = values[i];
            g_seeValues[i * 2 + 1] = values[i];
        }

        g_seeOrderedPts = {
            PieceTypes::kPawn,
            PieceTypes::kKnight,
            PieceTypes::kBishop,
            PieceTypes::kRook,
            PieceTypes::kQueen,
            PieceTypes::kKing,
        };

        std::ranges::stable_sort(g_seeOrderedPts, [&](PieceType a, PieceType b) {
            return b == PieceTypes::kKing
                || (a != PieceTypes::kKing
                    && (values[a.idx()] * 10000 + a.idx()) < (values[b.idx()] * 10000 + b.idx()));
        });
    }

    void init() {
        updateQuietLmrTable();
        updateNoisyLmrTable();

        updateSeeTables();
    }
} // namespace stormphrax::tunable
