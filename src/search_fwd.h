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

#include "types.h"

#include <atomic>

#include "move.h"

namespace stormphrax::search {
    struct SearchData {
        i32 rootDepth{};
        i32 seldepth{};

        std::atomic<usize> nodes{};
        std::atomic<usize> tbhits{};

        SearchData() = default;

        SearchData(const SearchData& other) {
            *this = other;
        }

        inline void updateSeldepth(i32 ply) {
            seldepth = std::max(seldepth, ply + 1);
        }

        [[nodiscard]] inline usize loadNodes() const {
            return nodes.load(std::memory_order::relaxed);
        }

        inline void incNodes() {
            // avoid the performance penalty of atomicity (fetch_add), as there is only ever one writer
            nodes.store(nodes.load(std::memory_order::relaxed) + 1, std::memory_order::relaxed);
        }

        [[nodiscard]] inline usize loadTbHits() const {
            return tbhits.load(std::memory_order::relaxed);
        }

        inline void incTbHits() {
            // see above
            tbhits.store(tbhits.load(std::memory_order::relaxed) + 1, std::memory_order::relaxed);
        }

        SearchData& operator=(const SearchData& other) {
            rootDepth = other.rootDepth;
            seldepth = other.seldepth;

            nodes.store(other.nodes.load());
            tbhits.store(other.tbhits.load());

            return *this;
        }
    };

    struct PlayedMove {
        Piece moving;
        Square dst;
    };

    struct PvList {
        std::array<Move, kMaxDepth> moves{};
        u32 length{};

        inline void update(Move move, const PvList& child) {
            moves[0] = move;
            std::copy(child.moves.begin(), child.moves.begin() + child.length, moves.begin() + 1);

            length = child.length + 1;

            assert(length == 1 || moves[0] != moves[1]);
        }

        inline void reset() {
            moves[0] = kNullMove;
            length = 0;
        }
    };

    struct RootMove {
        Score score{-kScoreInf};
        Score windowScore{-kScoreInf};

        Score displayScore{-kScoreInf};
        Score previousScore{-kScoreInf};

        bool upperbound{false};
        bool lowerbound{false};

        i32 seldepth{};
        PvList pv{};

        usize nodes{};
    };
} // namespace stormphrax::search
