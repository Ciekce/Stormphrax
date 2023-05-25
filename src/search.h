/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"

#include <memory>
#include <limits>
#include <string>
#include <optional>
#include <utility>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "search_fwd.h"
#include "position/position.h"
#include "limit/limit.h"
#include "util/timer.h"
#include "ttable.h"
#include "eval/eval.h"
#include "movegen.h"

namespace polaris::search
{
	constexpr i32 MaxDepth = 255;

	struct BenchData
	{
		SearchData search{};
		f64 time{};
	};

	constexpr u32 DefaultThreadCount = 1;
	constexpr auto ThreadCountRange = util::Range<u32>{1,  2048};

	class Searcher final
	{
	public:
		explicit Searcher(std::optional<usize> hashSize = {});
		~Searcher();

		void newGame();

		void startSearch(const Position &pos, i32 maxDepth, std::unique_ptr<limit::ISearchLimiter> limiter);
		void stop();

		void runBench(BenchData &data, const Position &pos, i32 depth);

		[[nodiscard]] inline bool searching() const
		{
			std::unique_lock lock{m_searchMutex};
			return m_flag.load(std::memory_order::relaxed) == SearchFlag;
		}

		void setThreads(u32 threads);

		inline void clearHash()
		{
			m_table.clear();
		}

		inline void setHashSize(usize size)
		{
			m_table.resize(size);
		}

	private:
		static constexpr i32 IdleFlag = 0;
		static constexpr i32 SearchFlag = 1;
		static constexpr i32 QuitFlag = 2;

		struct SearchStackEntry
		{
			StaticVector<HistoryMove, DefaultMoveListCapacity> quietsTried{};

			Move killer{NullMove};

			Score eval{};
			HistoryMove currMove{};
			Move excluded{};
		};

		struct ThreadData
		{
			ThreadData()
			{
				stack.resize(MaxDepth + 2);
				moveStack.resize(MaxDepth * 2);
			}

			u32 id{};
			std::thread thread{};

			// this is in here so clion in its infinite wisdom doesn't
			// mark the entire iterative deepening loop unreachable
			i32 maxDepth{};
			SearchData search{};

			eval::PawnCache pawnCache{};

			std::vector<SearchStackEntry> stack{};
			std::vector<ScoredMoveList> moveStack{};

			HistoryTable history{};

			Position pos{false};
		};

		TTable m_table{};

		u32 m_nextThreadId{};
		std::vector<ThreadData> m_threads{};

		mutable std::mutex m_searchMutex{};

		std::mutex m_startMutex{};
		std::condition_variable m_startSignal{};
		std::atomic_int m_flag{};

		std::atomic_int m_stop{};

		std::mutex m_stopMutex{};
		std::condition_variable m_stopSignal{};
		std::atomic_int m_runningThreads{};

		std::unique_ptr<limit::ISearchLimiter> m_limiter{};

		void stopThreads();

		void run(ThreadData &data);

		[[nodiscard]] inline bool shouldStop(const SearchData &data, bool allowSoftTimeout)
		{
			if (m_stop.load(std::memory_order::relaxed))
				return true;

			bool shouldStop = m_limiter->stop(data, allowSoftTimeout);
			return m_stop.fetch_or(shouldStop, std::memory_order::relaxed) || shouldStop;
		}

		void searchRoot(ThreadData &data, bool bench);

		Score search(ThreadData &data, i32 depth, i32 ply, u32 moveStackIdx, Score alpha, Score beta, bool cutnode);
		Score qsearch(ThreadData &data, i32 ply, u32 moveStackIdx, Score alpha, Score beta);

		void report(const ThreadData &data, i32 depth, Move move, f64 time, Score score, Score alpha, Score beta);
	};
}
