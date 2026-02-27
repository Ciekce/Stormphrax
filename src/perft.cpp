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

#include "perft.h"

#include "movegen.h"
#include "uci.h"
#include "util/timer.h"

namespace stormphrax {
    using util::Instant;

    namespace {
        usize doPerft(const Position& pos, i32 depth) {
            if (depth <= 0) {
                return 1;
            }

            ScoredMoveList moves{};
            generateAll(moves, pos);

            usize total{};

            for (const auto [move, score] : moves) {
                if (!pos.isLegal(move)) {
                    continue;
                }

                if (depth == 1) {
                    ++total;
                } else {
                    const auto newPos = pos.applyMove(move);
                    total += doPerft(newPos, depth - 1);
                }
            }

            return total;
        }
    } // namespace

    void perft(const Position& pos, i32 depth) {
        println("{}", doPerft(pos, depth));
    }

    void splitPerft(const Position& pos, i32 depth) {
        const auto start = Instant::now();

        ScoredMoveList moves{};
        generateAll(moves, pos);

        usize total{};

        for (const auto [move, score] : moves) {
            if (!pos.isLegal(move)) {
                continue;
            }

            const auto newPos = pos.applyMove(move);
            const auto value = doPerft(newPos, depth - 1);

            total += value;
            println("{}\t{}", move, value);
        }

        const auto nps = static_cast<usize>(static_cast<f64>(total) / start.elapsed());

        println();
        println("total {}", total);
        println("{} nps", nps);
    }
} // namespace stormphrax
