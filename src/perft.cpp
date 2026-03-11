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
        template <auto MOVEGEN>
        usize doPerft(const Position& pos, i32 depth) {
            if (depth <= 0) {
                return 1;
            }

            ScoredMoveList moves{};
            MOVEGEN(moves, pos);

            usize total{};

            for (const auto [move, score] : moves) {
                if (depth == 1) {
                    ++total;
                } else {
                    const auto newPos = pos.applyMove(move);
                    total += doPerft<MOVEGEN>(newPos, depth - 1);
                }
            }

            return total;
        }

        template <auto MOVEGEN>
        void doSplitPerft(const Position& pos, i32 depth) {
            const auto start = Instant::now();

            ScoredMoveList moves{};
            MOVEGEN(moves, pos);

            usize total{};

            for (const auto [move, score] : moves) {
                const auto newPos = pos.applyMove(move);
                const auto value = doPerft<MOVEGEN>(newPos, depth - 1);

                total += value;
                println("{}\t{}", move, value);
            }

            const auto nps = static_cast<usize>(static_cast<f64>(total) / start.elapsed());

            println();
            println("total {}", total);
            println("{} nps", nps);
        }

        void isLegalMoveGen(ScoredMoveList& dst, const Position& pos) {
            for (i32 i = 0; i < 0x10000; ++i) {
                const auto move = Move::fromRaw(static_cast<u16>(i));
                if (pos.isLegal(move)) {
                    dst.push({move, 0});
                }
            }
        }
    } // namespace

    void perft(const Position& pos, i32 depth) {
        println("{}", doPerft<generateAll>(pos, depth));
    }

    void splitPerft(const Position& pos, i32 depth) {
        doSplitPerft<generateAll>(pos, depth);
    }

    void splitIsLegalPerft(const Position& pos, i32 depth) {
        doSplitPerft<isLegalMoveGen>(pos, depth);
    }

} // namespace stormphrax
