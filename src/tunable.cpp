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

#include <cmath>

namespace stormphrax::tunable {
    namespace {
        inline i32 lmrReduction(f64 base, f64 divisor, i32 depth, i32 moves) {
            const auto lnDepth = std::log(static_cast<f64>(depth));
            const auto lnMoves = std::log(static_cast<f64>(moves));
            return static_cast<i32>(128.0 * (base + lnDepth * lnMoves / divisor));
        }
    } // namespace

    util::MultiArray<i32, 2, 256, 256> g_lmrTable{};
    std::array<i32, 13> g_seeValues{};

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

    void updateSeeValueTable() {
        // king and none
        g_seeValues.fill(0);

        const std::array scores = {
            seeValuePawn(),
            seeValueKnight(),
            seeValueBishop(),
            seeValueRook(),
            seeValueQueen(),
        };

        for (usize i = 0; i < scores.size(); ++i) {
            g_seeValues[i * 2 + 0] = scores[i];
            g_seeValues[i * 2 + 1] = scores[i];
        }
    }

    void init() {
        updateQuietLmrTable();
        updateNoisyLmrTable();

        updateSeeValueTable();
    }
} // namespace stormphrax::tunable
