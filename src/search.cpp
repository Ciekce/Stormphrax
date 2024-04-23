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

#include "search.h"

#include <iostream>
#include <cmath>
#include <cassert>

#include "uci.h"
#include "limit/trivial.h"
#include "opts.h"
#include "3rdparty/fathom/tbprobe.h"
#include "tb.h"
#include "see.h"

namespace stormphrax::search
{
	using namespace stormphrax::tunable;

	namespace
	{
		constexpr f64 MinReportDelay = 1.0;

		inline auto drawScore(usize nodes)
		{
			return 2 - static_cast<Score>(nodes % 4);
		}

		inline auto generateLegal(MoveList &moves, const Position &pos)
		{
			ScoredMoveList generated{};
			generateAll(generated, pos);

			for (const auto [move, _s] : generated)
			{
				if (pos.isLegal(move))
					moves.push(move);
			}
		}
	}

	Searcher::Searcher(usize ttSize)
		: m_ttable{ttSize}
	{
		auto &thread = m_threads.emplace_back();

		thread.id = m_nextThreadId++;
		thread.thread = std::thread{[this, &thread]
		{
			run(thread);
		}};
	}

	auto Searcher::newGame() -> void
	{
		m_ttable.clear();

		for (auto &thread : m_threads)
		{
			thread.history.clear();
		}
	}

	auto Searcher::startSearch(const Position &pos, i32 maxDepth,
		std::unique_ptr<limit::ISearchLimiter> limiter) -> void
	{
		if (!m_limiter && !limiter)
		{
			std::cerr << "missing limiter" << std::endl;
			return;
		}

		m_minRootScore = -ScoreInf;
		m_maxRootScore =  ScoreInf;

		bool tbRoot = false;
		MoveList rootMoves{};

		if (g_opts.syzygyEnabled
			&& pos.bbs().occupancy().popcount()
				<= std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST)))
		{
			tbRoot = true;
			const auto wdl = tb::probeRoot(rootMoves, pos);

			switch (wdl)
			{
			case tb::ProbeResult::Win:
				m_minRootScore = ScoreTbWin;
				break;
			case tb::ProbeResult::Draw:
				m_minRootScore = m_maxRootScore = 0;
				break;
			case tb::ProbeResult::Loss:
				m_maxRootScore = -ScoreTbWin;
				break;
			default:
				tbRoot = false;
				break;
			}
		}

		if (rootMoves.empty())
			generateLegal(rootMoves, pos);

		m_resetBarrier.arriveAndWait();

		if (limiter)
			m_limiter = std::move(limiter);

		const auto contempt = g_opts.contempt;

		m_contempt[static_cast<i32>(pos.  toMove())] =  contempt;
		m_contempt[static_cast<i32>(pos.opponent())] = -contempt;

		for (auto &thread : m_threads)
		{
			thread.maxDepth = maxDepth;
			thread.search = SearchData{};
			thread.pos = pos;

			thread.rootMoves = rootMoves;

			thread.nnueState.reset(thread.pos.bbs(), thread.pos.blackKing(), thread.pos.whiteKing());
		}

		if (tbRoot)
			m_threads[0].search.tbhits = 1;

		m_stop.store(false, std::memory_order::seq_cst);
		m_runningThreads.store(static_cast<i32>(m_threads.size()));

		m_searching.store(true, std::memory_order::relaxed);

		m_idleBarrier.arriveAndWait();
	}

	auto Searcher::stop() -> void
	{
		m_stop.store(true, std::memory_order::relaxed);

		// safe, always runs from uci thread
		if (m_runningThreads.load() > 0)
		{
			std::unique_lock lock{m_stopMutex};
			m_stopSignal.wait(lock, [this]
			{
				return m_runningThreads.load(std::memory_order::seq_cst) == 0;
			});
		}
	}

	auto Searcher::runDatagenSearch(ThreadData &thread) -> std::pair<Score, Score>
	{
		thread.rootMoves.clear();
		generateLegal(thread.rootMoves, thread.pos);

		m_stop.store(false, std::memory_order::seq_cst);

		const auto score = searchRoot(thread, false);

		const auto whitePovScore = thread.pos.toMove() == Color::Black ? -score : score;
		return {whitePovScore, wdl::normalizeScoreMove32(whitePovScore)};
	}

	auto Searcher::runBench(BenchData &data, const Position &pos, i32 depth) -> void
	{
		m_limiter = std::make_unique<limit::InfiniteLimiter>();
		m_contempt = {};

		// this struct is a small boulder the size of a large boulder
		// and overflows the stack if not on the heap
		auto thread = std::make_unique<ThreadData>();

		thread->pos = pos;
		thread->maxDepth = depth;

		thread->nnueState.reset(thread->pos.bbs(), thread->pos.blackKing(), thread->pos.whiteKing());

		thread->rootMoves.clear();
		generateLegal(thread->rootMoves, thread->pos);

		m_stop.store(false, std::memory_order::seq_cst);

		const auto start = util::g_timer.time();

		searchRoot(*thread, false);

		const auto time = util::g_timer.time() - start;

		data.search = thread->search;
		data.time = time;
	}

	auto Searcher::setThreads(u32 threads) -> void
	{
		if (threads != m_threads.size())
		{
			stopThreads();

			m_quit.store(false, std::memory_order::seq_cst);

			m_threads.clear();
			m_threads.shrink_to_fit();
			m_threads.reserve(threads);

			m_nextThreadId = 0;

			m_resetBarrier.reset(threads + 1);
			m_idleBarrier.reset(threads + 1);

			m_searchEndBarrier.reset(threads);

			for (i32 i = 0; i < threads; ++i)
			{
				auto &thread = m_threads.emplace_back();

				thread.id = m_nextThreadId++;
				thread.thread = std::thread{[this, &thread]
				{
					run(thread);
				}};
			}
		}
	}

	auto Searcher::stopThreads() -> void
	{
		m_quit.store(true, std::memory_order::release);
		m_resetBarrier.arriveAndWait();
		m_idleBarrier.arriveAndWait();

		for (auto &thread : m_threads)
		{
			thread.thread.join();
		}
	}

	auto Searcher::run(ThreadData &thread) -> void
	{
		while (true)
		{
			m_resetBarrier.arriveAndWait();
			m_idleBarrier.arriveAndWait();

			if (m_quit.load(std::memory_order::acquire))
				return;

			searchRoot(thread, true);
		}
	}

	auto Searcher::searchRoot(ThreadData &thread, bool actualSearch) -> Score
	{
		auto &searchData = thread.search;

		const bool mainThread = actualSearch && thread.isMainThread();

		thread.rootPv.moves[0] = NullMove;
		thread.rootPv.length = 0;

		auto score = -ScoreInf;
		PvList pv{};

		const auto startTime = mainThread ? util::g_timer.time() : 0.0;
		const auto startDepth = 1 + static_cast<i32>(thread.id) % 16;

		i32 depthCompleted{};

		bool hitSoftTimeout = false;

		for (i32 depth = startDepth;
			depth <= thread.maxDepth
				// imperfect
				&& !(hitSoftTimeout = shouldStop(searchData, thread.isMainThread(), true));
			++depth)
		{
			searchData.depth = depth;
			searchData.seldepth = 0;

			Score delta{};

			auto alpha = -ScoreInf;
			auto beta = ScoreInf;

			if (depth >= minAspDepth())
			{
				delta = initialAspWindow();

				alpha = std::max(score - delta, -ScoreInf);
				beta  = std::min(score + delta,  ScoreInf);
			}

			Score newScore{};

			while (!shouldStop(searchData, false, false))
			{
				newScore = search<true>(thread, thread.rootPv, depth, 0, 0, alpha, beta, false);

				if (newScore <= alpha)
				{
					beta = (alpha + beta) / 2;
					alpha = std::max(newScore - delta, -ScoreInf);
				}
				else if (newScore >= beta)
					beta = std::min(newScore + delta, ScoreInf);
				else break;

				delta += delta * aspWideningFactor() / 16;
			}

			if (thread.rootPv.length == 0
				|| depth > 1 && m_stop.load(std::memory_order::relaxed))
				break;

			depthCompleted = depth;

			score = newScore;
			pv.copyFrom(thread.rootPv);

			if (mainThread)
			{
				m_limiter->update(thread.search, pv.moves[0], thread.search.nodes);

				if (depth < thread.maxDepth)
				{
					if (pv.length == 0)
						pv.copyFrom(thread.rootPv);

					if (pv.length > 0)
					{
						const auto time = util::g_timer.time() - startTime;
						report(thread, pv, searchData.depth, time, score);
					}
					else break;
				}
			}
		}

		if (pv.length == 0)
		{
			std::cout << "info string no legal moves" << std::endl;
			return -ScoreMate;
		}

		thread.history.clear();

		const auto waitForThreads = [&]
		{
			--m_runningThreads;
			m_stopSignal.notify_all();

			m_searchEndBarrier.arriveAndWait();
		};

		if (mainThread)
		{
			const std::unique_lock lock{m_searchMutex};

			m_stop.store(true, std::memory_order::seq_cst);
			waitForThreads();

			finalReport(startTime, thread, pv, score, depthCompleted, hitSoftTimeout);

			m_searching.store(false, std::memory_order::relaxed);
		}
		else waitForThreads();

		return score;
	}

	template <bool RootNode>
	auto Searcher::search(ThreadData &thread, PvList &pv, i32 depth,
		i32 ply, u32 moveStackIdx, Score alpha, Score beta, bool cutnode) -> Score
	{
		assert(ply >= 0 && ply <= MaxDepth);
		assert(RootNode || ply > 0);

		if (ply > 0 && shouldStop(thread.search, thread.isMainThread(), false))
			return 0;

		auto &pos = thread.pos;
		const auto &boards = pos.boards();
		const auto &bbs = pos.bbs();

		if (ply >= MaxDepth)
			return eval::staticEval(pos, thread.nnueState, m_contempt);

		if (!RootNode)
		{
			alpha = std::max(alpha, -ScoreMate + ply);
			beta  = std::min( beta,  ScoreMate - ply - 1);

			if (alpha >= beta)
				return alpha;
		}

		const bool inCheck = pos.isCheck();

		if (depth <= 0 && !inCheck)
			return qsearch(thread, ply, moveStackIdx, alpha, beta);

		if (depth < 0)
			depth = 0;

		const auto us = pos.toMove();
		const auto them = oppColor(us);

		const bool pvNode = beta - alpha > 1;

		assert(!pvNode || !cutnode);

		auto &curr = thread.stack[ply];
		const auto *parent = RootNode ? nullptr : &thread.stack[ply - 1];

		auto &moveStack = thread.moveStack[moveStackIdx];

		if (ply > thread.search.seldepth)
			thread.search.seldepth = ply;

		ProbedTTableEntry ttEntry{};
		m_ttable.probe(ttEntry, pos.key(), ply);

		if (!pvNode
			&& ttEntry.depth >= depth
			&& (ttEntry.flag == TtFlag::Exact
				|| ttEntry.flag == TtFlag::UpperBound && ttEntry.score <= alpha
				|| ttEntry.flag == TtFlag::LowerBound && ttEntry.score >= beta))
			return ttEntry.score;

		const auto pieceCount = bbs.occupancy().popcount();

		auto syzygyMin = -ScoreMate;
		auto syzygyMax =  ScoreMate;

		const auto syzygyPieceLimit = std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST));

		// Probe the Syzygy tablebases for a WDL result
		// if there are few enough pieces left on the board
		if (!RootNode
			&& g_opts.syzygyEnabled
			&& pieceCount <= syzygyPieceLimit
			&& (pieceCount < syzygyPieceLimit || depth >= g_opts.syzygyProbeDepth)
			&& pos.halfmove() == 0
			&& pos.castlingRooks() == CastlingRooks{})
		{
			const auto result = tb::probe(pos);

			if (result != tb::ProbeResult::Failed)
			{
				++thread.search.tbhits;

				Score score{};
				TtFlag entryType{};

				if (result == tb::ProbeResult::Win)
				{
					score = ScoreTbWin - ply;
					entryType = TtFlag::LowerBound;
				}
				else if (result == tb::ProbeResult::Loss)
				{
					score = -ScoreTbWin + ply;
					entryType = TtFlag::UpperBound;
				}
				else // draw
				{
					score = drawScore(thread.search.nodes);
					entryType = TtFlag::Exact;
				}

				return score;
			}
		}

		if (depth >= minIirDepth()
			&& (pvNode || cutnode)
			&& !ttEntry.move)
			--depth;

		const auto staticEval = eval::staticEval(pos, thread.nnueState, m_contempt);

		if (!pvNode
			&& !inCheck)
		{
			if (depth <= maxRfpDepth()
				&& staticEval - rfpMargin() * depth >= beta)
				return staticEval;

			if (depth <= maxRazoringDepth()
				&& std::abs(alpha) < 2000
				&& staticEval + razoringMargin() * depth <= alpha)
			{
				const auto score = qsearch(thread, ply, moveStackIdx, alpha, alpha + 1);

				if (score <= alpha)
					return score;
			}

			if (depth >= minNmpDepth()
				&& staticEval >= beta
				&& !parent->move.isNull()
				&& !bbs.nonPk(us).empty())
			{
				const auto R = nmpBaseReduction()
					+ depth / nmpDepthReductionDiv();

				curr.move = NullMove;
				const auto guard = pos.applyNullMove();

				const auto score = -search(thread, curr.pv, depth - R,
					ply + 1, moveStackIdx, -beta, -beta + 1, !cutnode);

				if (score >= beta)
					return score > ScoreWin ? beta : score;
			}
		}

		moveStack.failLowQuiets .clear();
		moveStack.failLowNoisies.clear();

		auto bestMove = NullMove;
		auto bestScore = -ScoreInf;

		auto ttFlag = TtFlag::UpperBound;

		auto generator = mainMoveGenerator(pos, moveStack.movegenData, ttEntry.move, thread.history);

		u32 legalMoves = 0;

		while (const auto move = generator.next())
		{
			if constexpr (RootNode)
			{
				if (!thread.isLegalRootMove(move))
					continue;

				assert(pos.isLegal(move));
			}
			else if (!pos.isLegal(move))
				continue;

			const bool noisy = pos.isNoisy(move);

			if (bestScore > -ScoreWin)
			{
				if (!noisy
					&& depth <= maxFpDepth()
					&& std::abs(alpha) < 2000
					&& staticEval + fpMargin() + depth * fpScale() <= alpha)
				{
					generator.skipQuiets();
					continue;
				}

				const auto seeThreshold = noisy
					? seePruningThresholdNoisy() * depth
					: seePruningThresholdQuiet() * depth * depth;

				if (!see::see(pos, move, seeThreshold))
					continue;
			}

			if (pvNode)
				curr.pv.length = 0;

			const auto prevNodes = thread.search.nodes;

			++thread.search.nodes;
			++legalMoves;

			curr.move = move;
			const auto guard = pos.applyMove(move, &thread.nnueState);

			Score score{};

			if (pos.isDrawn(true))
				score = drawScore(thread.search.nodes);
			else
			{
				const auto newDepth = depth - 1;

				if (legalMoves == 1)
					score = -search(thread, curr.pv, newDepth, ply + 1, moveStackIdx + 1, -beta, -alpha, false);
				else
				{
					const auto reduction = [&]
					{
						if (depth < minLmrDepth()
							|| legalMoves < lmrMinMoves()
							|| generator.stage() < MovegenStage::Quiet)
							return 0;

						auto r =  g_lmrTable[depth][legalMoves];

						r += !pvNode;

						return r;
					}();

					// can't use std::clamp because newDepth can be <0
					const auto reduced = std::min(std::max(newDepth - reduction, 0), newDepth);
					score = -search(thread, curr.pv, reduced, ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, true);

					if (score > alpha && reduced < newDepth)
						score = -search(thread, curr.pv, newDepth, ply + 1,
							moveStackIdx + 1, -alpha - 1, -alpha, !cutnode);

					if (score > alpha && score < beta)
						score = -search(thread, curr.pv, newDepth, ply + 1, moveStackIdx + 1, -beta, -alpha, false);
				}
			}

			if constexpr (RootNode)
			{
				if (thread.isMainThread())
					m_limiter->updateMoveNodes(move, thread.search.nodes - prevNodes);
			}

			if (score > bestScore)
				bestScore = score;

			if (score > alpha)
			{
				alpha = score;
				bestMove = move;

				if (pvNode)
				{
					pv.moves[0] = move;

					assert(curr.pv.length + 1 <= MaxDepth);

					std::copy(curr.pv.moves.begin(),
						curr.pv.moves.begin() + curr.pv.length,
						pv.moves.begin() + 1);
					pv.length = curr.pv.length + 1;

					assert(pv.length == 1 || pv.moves[0] != pv.moves[1]);
				}

				ttFlag = TtFlag::Exact;
			}

			if (score >= beta)
			{
				ttFlag = TtFlag::LowerBound;
				break;
			}

			if (move != bestMove)
			{
				if (noisy)
					moveStack.failLowNoisies.push(move);
				else moveStack.failLowQuiets.push(move);
			}
		}

		if (legalMoves == 0)
			return inCheck ? (-ScoreMate + ply) : 0;

		if (bestMove)
		{
			const auto bonus = historyBonus(depth);
			const auto penalty = -bonus;

			if (!pos.isNoisy(bestMove))
			{
				thread.history.updateQuietScore(pos.threats(), bestMove, bonus);

				for (const auto prevQuiet : moveStack.failLowQuiets)
				{
					thread.history.updateQuietScore(pos.threats(), prevQuiet, penalty);
				}
			}
			else
			{
				const auto captured = pos.captureTarget(bestMove);
				thread.history.updateNoisyScore(bestMove, captured, bonus);
			}

			// unconditionally update capthist
			for (const auto prevNoisy : moveStack.failLowNoisies)
			{
				const auto captured = pos.captureTarget(prevNoisy);
				thread.history.updateNoisyScore(prevNoisy, captured, penalty);
			}
		}

		bestScore = std::clamp(bestScore, syzygyMin, syzygyMax);

		if (!shouldStop(thread.search, false, false))
			m_ttable.put(pos.key(), bestScore, bestMove, depth, ply, ttFlag);

		return bestScore;
	}

	auto Searcher::qsearch(ThreadData &thread, i32 ply, u32 moveStackIdx, Score alpha, Score beta) -> Score
	{
		assert(ply >= 0 && ply <= MaxDepth);

		if (ply > 0 && shouldStop(thread.search, thread.isMainThread(), false))
			return 0;

		auto &pos = thread.pos;

		if (ply >= MaxDepth)
			return eval::staticEval(pos, thread.nnueState, m_contempt);

		if (ply > thread.search.seldepth)
			thread.search.seldepth = ply;

		Score staticEval;

		if (pos.isCheck())
			staticEval = -ScoreMate + ply;
		else
		{
			staticEval = eval::staticEval(pos, thread.nnueState, m_contempt);

			if (staticEval >= beta)
				return staticEval;

			if (staticEval > alpha)
				alpha = staticEval;
		}

		auto bestScore = staticEval;

		auto generator = qsearchMoveGenerator(pos, thread.moveStack[moveStackIdx].movegenData, thread.history);

		while (const auto move = generator.next())
		{
			if (!pos.isLegal(move))
				continue;

			if (!see::see(pos, move, qsearchSeeThreshold()))
				continue;

			++thread.search.nodes;

			const auto guard = pos.applyMove(move, &thread.nnueState);

			const auto score = -qsearch(thread, ply + 1, moveStackIdx + 1, -beta, -alpha);

			if (score > bestScore)
			{
				bestScore = score;

				if (score > alpha)
				{
					alpha = score;

					if (score >= beta)
						break;
				}
			}
		}

		return bestScore;
	}

	auto Searcher::report(const ThreadData &mainThread, const PvList &pv, i32 depth, f64 time, Score score) -> void
	{
		usize nodes = 0;
		i32 seldepth = 0;

		// technically a potential race but it doesn't matter
		for (const auto &thread : m_threads)
		{
			nodes += thread.search.nodes;
			seldepth = std::max(seldepth, thread.search.seldepth);
		}

		const auto ms  = static_cast<usize>(time * 1000.0);
		const auto nps = static_cast<usize>(static_cast<f64>(nodes) / time);

		std::cout << "info depth " << depth << " seldepth " << seldepth
			<< " time " << ms << " nodes " << nodes << " nps " << nps << " score ";

		if (std::abs(score) <= 2) // draw score
			score = 0;

		score = std::clamp(score, m_minRootScore, m_maxRootScore);

		const auto plyFromStartpos = mainThread.pos.plyFromStartpos();

		// mates
		if (std::abs(score) >= ScoreMaxMate)
		{
			if (score > 0)
				std::cout << "mate " << ((ScoreMate - score + 1) / 2);
			else std::cout << "mate " << (-(ScoreMate + score) / 2);
		}
		else
		{
			// adjust score to 100cp == 50% win probability
			const auto normScore = wdl::normalizeScore(score, plyFromStartpos);
			std::cout << "cp " << normScore;
		}

		// wdl display
		if (g_opts.showWdl)
		{
			if (score > ScoreWin)
				std::cout << " wdl 1000 0 0";
			else if (score < -ScoreWin)
				std::cout << " wdl 0 0 1000";
			else
			{
				const auto [wdlWin, wdlLoss] = wdl::wdlModel(score, plyFromStartpos);
				const auto wdlDraw = 1000 - wdlWin - wdlLoss;

				std::cout << " wdl " << wdlWin << " " << wdlDraw << " " << wdlLoss;
			}
		}

		std::cout << " hashfull " << m_ttable.full();

		if (g_opts.syzygyEnabled)
		{
			usize tbhits = 0;

			// technically a potential race but it doesn't matter
			for (const auto &thread : m_threads)
			{
				tbhits += thread.search.tbhits;
			}

			std::cout << " tbhits " << tbhits;
		}

		std::cout << " pv";

		for (u32 i = 0; i < pv.length; ++i)
		{
			std::cout << ' ' << uci::moveToString(pv.moves[i]);
		}

		std::cout << std::endl;
	}

	auto Searcher::finalReport(f64 startTime, const ThreadData &mainThread,
		const PvList &pv, Score score, i32 depthCompleted, bool softTimeout) -> void
	{
		if (!softTimeout || !m_limiter->stopped())
		{
			const auto time = util::g_timer.time() - startTime;
			report(mainThread, pv, depthCompleted, time, score);
		}

		std::cout << "bestmove " << uci::moveToString(pv.moves[0]) << std::endl;
	}
}
