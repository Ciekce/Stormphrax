/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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
#include <algorithm>
#include <cassert>

#include "search_fwd.h"
#include "position/position.h"
#include "limit/limit.h"
#include "util/timer.h"
#include "ttable.h"
#include "eval/eval.h"
#include "movepick.h"
#include "util/barrier.h"
#include "history.h"

namespace stormphrax::search
{
	struct BenchData
	{
		SearchData search{};
		f64 time{};
	};

	constexpr u32 DefaultThreadCount = 1;
	constexpr auto ThreadCountRange = util::Range<u32>{1,  2048};

	constexpr auto SyzygyProbeDepthRange = util::Range<i32>{1, MaxDepth};
	constexpr auto SyzygyProbeLimitRange = util::Range<i32>{0, 7};

	struct PvList
	{
		std::array<Move, MaxDepth> moves{};
		u32 length{};

		inline auto update(Move move, const PvList &child)
		{
			moves[0] = move;
			std::copy(child.moves.begin(),
				child.moves.begin() + child.length,
				moves.begin() + 1);

			length = child.length + 1;

			assert(length == 1 || moves[0] != moves[1]);
		}

		inline auto operator=(const PvList &other) -> auto &
		{
			std::copy(other.moves.begin(), other.moves.begin() + other.length, moves.begin());
			length = other.length;

			return *this;
		}
	};

	struct ThreadData;

	struct SearchStackEntry
	{
		PvList pv{};
		Move move;

		Score staticEval;

		Move excluded{};
	};

	struct MoveStackEntry
	{
		MovegenData movegenData{};
		StaticVector<Move, 256> failLowQuiets{};
		StaticVector<Move, 32> failLowNoisies{};
	};

	struct alignas(SP_CACHE_LINE_SIZE) ThreadData
	{
		ThreadData()
		{
			stack.resize(MaxDepth + 4);
			moveStack.resize(MaxDepth * 2);
			conthist.resize(MaxDepth + 4);
		}

		u32 id{};
		std::thread thread{};

		// this is in here so clion in its infinite wisdom doesn't
		// mark the entire iterative deepening loop unreachable
		i32 maxDepth{};
		SearchData search{};

		PvList rootPv{};

		eval::NnueState nnueState{};

		std::vector<SearchStackEntry> stack{};
		std::vector<MoveStackEntry> moveStack{};
		std::vector<ContinuationSubtable *> conthist{};

		MoveList rootMoves{};

		HistoryTables history{};

		Position pos{};

		[[nodiscard]] inline auto isMainThread() const
		{
			return id == 0;
		}

		[[nodiscard]] inline auto isLegalRootMove(Move move) const
		{
			return std::ranges::find(rootMoves, move) != rootMoves.end();
		}

		inline auto setNullmove(i32 ply)
		{
			assert(ply <= MaxDepth);

			stack[ply].move = NullMove;
			conthist[ply] = &history.contTable(Piece::WhitePawn, Square::A1);
		}

		inline auto setMove(i32 ply, Move move)
		{
			assert(ply <= MaxDepth);

			stack[ply].move = move;
			conthist[ply] = &history.contTable(pos.boards().pieceAt(move.src()), move.dst());
		}
	};

	SP_ENUM_FLAGS(u32, NodeType)
	{
		None = 0x0,
		Pv = 0x1,
		Root = 0x3
	};

	class Searcher
	{
	public:
		explicit Searcher(usize ttSize = DefaultTtSize);

		~Searcher()
		{
			if (!m_quit)
				quit();
		}

		auto newGame() -> void;

		inline auto setLimiter(std::unique_ptr<limit::ISearchLimiter> limiter)
		{
			m_limiter = std::move(limiter);
		}

		auto startSearch(const Position &pos, i32 maxDepth, std::unique_ptr<limit::ISearchLimiter> limiter) -> void;
		auto stop() -> void;

		// -> [move, unnormalised, normalised]
		auto runDatagenSearch(ThreadData &thread) -> std::pair<Score, Score>;

		auto runBench(BenchData &data, const Position &pos, i32 depth) -> void;

		[[nodiscard]] inline auto searching() const
		{
			const std::unique_lock lock{m_searchMutex};
			return m_searching.load(std::memory_order::relaxed);
		}

		auto setThreads(u32 threads) -> void;

		inline auto setTtSize(usize size)
		{
			m_ttable.resize(size);
		}

		inline auto quit() -> void
		{
			m_quit.store(true, std::memory_order::release);

			stop();
			stopThreads();
		}

	private:
		TTable m_ttable;

		u32 m_nextThreadId{};
		std::vector<ThreadData> m_threads{};

		mutable std::mutex m_searchMutex{};

		std::atomic_bool m_quit{};
		std::atomic_bool m_searching{};

		util::Barrier m_resetBarrier{2};
		util::Barrier m_idleBarrier{2};

		util::Barrier m_searchEndBarrier{1};

		std::atomic_int m_stop{};

		std::mutex m_stopMutex{};
		std::condition_variable m_stopSignal{};
		std::atomic_int m_runningThreads{};

		std::unique_ptr<limit::ISearchLimiter> m_limiter{};

		Score m_minRootScore{};
		Score m_maxRootScore{};

		eval::Contempt m_contempt{};

		auto stopThreads() -> void;

		auto run(ThreadData &thread) -> void;

		[[nodiscard]] inline auto shouldStop(const SearchData &data, bool checkLimiter, bool allowSoftTimeout) -> bool
		{
			if (checkLimiter)
			{
				if (m_stop.load(std::memory_order::relaxed))
					return true;

				if (m_limiter->stop(data, allowSoftTimeout))
				{
					m_stop.store(true, std::memory_order::relaxed);
					return true;
				}
			}

			return m_stop.load(std::memory_order::relaxed);
		}

		auto searchRoot(ThreadData &thread, bool mainSearchThread) -> Score;

		template <bool RootNode = false, bool PvNode = false>
		auto search(ThreadData &thread, PvList &pv, i32 depth,
			i32 ply, u32 moveStackIdx, Score alpha, Score beta, bool cutnode) -> Score;

		template <>
		auto search<true, false>(ThreadData &thread, PvList &pv, i32 depth,
			i32 ply, u32 moveStackIdx, Score alpha, Score beta, bool cutnode) -> Score = delete;

		auto qsearch(ThreadData &thread, i32 ply, u32 moveStackIdx, Score alpha, Score beta) -> Score;

		auto report(const ThreadData &mainThread, const PvList &pv, i32 depth,
			f64 time, Score score, Score alpha = -ScoreInf, Score beta = ScoreInf) -> void;
		auto finalReport(const ThreadData &mainThread, const PvList &pv,
			i32 depthCompleted, f64 time, Score score, bool softTimeout) -> void;
	};
}
