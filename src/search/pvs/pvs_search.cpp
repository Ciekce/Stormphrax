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

#include "pvs_search.h"

#include <iostream>
#include <algorithm>
#include <cmath>

#include "../../uci.h"
#include "../../movegen.h"
#include "../../eval/eval.h"

namespace polaris::search::pvs
{
	namespace
	{
		constexpr f64 MinReportDelay = 1.0;

		constexpr i32 MinAspDepth = 6;

		constexpr Score InitialWindow = 10;
		constexpr Score MaxWindow = 500;

		constexpr i32 MinNullmoveDepth = 3;
		constexpr i32 MinLmrDepth = 3;

		// values from viriditas
		//TODO tune for polaris
		constexpr f64 LmrBase = 0.77;
		constexpr f64 LmrDivisor = 2.36;

		[[nodiscard]] std::array<std::array<i32, 256>, 256> generateLmrTable()
		{
			std::array<std::array<i32, 256>, 256> dst{};

			// neither can be 0
			for (i32 depth = 1; depth < 256; ++depth)
			{
				for (i32 moves = 1; moves < 256; ++moves)
				{
					dst[depth][moves] = static_cast<i32>(LmrBase
						+ std::log(static_cast<f64>(depth)) * std::log(static_cast<f64>(moves)) / LmrDivisor);
				}
			}

			return dst;
		}

		const auto LmrTable = generateLmrTable();
	}

	PvsSearcher::PvsSearcher(std::optional<size_t> hashSize)
		: m_table{hashSize ? *hashSize : DefaultHashSize}
	{
		auto &threadData = m_threads.emplace_back();

		threadData.id = m_nextThreadId++;
		threadData.thread = std::thread{[this, &threadData]
		{
			run(threadData);
		}};
	}

	PvsSearcher::~PvsSearcher()
	{
		m_flag.store(QuitFlag, std::memory_order_seq_cst);
		stop();

		m_signal.notify_all();

		for (auto &thread : m_threads)
		{
			thread.thread.join();
		}
	}

	void PvsSearcher::newGame()
	{
		m_table.clear();

		for (auto &thread : m_threads)
		{
			thread.pawnCache.clear();
			std::fill(thread.stack.begin(), thread.stack.end(), SearchStackEntry{});
		}
	}

	void PvsSearcher::startSearch(const Position &pos, i32 maxDepth, std::unique_ptr<limit::ISearchLimiter> limiter)
	{
		if (!limiter)
		{
			std::cerr << "missing limiter" << std::endl;
			return;
		}

		m_flag.store(SearchFlag, std::memory_order::seq_cst);

		for (auto &thread : m_threads)
		{
			thread.maxDepth = maxDepth;
			thread.search = SearchData{};
			thread.pos = pos;
		}

		m_limiter = std::move(limiter);

		m_signal.notify_all();
	}

	void PvsSearcher::stop()
	{
		m_stop.store(true, std::memory_order::relaxed);
	}

	bool PvsSearcher::searching()
	{
		return m_flag.load(std::memory_order_seq_cst) == SearchFlag;
	}

	void PvsSearcher::clearHash()
	{
		m_table.clear();
	}

	void PvsSearcher::setHashSize(size_t size)
	{
		m_table.resize(size);
	}

	void PvsSearcher::run(ThreadData &data)
	{
		while (true)
		{
			i32 flag{};

			std::unique_lock lock{m_waitMutex};
			m_signal.wait(lock, [this, &flag] { return flag = m_flag.load(std::memory_order_seq_cst); });

			if (flag == QuitFlag)
				return;

			auto &searchData = data.search;

			Score score{};
			Move best{};

			m_stop.store(false, std::memory_order::seq_cst);

			const auto startTime = util::g_timer.time();

			for (i32 depth = 1; depth <= data.maxDepth && !shouldStop(searchData); ++depth)
			{
				searchData.depth = depth;

				const auto prevBest = best;

				bool reportThisIter = data.id == 0;

				if (depth < MinAspDepth)
				{
					const auto newScore = search(data, depth, 0, -ScoreMax, ScoreMax);

					if (depth > 1 && m_stop.load(std::memory_order::relaxed) || !data.search.move)
						break;

					score = newScore;
					best = data.search.move;
				}
				else
				{
					auto aspDepth = depth;

					auto delta = InitialWindow;

					auto alpha = score - delta;
					auto beta = score + delta;

					while (!shouldStop(searchData))
					{
						if (aspDepth < depth - 3)
							aspDepth = depth - 3;

						const auto newScore = search(data, aspDepth, 0, alpha, beta);

						const bool stop = m_stop.load(std::memory_order::relaxed);
						if (stop || !searchData.move)
						{
							reportThisIter &= !stop;
							break;
						}

						score = newScore;

						if (data.id == 0 && (score <= alpha || score >= beta))
						{
							const auto time = util::g_timer.time() - startTime;
							if (time > MinReportDelay)
								report(data, best ?: searchData.move, time, score, alpha, beta);
						}

						delta += delta / 2;

						if (delta > MaxWindow)
							delta = ScoreMate;

						if (score >= beta)
						{
							beta += delta;
							--aspDepth;
						}
						else if (score <= alpha)
						{
							beta = (alpha + beta) / 2;
							alpha = std::max(alpha - delta, -ScoreMate);
							aspDepth = depth;
						}
						else
						{
							best = searchData.move;
							break;
						}
					}
				}

				m_limiter->update(data.search, prevBest == best);

				if (reportThisIter && depth < data.maxDepth)
				{
					if (const auto move = best ?: searchData.move)
						report(data, move, util::g_timer.time() - startTime, score, -ScoreMax, ScoreMax);
					else
					{
						std::cout << "info string no legal moves" << std::endl;
						break;
					}
				}
			}

			if (data.id == 0)
			{
				if (const auto move = best ?: searchData.move)
				{
					report(data, move, util::g_timer.time() - startTime, score, -ScoreMax, ScoreMax);
					std::cout << "bestmove " << uci::moveToString(move) << std::endl;
				}

				m_flag.store(IdleFlag, std::memory_order::seq_cst);
			}
		}
	}

	bool PvsSearcher::shouldStop(const SearchData &data)
	{
		if (m_stop.load(std::memory_order_relaxed))
			return true;

		bool shouldStop = m_limiter->stop(data);
		return m_stop.fetch_or(shouldStop, std::memory_order_relaxed) || shouldStop;
	}

	Score PvsSearcher::search(ThreadData &data, i32 depth, i32 ply, Score alpha, Score beta)
	{
		if (depth > 1 && shouldStop(data.search))
			return beta;

		auto &pos = data.pos;

		if (depth == 0 && !pos.isCheck())
			return qsearch(data, alpha, beta, ply);

		++data.search.nodes;

		const auto us = pos.toMove();
		const auto them = oppColor(us);

		const bool root = ply == 0;
		const bool pv = root || beta - alpha > 1;

		const bool inCheck = pos.isCheck();

		auto &stack = data.stack[ply];

		const auto newBaseDepth = depth > 0 ? depth - 1 : depth;
		++ply;

		if (ply > data.search.seldepth)
			data.search.seldepth = ply;

		// mate distance pruning
		if (!pv)
		{
			const auto mdAlpha = std::max(alpha, -ScoreMate + ply);
			const auto mdBeta = std::min(beta, ScoreMate - ply - 1);

			if (mdAlpha >= mdBeta)
				return mdAlpha;
		}

		TTableEntry entry{};
		auto hashMove = NullMove;

		if (m_table.probe(entry, pos.key(), depth, alpha, beta) && !pv)
			return entry.score;
		else if (entry.move && pos.isPseudolegal(entry.move))
			hashMove = entry.move;

		const bool tableHit = !hashMove.isNull();

		if (!root && !pos.lastMove())
			stack.eval = eval::flipTempo(-data.stack[ply - 1].eval);
		else stack.eval = inCheck ? 0
			: (entry.score != 0 ? entry.score : eval::staticEval(pos, &data.pawnCache));

		if (!pv && !inCheck)
		{
			const bool nmpFailsLow = tableHit && (entry.type == EntryType::Alpha) && entry.score < beta;

			// nullmove pruning (~66 elo)
			if (depth >= MinNullmoveDepth
				&& stack.eval >= beta
				&& !nmpFailsLow
				&& pos.lastMove()
				&& !pos.nonPk(us).empty())
			{
				const auto R = std::min(newBaseDepth - 1, 3);

				const auto guard = pos.applyMove(NullMove);
				const auto score = -search(data, newBaseDepth - R, ply, -beta, -beta + 1);

				if (score >= beta)
				{
					if (score > (ScoreMate / 2))
						return beta;
					return score;
				}
			}
		}

		// check extension
		if (inCheck)
			++depth;

		auto best = NullMove;
		auto bestScore = -ScoreMax;

		auto entryType = EntryType::Alpha;

		MoveGenerator generator{pos, stack.movegen, hashMove};
		u32 legalMoves = 0;

		while (const auto move = generator.next())
		{
#ifndef NDEBUG
			const auto savedPos = pos;
			{
#endif

			auto guard = pos.applyMove(move);

			if (pos.isAttacked(pos.king(us), them))
				continue;

			++legalMoves;

			Score score{};

			if (pos.isDrawn())
				score = 2 - static_cast<Score>(data.search.nodes % 4);
			else
			{
				auto newDepth = newBaseDepth;

				// lmr
				if (depth >= MinLmrDepth
					&& !inCheck // we are in check
					&& !pos.isCheck() // this move gives check
					&& generator.stage() >= MovegenStage::Quiet)
				{
					auto lmr = LmrTable[depth][legalMoves];

					if (pv)
						lmr = std::max(1, lmr - 1);

					newDepth = std::clamp(newDepth - lmr, 1, newDepth);
				}

				if (pv && legalMoves == 1)
					score = -search(data, newDepth, ply, -beta, -alpha);
				else
				{
					score = -search(data, newDepth, ply, -alpha - 1, -alpha);

					if (score > alpha && score < beta)
						score = -search(data, newDepth, ply, -beta, -alpha);
				}
			}

			if (score > bestScore)
			{
				best = move;
				bestScore = score;

				if (score > alpha)
				{
					if (score >= beta)
					{
						if (generator.stage() >= MovegenStage::Quiet
							&& move != stack.movegen.killer1)
						{
							stack.movegen.killer2 = stack.movegen.killer1;
							stack.movegen.killer1 = move;
						}

						entryType = EntryType::Beta;
						break;
					}

					alpha = score;
					entryType = EntryType::Exact;
				}
			}

#ifndef NDEBUG
			}
			if (!m_stop && pos != savedPos)
			{
				std::cerr << "corrupt board state" << std::endl;
				pos.printHistory(move);
				m_stop = true;
				return beta;
			}
#endif
		}

		if (legalMoves == 0)
			return pos.isCheck() ? (-ScoreMate + ply) : 0;

		m_table.put(pos.key(), bestScore, best, depth, entryType);

		if (root && (!m_stop || !data.search.move))
			data.search.move = best;

		return bestScore;
	}

	Score PvsSearcher::qsearch(ThreadData &data, Score alpha, Score beta, i32 ply)
	{
		if (shouldStop(data.search))
			return beta;

		auto &pos = data.pos;

		++data.search.nodes;

		auto staticEval = eval::staticEval(pos, &data.pawnCache);

		if (staticEval > alpha)
		{
			if (staticEval >= beta)
				return staticEval;

			alpha = staticEval;
		}

		const auto us = pos.toMove();

		auto &stack = data.stack[ply];

		++ply;

		if (ply > data.search.seldepth)
			data.search.seldepth = ply;

		auto bestScore = staticEval;

		auto hashMove = m_table.probeMove(pos.key());
		if (hashMove && !pos.isPseudolegal(hashMove))
			hashMove = NullMove;

		QMoveGenerator generator{pos, stack.movegen, hashMove};

		while (const auto move = generator.next())
		{
			auto guard = pos.applyMove(move);

			if (pos.isAttacked(pos.king(us), oppColor(us)))
				continue;

			const auto score = pos.isDrawn() ? 0 : -qsearch(data, -beta, -alpha, ply);

			if (score > bestScore)
			{
				bestScore = score;

				if (score > alpha)
					alpha = score;

				if (score >= beta)
					break;
			}
		}

		return bestScore;
	}

	void PvsSearcher::report(const ThreadData &data, Move move, f64 time, Score score, Score alpha, Score beta)
	{
		size_t nodes = 0;

		// technically a potential race but it doesn't matter
		for (const auto &thread : m_threads)
		{
			nodes += thread.search.nodes;
		}

		const auto ms = static_cast<size_t>(time * 1000.0);
		const auto nps = static_cast<size_t>(static_cast<f64>(nodes) / time);

		std::cout << "info depth " << data.search.depth << " seldepth " << data.search.seldepth
			<< " time " << ms << " nodes " << nodes << " nps " << nps << " score ";

		score = std::clamp(score, alpha, beta);

		if (std::abs(score) > ScoreMate / 2)
		{
			if (score > ScoreMate / 2)
				std::cout << "mate " << ((ScoreMate - score + 1) / 2);
			else std::cout << "mate " << (-(ScoreMate + score) / 2);
		}
		else std::cout << "cp " << score;

		if (score == alpha)
			std::cout << " upperbound";
		else if (score == beta)
			std::cout << " lowerbound";

		std::cout << " hashfull " << m_table.full() << " pv " << uci::moveToString(move);

		StaticVector<Move, MaxDepth> pvWritten{};
		pvWritten.push(move);

		Position pos{data.pos};
		pos.applyMoveUnchecked<false, false>(move);

		while (true)
		{
			const auto pvMove = m_table.probeMove(pos.key());

			if (pvMove
				&& std::find(pvWritten.begin(), pvWritten.end(), pvMove) == pvWritten.end()
				&& pos.isPseudolegal(pvMove))
			{
				std::cout << ' ' << uci::moveToString(pvMove);

				pvWritten.push(pvMove);
				pos.applyMoveUnchecked<false, false>(pvMove);
			}
			else break;
		}

		std::cout << std::endl;
	}
}
