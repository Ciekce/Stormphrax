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

#include "eval/eval.h"
#include "limit.h"
#include "position/position.h"
#include "tb.h"
#include "thread.h"
#include "ttable.h"
#include "util/barrier.h"
#include "util/numa/numa.h"
#include "util/timer.h"

namespace stormphrax::search {
    struct BenchData {
        f64 time{};
        usize nodes{};
    };

    constexpr auto kSyzygyProbeDepthRange = util::Range<i32>{1, kMaxDepth};
    constexpr auto kSyzygyProbeLimitRange = util::Range<i32>{0, 7};

    enum class RootStatus {
        kNoLegalMoves = 0,
        kTablebase,
        kGenerated,
        kSearchmoves,
    };

    struct SetupInfo {
        Position rootPos{};

        usize keyHistorySize{};
        std::span<const u64> keyHistory{};
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

        inline void setLimiter(limit::SearchLimiter limiter) {
            m_limiter = limiter;
        }

        inline void setMaxDepth(i32 maxDepth) {
            m_maxDepth = maxDepth;
        }

        void startSearch(
            const Position& pos,
            std::span<const u64> keyHistory,
            util::Instant startTime,
            std::span<Move> moves,
            bool infinite
        );

        void stop();
        void waitForStop();

        // Clears all threads, and reallocates main thread data on the current NUMA node.
        // Makes this object unusable for normal searches, just for benching or datagen
        [[nodiscard]] ThreadData& take(u32 numaId = 0);

        // -> [move, unnormalised, normalised]
        std::pair<Score, Score> runDatagenSearch();

        void runBenchSearch(BenchData& data);

        [[nodiscard]] inline bool searching() const {
            const std::unique_lock lock{m_searchMutex};
            return m_searching.load(std::memory_order::relaxed);
        }

        void setThreads(u32 threadCount);

        [[nodiscard]] u32 threadCount() const {
            return m_threadData.size();
        }

        inline void setTtSize(usize mib) {
            m_ttable.resize(mib);
        }

        inline void setSilent(bool silent) {
            m_silent = silent;
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

        bool m_silent{};

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

        std::optional<limit::SearchLimiter> m_limiter{};

        bool m_infinite{};
        i32 m_maxDepth{kMaxDepth};

        MoveList m_rootMoveList{};
        u32 m_multiPv{};

        Score m_minRootScore{};
        Score m_maxRootScore{};

        tb::ProbeResult m_rootProbeResult{};

        eval::Contempt m_contempt{};

        RootStatus m_rootStatus{};
        SetupInfo m_setupInfo{};

        numa::NumaUniqueAllocation<CorrectionHistoryTable> m_corrhists{};

        RootStatus initRootMoveList(const Position& pos);

        void stopThreads();

        void run(u32 threadId);

        [[nodiscard]] inline bool hasStopped() const {
            return m_stop.load(std::memory_order::relaxed) != 0;
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

        const ThreadData& selectThread() const;
        void finalReport();
    };
} // namespace stormphrax::search
