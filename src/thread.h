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

#include "correction.h"
#include "eval/eval.h"
#include "history.h"
#include "move.h"
#include "movepick.h"
#include "pv.h"
#include "root_move.h"

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

    struct SearchStackEntry {
        PvList pv{};
        Move move;

        Score staticEval{};
        bool ttpv{};

        KillerTable killers{};

        Move excluded{};
        i32 reduction{};
    };

    struct MoveStackEntry {
        MovegenData movegenData{};
        StaticVector<Move, 256> failLowQuiets{};
        StaticVector<Move, 32> failLowNoisies{};
    };

    template <bool kUpdateNnue>
    class ThreadPosGuard {
    public:
        explicit ThreadPosGuard(std::vector<u64>& keyHistory, eval::NnueState& nnueState) :
                m_keyHistory{keyHistory}, m_nnueState{nnueState} {}

        ThreadPosGuard(const ThreadPosGuard&) = delete;
        ThreadPosGuard(ThreadPosGuard&&) = delete;

        inline ~ThreadPosGuard() {
            m_keyHistory.pop_back();

            if constexpr (kUpdateNnue) {
                m_nnueState.pop();
            }
        }

    private:
        std::vector<u64>& m_keyHistory;
        eval::NnueState& m_nnueState;
    };

    struct alignas(kCacheLineSize) ThreadData {
        ThreadData() {
            stack.resize(kMaxDepth + 4);
            moveStack.resize(kMaxDepth * 2);
            conthist.resize(kMaxDepth + 4);

            keyHistory.reserve(1024);
        }

        u32 id{};

        SearchData search{};

        bool datagen{false};

        i32 minNmpPly{};

        eval::NnueState nnueState{};

        u32 pvIdx{};
        std::vector<RootMove> rootMoves{};

        i32 depthCompleted{};

        std::vector<SearchStackEntry> stack{};
        std::vector<MoveStackEntry> moveStack{};
        std::vector<ContinuationSubtable*> conthist{};

        HistoryTables history{};

        CorrectionHistoryTable* correctionHistory{};

        Position rootPos{};

        std::vector<u64> keyHistory{};

        [[nodiscard]] inline bool isMainThread() const {
            return id == 0;
        }

        [[nodiscard]] std::pair<Position, ThreadPosGuard<false>> applyNullmove(const Position& pos, i32 ply);
        [[nodiscard]] std::pair<Position, ThreadPosGuard<true>> applyMove(const Position& pos, i32 ply, Move move);

        [[nodiscard]] RootMove* findRootMove(Move move);

        [[nodiscard]] inline bool isLegalRootMove(Move move) {
            return findRootMove(move) != nullptr;
        }

        void sortRootMoves();
        void sortRemainingRootMoves();

        [[nodiscard]] inline RootMove& pvMove() {
            return rootMoves[0];
        }

        [[nodiscard]] inline const RootMove& pvMove() const {
            return rootMoves[0];
        }
    };
} // namespace stormphrax::search
