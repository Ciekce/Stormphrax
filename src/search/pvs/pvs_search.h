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

#include "../../types.h"

#include <utility>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "../search.h"
#include "../../util/timer.h"
#include "../../ttable.h"
#include "../../eval/eval.h"
#include "../../movegen.h"

namespace polaris::search::pvs
{
	class PvsSearcher final : public ISearcher
	{
	public:
		explicit PvsSearcher(std::optional<usize> hashSize = {});
		~PvsSearcher() final;

		void newGame() final;

		void startSearch(const Position &pos, i32 maxDepth, std::unique_ptr<limit::ISearchLimiter> limiter) final;
		void stop() final;

		void runBench(BenchData &data, const Position &pos, i32 depth) final;

		bool searching() final;

		void clearHash() final;
		void setHashSize(usize size) final;

	private:
		static constexpr i32 IdleFlag = 0;
		static constexpr i32 SearchFlag = 1;
		static constexpr i32 QuitFlag = 2;

		struct SearchStackEntry
		{
			Score eval{};
			MovegenData movegen{};
		};

		struct HistoryMove
		{
			Piece moving;
			Square dst;
		};

		struct ThreadData
		{
			ThreadData()
			{
				stack.resize(MaxDepth);
			}

			u32 id{};
			std::thread thread{};

			// this is in here so clion in its infinite wisdom doesn't
			// mark the entire iterative deepening loop unreachable
			i32 maxDepth{};
			SearchData search{};

			eval::PawnCache pawnCache{};
			std::vector<SearchStackEntry> stack{};

			HistoryTable history{};
			StaticVector<HistoryMove, DefaultMoveListCapacity> quietsTried{};

			Position pos{false};
		};

		TTable m_table{};

		u32 m_nextThreadId{};
		std::vector<ThreadData> m_threads{};

		std::mutex m_waitMutex{};
		std::condition_variable m_signal{};
		std::atomic_int m_flag{};

		std::atomic_int m_stop{};

		std::unique_ptr<limit::ISearchLimiter> m_limiter{};

		void run(ThreadData &data);

		bool shouldStop(const SearchData &data);

		void searchRoot(ThreadData &data, bool shouldReport);

		Score search(ThreadData &data, i32 depth, i32 ply, Score alpha, Score beta);
		Score qsearch(ThreadData &data, Score alpha, Score beta, i32 ply);

		void report(const ThreadData &data, i32 depth, Move move, f64 time, Score score, Score alpha, Score beta);
	};
}
