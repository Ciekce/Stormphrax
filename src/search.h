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

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "correction.h"
#include "eval/eval.h"
#include "history.h"
#include "limit/limit.h"
#include "movepick.h"
#include "position/position.h"
#include "search_fwd.h"
#include "ttable.h"
#include "util/barrier.h"
#include "util/timer.h"

namespace stormphrax::search {
    struct BenchData {
        SearchData search{};
        f64 time{};
    };

    constexpr auto SyzygyProbeDepthRange = util::Range<i32>{1, MaxDepth};
    constexpr auto SyzygyProbeLimitRange = util::Range<i32>{0, 7};

    struct PvList {
        std::array<Move, MaxDepth> moves{};
        u32 length{};

        inline void update(Move move, const PvList& child) {
            moves[0] = move;
            std::copy(child.moves.begin(), child.moves.begin() + child.length, moves.begin() + 1);

            length = child.length + 1;

            assert(length == 1 || moves[0] != moves[1]);
        }

        inline PvList& operator=(const PvList& other) {
            std::copy(other.moves.begin(), other.moves.begin() + other.length, moves.begin());
            length = other.length;

            return *this;
        }

        inline void reset() {
            moves[0] = NullMove;
            length = 0;
        }
    };

    struct ThreadData;

    struct SearchStackEntry {
        PvList pv{};
        Move move;

        Score staticEval;

        KillerTable killers{};

        Move excluded{};
    };

    struct MoveStackEntry {
        MovegenData movegenData{};
        StaticVector<Move, 256> failLowQuiets{};
        StaticVector<Move, 32> failLowNoisies{};
    };

    template <bool UpdateNnue>
    class ThreadPosGuard {
    public:
        explicit ThreadPosGuard(std::vector<u64>& keyHistory, eval::NnueState& nnueState) :
                m_keyHistory{keyHistory}, m_nnueState{nnueState} {}

        ThreadPosGuard(const ThreadPosGuard&) = delete;
        ThreadPosGuard(ThreadPosGuard&&) = delete;

        inline ~ThreadPosGuard() {
            m_keyHistory.pop_back();

            if constexpr (UpdateNnue) {
                m_nnueState.pop();
            }
        }

    private:
        std::vector<u64>& m_keyHistory;
        eval::NnueState& m_nnueState;
    };

    struct alignas(CacheLineSize) ThreadData {
        ThreadData() {
            stack.resize(MaxDepth + 4);
            moveStack.resize(MaxDepth * 2);
            conthist.resize(MaxDepth + 4);
            contMoves.resize(MaxDepth + 4);

            keyHistory.reserve(1024);
        }

        u32 id{};
        std::thread thread{};

        SearchData search{};

        bool datagen{false};

        i32 minNmpPly{};

        PvList rootPv{};

        eval::NnueState nnueState{};

        std::vector<SearchStackEntry> stack{};
        std::vector<MoveStackEntry> moveStack{};
        std::vector<ContinuationSubtable*> conthist{};
        std::vector<PlayedMove> contMoves{};

        HistoryTables history{};
        CorrectionHistoryTable correctionHistory{};

        Position rootPos{};

        std::vector<u64> keyHistory{};

        [[nodiscard]] inline bool isMainThread() const {
            return id == 0;
        }

        [[nodiscard]] inline std::pair<Position, ThreadPosGuard<false>> applyNullmove(const Position& pos, i32 ply) {
            assert(ply <= MaxDepth);

            stack[ply].move = NullMove;
            conthist[ply] = &history.contTable(Piece::WhitePawn, Square::A1);
            contMoves[ply] = {Piece::None, Square::None};

            keyHistory.push_back(pos.key());

            return std::pair<Position, ThreadPosGuard<false>>{
                std::piecewise_construct,
                std::forward_as_tuple(pos.applyNullMove()),
                std::forward_as_tuple(keyHistory, nnueState)
            };
        }

        [[nodiscard]] inline std::pair<Position, ThreadPosGuard<true>> applyMove(
            const Position& pos,
            i32 ply,
            Move move
        ) {
            assert(ply <= MaxDepth);

            const auto moving = pos.boards().pieceAt(move.src());

            stack[ply].move = move;
            conthist[ply] = &history.contTable(moving, move.dst());
            contMoves[ply] = {moving, move.dst()};

            keyHistory.push_back(pos.key());

            return std::pair<Position, ThreadPosGuard<true>>{
                std::piecewise_construct,
                std::forward_as_tuple(pos.applyMove<NnueUpdateAction::Queue>(move, &nnueState)),
                std::forward_as_tuple(keyHistory, nnueState)
            };
        }

        inline void clearContMove(i32 ply) {
            contMoves[ply] = {Piece::None, Square::None};
        }
    };

    struct SetupInfo {
        Position rootPos{};

        usize keyHistorySize{};
        std::span<const u64> keyHistory{};
    };

    enum class RootStatus {
        NoLegalMoves = 0,
        Tablebase,
        Generated,
        Searchmoves,
    };

    class Searcher {
    public:
        explicit Searcher(usize ttSize = DefaultTtSizeMib);

        ~Searcher() {
            if (!m_quit) {
                quit();
            }
        }

        void newGame();
        void ensureReady();

        inline void setLimiter(std::unique_ptr<limit::ISearchLimiter> limiter) {
            m_limiter = std::move(limiter);
        }

        // ignored for bench and real searches
        inline void setDatagenMaxDepth(i32 maxDepth) {
            m_maxDepth = maxDepth;
        }

        void startSearch(
            const Position& pos,
            std::span<const u64> keyHistory,
            util::Instant startTime,
            i32 maxDepth,
            std::span<Move> moves,
            std::unique_ptr<limit::ISearchLimiter> limiter,
            bool infinite
        );

        void stop();

        // -> [move, unnormalised, normalised]
        std::pair<Score, Score> runDatagenSearch(ThreadData& thread);

        void runBench(BenchData& data, const Position& pos, i32 depth);

        [[nodiscard]] inline bool searching() const {
            const std::unique_lock lock{m_searchMutex};
            return m_searching.load(std::memory_order::relaxed);
        }

        void setThreads(u32 threadCount);

        inline void setTtSize(usize mib) {
            m_ttable.resize(mib);
        }

        inline void quit() {
            m_quit.store(true, std::memory_order::release);

            stop();
            stopThreads();
        }

    private:
        TTable m_ttable;

        std::vector<ThreadData> m_threads{};

        mutable std::mutex m_searchMutex{};

        std::atomic_bool m_quit{};
        std::atomic_bool m_searching{};

        util::Instant m_startTime;

        util::Barrier m_resetBarrier{2};
        util::Barrier m_idleBarrier{2};
        util::Barrier m_setupBarrier{2};

        util::Barrier m_searchEndBarrier{1};

        std::atomic_int m_stop{};

        std::mutex m_stopMutex{};
        std::condition_variable m_stopSignal{};
        std::atomic_int m_runningThreads{};

        std::unique_ptr<limit::ISearchLimiter> m_limiter{};

        bool m_infinite{};
        i32 m_maxDepth{MaxDepth};

        MoveList m_rootMoves{};

        Score m_minRootScore{};
        Score m_maxRootScore{};

        eval::Contempt m_contempt{};

        RootStatus m_rootStatus{};
        SetupInfo m_setupInfo{};

        RootStatus initRootMoves(const Position& pos);

        void stopThreads();

        void run(ThreadData& thread);

        [[nodiscard]] inline bool hasStopped() const {
            return m_stop.load(std::memory_order::relaxed) != 0;
        }

        [[nodiscard]] inline bool checkStop(const SearchData& data, bool mainThread, bool allowSoft) {
            if (hasStopped()) {
                return true;
            }

            if (mainThread && m_limiter->stop(data, allowSoft)) {
                m_stop.store(1, std::memory_order::relaxed);
                return true;
            }

            return false;
        }

        [[nodiscard]] inline bool checkHardTimeout(const SearchData& data, bool mainThread) {
            return checkStop(data, mainThread, false);
        }

        [[nodiscard]] inline bool checkSoftTimeout(const SearchData& data, bool mainThread) {
            return checkStop(data, mainThread, true);
        }

        [[nodiscard]] inline f64 elapsed() const {
            return m_startTime.elapsed();
        }

        [[nodiscard]] inline bool isLegalRootMove(Move move) const {
            return std::ranges::find(m_rootMoves, move) != m_rootMoves.end();
        }

        Score searchRoot(ThreadData& thread, bool actualSearch);

        template <bool PvNode = false, bool RootNode = false>
        Score search(
            ThreadData& thread,
            const Position& pos,
            PvList& pv,
            i32 depth,
            i32 ply,
            u32 moveStackIdx,
            Score alpha,
            Score beta,
            bool cutnode
        );

        template <>
        Score search<false, true>(
            ThreadData& thread,
            const Position& pos,
            PvList& pv,
            i32 depth,
            i32 ply,
            u32 moveStackIdx,
            Score alpha,
            Score beta,
            bool cutnode
        ) = delete;

        template <bool PvNode = false>
        Score qsearch(ThreadData& thread, const Position& pos, i32 ply, u32 moveStackIdx, Score alpha, Score beta);

        void report(
            const ThreadData& mainThread,
            const PvList& pv,
            i32 depth,
            f64 time,
            Score score,
            Score alpha = -ScoreInf,
            Score beta = ScoreInf
        );

        void finalReport(const ThreadData& mainThread, const PvList& pv, i32 depthCompleted, f64 time, Score score);
    };
} // namespace stormphrax::search
