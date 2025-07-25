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
#include <string_view>
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
#include "tb.h"
#include "ttable.h"
#include "util/barrier.h"
#include "util/timer.h"

namespace stormphrax::search {
    struct BenchData {
        SearchData search{};
        f64 time{};
    };

    constexpr auto kSyzygyProbeDepthRange = util::Range<i32>{1, kMaxDepth};
    constexpr auto kSyzygyProbeLimitRange = util::Range<i32>{0, 7};

    struct PvList {
        std::array<Move, kMaxDepth> moves{};
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
            moves[0] = kNullMove;
            length = 0;
        }
    };

    struct ThreadData;

    struct SearchStackEntry {
        PvList pv{};
        Move move;

        Score staticEval;

        Move excluded{};
        i32 reduction{};
    };

    struct MoveStackEntry {
        MovegenData movegenData{};
        StaticVector<Move, 256> failLowQuiets{};
        StaticVector<Move, 32> failLowNoisies{};
    };

    struct RootMove {
        Score displayScore{-kScoreInf};
        Score score{-kScoreInf};

        bool upperbound{false};
        bool lowerbound{false};

        i32 seldepth{};
        PvList pv{};
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
            contMoves.resize(kMaxDepth + 4);

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
        std::vector<PlayedMove> contMoves{};

        HistoryTables history{};
        CorrectionHistoryTable correctionHistory{};

        Position rootPos{};

        std::vector<u64> keyHistory{};

        [[nodiscard]] inline bool isMainThread() const {
            return id == 0;
        }

        [[nodiscard]] inline std::pair<Position, ThreadPosGuard<false>> applyNullmove(const Position& pos, i32 ply) {
            assert(ply <= kMaxDepth);

            stack[ply].move = kNullMove;
            conthist[ply] = &history.contTable(Piece::kWhitePawn, Square::kA1);
            contMoves[ply] = {Piece::kNone, Square::kNone};

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
            assert(ply <= kMaxDepth);

            const auto moving = pos.boards().pieceOn(move.fromSq());

            stack[ply].move = move;
            conthist[ply] = &history.contTable(moving, move.toSq());
            contMoves[ply] = {moving, move.toSq()};

            keyHistory.push_back(pos.key());

            return std::pair<Position, ThreadPosGuard<true>>{
                std::piecewise_construct,
                std::forward_as_tuple(pos.applyMove<NnueUpdateAction::kQueue>(move, &nnueState)),
                std::forward_as_tuple(keyHistory, nnueState)
            };
        }

        [[nodiscard]] inline RootMove* findRootMove(Move move) {
            for (u32 idx = pvIdx; idx < rootMoves.size(); ++idx) {
                auto& rootMove = rootMoves[idx];
                assert(rootMove.pv.length > 0);

                if (move == rootMove.pv.moves[0]) {
                    return &rootMove;
                }
            }

            return nullptr;
        }

        [[nodiscard]] inline bool isLegalRootMove(Move move) {
            return findRootMove(move) != nullptr;
        }

        [[nodiscard]] inline RootMove& pvMove() {
            return rootMoves[0];
        }

        [[nodiscard]] inline const RootMove& pvMove() const {
            return rootMoves[0];
        }
    };

    struct SetupInfo {
        Position rootPos{};

        usize keyHistorySize{};
        std::span<const u64> keyHistory{};
    };

    enum class RootStatus {
        kNoLegalMoves = 0,
        kTablebase,
        kGenerated,
        kSearchmoves,
    };

    class Searcher {
    public:
        explicit Searcher(usize ttSize = kDefaultTtSizeMib);

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
        void waitForStop();

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

        std::vector<std::thread> m_threads{};
        std::vector<std::unique_ptr<ThreadData>> m_threadData{};

        mutable std::mutex m_searchMutex{};

        std::atomic_bool m_quit{};
        std::atomic_bool m_searching{};

        util::Instant m_startTime;

        util::Barrier m_initBarrier{2};

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
        i32 m_maxDepth{kMaxDepth};

        MoveList m_rootMoveList{};
        u32 m_multiPv{};

        Score m_minRootScore{};
        Score m_maxRootScore{};

        eval::Contempt m_contempt{};

        RootStatus m_rootStatus{};
        SetupInfo m_setupInfo{};

        std::pair<RootStatus, std::optional<tb::ProbeResult>> initRootMoveList(const Position& pos);

        void stopThreads();

        void run(u32 threadId);

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

        Score searchRoot(ThreadData& thread, bool actualSearch);

        template <bool kPvNode = false, bool kRootNode = false>
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

        template <bool kPvNode = false>
        Score qsearch(ThreadData& thread, const Position& pos, i32 ply, u32 moveStackIdx, Score alpha, Score beta);

        void reportSingle(const ThreadData& thread, u32 pvIdx, i32 depth, f64 time);
        void report(const ThreadData& thread, i32 depth, f64 time);

        const ThreadData& selectThread();
        void finalReport();
    };
} // namespace stormphrax::search
