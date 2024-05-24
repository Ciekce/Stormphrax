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
		constexpr f64 MinWidenReportDelay = 1.0;

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

		if (rootMoves.empty())
		{
			std::cout << "info string no legal moves" << std::endl;
			return;
		}

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

		if (thread.rootMoves.empty())
			return {-ScoreMate, -ScoreMate};

		m_stop.store(false, std::memory_order::seq_cst);

		const auto score = searchRoot(thread, false);

		m_ttable.age();

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

		if (thread->rootMoves.empty())
			return;

		m_stop.store(false, std::memory_order::seq_cst);

		const auto start = util::g_timer.time();

		searchRoot(*thread, false);

		m_ttable.age();

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
		assert(!thread.rootMoves.empty());

		auto &searchData = thread.search;

		const bool mainThread = actualSearch && thread.isMainThread();

		thread.rootPv.moves[0] = NullMove;
		thread.rootPv.length = 0;

		auto score = -ScoreInf;
		PvList pv{};

		const auto startTime = mainThread ? util::g_timer.time() : 0.0;
		const auto startDepth = 1 + static_cast<i32>(thread.id) % 16;

		const auto totalTime = [&]
		{
			return util::g_timer.time() - startTime;
		};

		searchData.nodes = 1;
		thread.stack[0].killers.clear();

		i32 depthCompleted{};

		for (i32 depth = startDepth;; ++depth)
		{
			searchData.depth = depth;
			searchData.seldepth = 0;

			auto delta = initialAspWindow();

			auto alpha = -ScoreInf;
			auto beta = ScoreInf;

			if (depth >= minAspDepth())
			{
				alpha = std::max(score - delta, -ScoreInf);
				beta  = std::min(score + delta,  ScoreInf);
			}

			Score newScore{};

			while (!hasStopped())
			{
				newScore = search<true, true>(thread, thread.rootPv, depth, 0, 0, alpha, beta, false);

				if ((newScore > alpha && newScore < beta) || hasStopped())
					break;

				if (mainThread)
				{
					const auto time = totalTime();
					if (time >= MinWidenReportDelay)
						report(thread, thread.rootPv, depth, time, newScore, alpha, beta);
				}

				if (newScore <= alpha)
				{
					beta = (alpha + beta) / 2;
					alpha = std::max(newScore - delta, -ScoreInf);
				}
				else beta = std::min(newScore + delta, ScoreInf);

				delta += delta * aspWideningFactor() / 16;
			}

			assert(thread.rootPv.length > 0);

			if (hasStopped())
				break;

			depthCompleted = depth;

			score = newScore;
			pv = thread.rootPv;

			if (depth >= thread.maxDepth)
				break;

			if (mainThread)
			{
				m_limiter->update(thread.search, pv.moves[0], thread.search.nodes);

				if (checkSoftTimeout(thread.search, true))
					break;

				report(thread, pv, searchData.depth, totalTime(), score);
			}
			else if (checkSoftTimeout(thread.search, false))
				break;
		}

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

			finalReport(thread, pv, depthCompleted, totalTime(), score);

			m_ttable.age();

			m_searching.store(false, std::memory_order::relaxed);
		}
		else waitForThreads();

		return score;
	}

	template <bool PvNode, bool RootNode>
	auto Searcher::search(ThreadData &thread, PvList &pv, i32 depth,
		i32 ply, u32 moveStackIdx, Score alpha, Score beta, bool cutnode) -> Score
	{
		assert(ply >= 0 && ply <= MaxDepth);
		assert(RootNode || ply > 0);
		assert(PvNode || alpha + 1 == beta);

		if (ply > 0 && checkHardTimeout(thread.search, thread.isMainThread()))
			return 0;

		auto &pos = thread.pos;
		const auto &boards = pos.boards();
		const auto &bbs = pos.bbs();

		if constexpr (!RootNode)
		{
			alpha = std::max(alpha, -ScoreMate + ply);
			beta  = std::min( beta,  ScoreMate - ply - 1);

			if (alpha >= beta)
				return alpha;

			if (alpha < 0 && pos.hasCycle(ply))
			{
				alpha = drawScore(thread.search.nodes);
				if (alpha >= beta)
					return alpha;
			}
		}

		const bool inCheck = pos.isCheck();

		if (depth <= 0 && !inCheck)
			return qsearch<PvNode>(thread, ply, moveStackIdx, alpha, beta);

		if (depth < 0)
			depth = 0;

		if (ply + 1 > thread.search.seldepth)
			thread.search.seldepth = ply + 1;

		if (ply >= MaxDepth)
			return inCheck ? 0 : eval::staticEval(pos, thread.nnueState, m_contempt);

		const auto us = pos.toMove();
		const auto them = oppColor(us);

		assert(!PvNode || !cutnode);

		const auto *parent = RootNode ? nullptr : &thread.stack[ply - 1];
		auto &curr = thread.stack[ply];

		assert(!RootNode || curr.excluded == NullMove);

		auto &moveStack = thread.moveStack[moveStackIdx];

		ProbedTTableEntry ttEntry{};

		if (!curr.excluded)
		{
			m_ttable.probe(ttEntry, pos.key(), ply);

			if (!PvNode && ttEntry.depth >= depth)
			{
				if (ttEntry.flag == TtFlag::Exact
					|| ttEntry.flag == TtFlag::UpperBound && ttEntry.score <= alpha
					|| ttEntry.flag == TtFlag::LowerBound && ttEntry.score >= beta)
					return ttEntry.score;
				else if (depth <= maxTtNonCutoffExtDepth())
					++depth;
			}
		}

		const bool ttHit = ttEntry.flag != TtFlag::None;
		const bool ttMoveNoisy = ttEntry.move && pos.isNoisy(ttEntry.move);
		const bool ttpv = PvNode || ttEntry.wasPv;

		const auto pieceCount = bbs.occupancy().popcount();

		auto syzygyMin = -ScoreMate;
		auto syzygyMax =  ScoreMate;

		const auto syzygyPieceLimit = std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST));

		// Probe the Syzygy tablebases for a WDL result
		// if there are few enough pieces left on the board
		if (!RootNode
			&& !curr.excluded
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
				TtFlag flag{};

				if (result == tb::ProbeResult::Win)
				{
					score = ScoreTbWin - ply;
					flag = TtFlag::LowerBound;
				}
				else if (result == tb::ProbeResult::Loss)
				{
					score = -ScoreTbWin + ply;
					flag = TtFlag::UpperBound;
				}
				else // draw
				{
					score = drawScore(thread.search.nodes);
					flag = TtFlag::Exact;
				}

				if (flag == TtFlag::Exact
					|| flag == TtFlag::UpperBound && score <= alpha
					|| flag == TtFlag::LowerBound && score >= beta)
				{
					m_ttable.put(pos.key(), score, ScoreNone, NullMove, depth, ply, flag, ttpv);
					return score;
				}

				if constexpr (PvNode)
				{
					if (flag == TtFlag::UpperBound)
						syzygyMax = score;
					else if (flag == TtFlag::LowerBound)
					{
						if (score > alpha)
							alpha = score;
						syzygyMin = score;
					}
				}
			}
		}

		if (depth >= minIirDepth()
			&& !curr.excluded
			&& (PvNode || cutnode)
			&& !ttEntry.move)
			--depth;

		if (!curr.excluded)
		{
			if (inCheck)
				curr.staticEval = ScoreNone;
			else if (ttHit && ttEntry.staticEval != ScoreNone)
				curr.staticEval = ttEntry.staticEval;
			else curr.staticEval = eval::staticEval(pos, thread.nnueState, m_contempt);
		}

		const bool improving = [&]
		{
			if (inCheck)
				return false;
			if (ply > 1 && thread.stack[ply - 2].staticEval != ScoreNone)
				return curr.staticEval > thread.stack[ply - 2].staticEval;
			if (ply > 3 && thread.stack[ply - 4].staticEval != ScoreNone)
				return curr.staticEval > thread.stack[ply - 4].staticEval;
			return true;
		}();

		if (!PvNode
			&& !inCheck
			&& !curr.excluded)
		{
			if (depth <= maxRfpDepth()
				&& curr.staticEval - rfpMargin() * std::max(depth - improving, 0) >= beta)
				return curr.staticEval;

			if (depth <= maxRazoringDepth()
				&& std::abs(alpha) < 2000
				&& curr.staticEval + razoringMargin() * depth <= alpha)
			{
				const auto score = qsearch(thread, ply, moveStackIdx, alpha, alpha + 1);

				if (score <= alpha)
					return score;
			}

			if (depth >= minNmpDepth()
				&& curr.staticEval >= beta
				&& !parent->move.isNull()
				&& !bbs.nonPk(us).empty())
			{
				m_ttable.prefetch(pos.key() ^ keys::color());

				const auto R = nmpBaseReduction()
					+ depth / nmpDepthReductionDiv();

				thread.setNullmove(ply);
				const auto guard = pos.applyNullMove();

				const auto score = -search(thread, curr.pv, depth - R,
					ply + 1, moveStackIdx, -beta, -beta + 1, !cutnode);

				if (score >= beta)
					return score > ScoreWin ? beta : score;
			}

			const auto probcutBeta = beta + probcutMargin();
			const auto probcutDepth = std::max(depth - probcutReduction(), 1);

			if (!ttpv
				&& depth >= minProbcutDepth()
				&& std::abs(beta) < ScoreWin
				&& (!ttEntry.move || ttMoveNoisy)
				&& !(ttHit && ttEntry.depth >= probcutDepth && ttEntry.score < probcutBeta))
			{
				const auto seeThreshold = (probcutBeta - curr.staticEval) * probcutSeeScale() / 16;
				const auto keyBefore = pos.key();

				auto generator = MoveGenerator::probcut(pos, ttEntry.move, moveStack.movegenData, thread.history);

				while (const auto move = generator.next())
				{
					if (!pos.isLegal(move))
						continue;

					if (!see::see(pos, move, seeThreshold))
						continue;

					++thread.search.nodes;

					m_ttable.prefetch(pos.roughKeyAfter(move));

					thread.setMove(ply, move);
					const auto guard = pos.applyMove(move, &thread.nnueState);

					auto score = -qsearch(thread, ply + 1, moveStackIdx + 1, -probcutBeta, -probcutBeta + 1);

					if (score >= probcutBeta)
						score = -search(thread, curr.pv, probcutDepth - 1, ply + 1,
							moveStackIdx + 1, -probcutBeta, -probcutBeta + 1, !cutnode);

					if (score >= probcutBeta)
					{
						m_ttable.put(keyBefore, score, curr.staticEval,
							move, probcutDepth, ply, TtFlag::LowerBound, false);
						return score;
					}
				}
			}
		}

		if constexpr (!RootNode)
			curr.multiExtensions = parent->multiExtensions;

		thread.stack[ply + 1].killers.clear();

		moveStack.failLowQuiets .clear();
		moveStack.failLowNoisies.clear();

		auto bestMove = NullMove;
		auto bestScore = -ScoreInf;

		auto ttFlag = TtFlag::UpperBound;

		auto generator = MoveGenerator::main(pos, moveStack.movegenData,
			ttEntry.move, curr.killers, thread.history, thread.conthist, ply);

		u32 legalMoves = 0;

		while (const auto move = generator.next())
		{
			if (move == curr.excluded)
				continue;

			if constexpr (RootNode)
			{
				if (!thread.isLegalRootMove(move))
					continue;

				assert(pos.isLegal(move));
			}
			else if (!pos.isLegal(move))
				continue;

			const bool quietOrLosing = generator.stage() > MovegenStage::GoodNoisy;

			const bool noisy = pos.isNoisy(move);
			const auto moving = boards.pieceAt(move.src());

			const auto captured = pos.captureTarget(move);

			const auto baseLmr = g_lmrTable[noisy][depth][legalMoves + 1];

			const auto history = noisy
				? thread.history.noisyScore(move, captured)
				: thread.history.quietScore(thread.conthist, ply, pos.threats(), moving, move);

			if (!RootNode && bestScore > -ScoreWin)
			{
				if (!noisy)
				{
					if (legalMoves >= g_lmpTable[improving][std::min(depth, 15)])
					{
						generator.skipQuiets();
						continue;
					}

					if (depth <= maxHistoryPruningDepth()
						&& history < historyPruningMargin() * depth + historyPruningOffset())
					{
						generator.skipQuiets();
						continue;
					}

					if (!inCheck
						&& depth <= maxFpDepth()
						&& std::abs(alpha) < 2000
						&& curr.staticEval + fpMargin() + depth * fpScale() <= alpha)
					{
						generator.skipQuiets();
						continue;
					}
				}

				const auto lmrDepth = std::max(depth - baseLmr, 0);

				const auto seeThreshold = noisy
					? seePruningThresholdNoisy() * depth
					: seePruningThresholdQuiet() * lmrDepth * lmrDepth;

				if (quietOrLosing && !see::see(pos, move, seeThreshold))
					continue;
			}

			if constexpr (PvNode)
				curr.pv.length = 0;

			const auto prevNodes = thread.search.nodes;

			++thread.search.nodes;
			++legalMoves;

			i32 extension{};

			if (!RootNode
				&& depth >= minSeDepth()
				&& move == ttEntry.move
				&& !curr.excluded
				&& ttEntry.depth >= depth - seTtDepthMargin()
				&& ttEntry.flag != TtFlag::UpperBound)
			{
				const auto sBeta = std::max(-ScoreInf + 1, ttEntry.score - depth * sBetaMargin() / 16);
				const auto sDepth = (depth - 1) / 2;

				curr.excluded = move;
				const auto score = search(thread, curr.pv, sDepth, ply, moveStackIdx + 1, sBeta - 1, sBeta, cutnode);
				curr.excluded = NullMove;

				if (score < sBeta)
				{
					if (!PvNode && curr.multiExtensions <= multiExtLimit() && score < sBeta - doubleExtMargin())
						extension = 2 + (!ttMoveNoisy && score < sBeta - tripleExtMargin());
					else extension = 1;
				}
				else if (sBeta >= beta)
					return sBeta;
				else if (ttEntry.score >= beta)
					extension = -1;
			}

			curr.multiExtensions += extension >= 2;

			m_ttable.prefetch(pos.roughKeyAfter(move));

			thread.setMove(ply, move);
			const auto guard = pos.applyMove(move, &thread.nnueState);

			Score score{};

			if (pos.isDrawn(true))
				score = drawScore(thread.search.nodes);
			else
			{
				const auto newDepth = depth + extension - 1;

				if (depth >= minLmrDepth()
					&& legalMoves >= lmrMinMoves()
					&& quietOrLosing)
				{
					auto r = baseLmr;

					r += !PvNode - ttpv;
					r -= history / lmrHistoryDivisor();
					r -= improving;
					r -= pos.isCheck();

					// can't use std::clamp because newDepth can be <0
					const auto reduced = std::min(std::max(newDepth - r, 1), newDepth);
					score = -search(thread, curr.pv, reduced, ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, true);

					if (score > alpha && reduced < newDepth)
					{
						score = -search(thread, curr.pv, newDepth, ply + 1,
							moveStackIdx + 1, -alpha - 1, -alpha, !cutnode);

						if (!noisy && (score <= alpha || score >= beta))
						{
							const auto bonus = score <= alpha ? -historyBonus(newDepth) : historyBonus(newDepth);
							thread.history.updateConthist(thread.conthist, ply, moving, move, bonus);
						}
					}
				}
				// if we're skipping LMR for some reason (first move in a non-PV
				// node, or the conditions above for LMR were not met) then do an
				// unreduced zero-window search to check if this move can raise alpha
				else if (!PvNode || legalMoves > 1)
					score = -search(thread, curr.pv, newDepth, ply + 1,
						moveStackIdx + 1, -alpha - 1, -alpha, !cutnode);

				// if we're in a PV node and
				//   - we're searching the first legal move, or
				//   - alpha was raised by a previous zero-window search,
				// then do a full-window search to get the true score of this node
				if (PvNode && (legalMoves == 1 || score > alpha))
					score = -search<true>(thread, curr.pv, newDepth,
						ply + 1, moveStackIdx + 1, -beta, -alpha, false);
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

				if constexpr (PvNode)
				{
					assert(curr.pv.length + 1 <= MaxDepth);
					pv.update(move, curr.pv);
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
				curr.killers.push(bestMove);

				thread.history.updateQuietScore(thread.conthist, ply, pos.threats(),
					pos.boards().pieceAt(bestMove.src()), bestMove, bonus);

				for (const auto prevQuiet : moveStack.failLowQuiets)
				{
					thread.history.updateQuietScore(thread.conthist, ply, pos.threats(),
						pos.boards().pieceAt(prevQuiet.src()), prevQuiet, penalty);
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

		if (!curr.excluded && !hasStopped())
			m_ttable.put(pos.key(), bestScore, curr.staticEval, bestMove, depth, ply, ttFlag, ttpv);

		return bestScore;
	}

	template <bool PvNode>
	auto Searcher::qsearch(ThreadData &thread, i32 ply, u32 moveStackIdx, Score alpha, Score beta) -> Score
	{
		assert(ply > 0 && ply <= MaxDepth);

		if (checkHardTimeout(thread.search, thread.isMainThread()))
			return 0;

		auto &pos = thread.pos;

		if (alpha < 0 && pos.hasCycle(ply))
		{
			alpha = drawScore(thread.search.nodes);
			if (alpha >= beta)
				return alpha;
		}

		if (PvNode && ply + 1 > thread.search.seldepth)
			thread.search.seldepth = ply + 1;

		if (ply >= MaxDepth)
			return pos.isCheck() ? 0 : eval::staticEval(pos, thread.nnueState, m_contempt);

		ProbedTTableEntry ttEntry{};
		m_ttable.probe(ttEntry, pos.key(), ply);

		if (!PvNode
			&& (ttEntry.flag == TtFlag::Exact
				|| ttEntry.flag == TtFlag::UpperBound && ttEntry.score <= alpha
				|| ttEntry.flag == TtFlag::LowerBound && ttEntry.score >= beta))
			return ttEntry.score;

		const bool ttpv = PvNode || ttEntry.wasPv;

		Score staticEval, eval;

		if (pos.isCheck())
		{
			staticEval = ScoreNone;
			eval = -ScoreMate + ply;
		}
		else
		{
			if (ttEntry.flag != TtFlag::None && ttEntry.staticEval != ScoreNone)
				staticEval = ttEntry.staticEval;
			else staticEval = eval::staticEval(pos, thread.nnueState, m_contempt);

			if (ttEntry.flag != TtFlag::None
				&& (ttEntry.flag == TtFlag::Exact
					|| ttEntry.flag == TtFlag::UpperBound && ttEntry.score < staticEval
					|| ttEntry.flag == TtFlag::LowerBound && ttEntry.score > staticEval))
				eval = ttEntry.score;
			else eval = staticEval;

			if (eval >= beta)
				return eval;

			if (eval > alpha)
				alpha = eval;
		}

		const auto futility = eval + qsearchFpMargin();

		auto bestMove = NullMove;
		auto bestScore = eval;

		auto ttFlag = TtFlag::UpperBound;

		auto generator = MoveGenerator::qsearch(pos, thread.moveStack[moveStackIdx].movegenData, ttEntry.move, thread.history);

		while (const auto move = generator.next())
		{
			if (!pos.isLegal(move))
				continue;

			if (!pos.isCheck()
				&& futility <= alpha
				&& !see::see(pos, move, 1))
			{
				if (bestScore < futility)
					bestScore = futility;
				continue;
			}

			if (!see::see(pos, move, qsearchSeeThreshold()))
				continue;

			++thread.search.nodes;

			m_ttable.prefetch(pos.roughKeyAfter(move));
			const auto guard = pos.applyMove(move, &thread.nnueState);

			const auto score = pos.isDrawn(false)
				? drawScore(thread.search.nodes)
				: -qsearch<PvNode>(thread, ply + 1, moveStackIdx + 1, -beta, -alpha);

			if (score > bestScore)
				bestScore = score;

			if (score > alpha)
			{
				alpha = score;
				bestMove = move;

				ttFlag = TtFlag::Exact;
			}

			if (score >= beta)
			{
				ttFlag = TtFlag::LowerBound;
				break;
			}
		}

		if (!hasStopped())
			m_ttable.put(pos.key(), bestScore, staticEval, bestMove, 0, ply, ttFlag, ttpv);

		return bestScore;
	}

	auto Searcher::report(const ThreadData &mainThread, const PvList &pv,
		i32 depth, f64 time, Score score, Score alpha, Score beta) -> void
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

		const bool upperbound = score <= alpha;
		const bool lowerbound = score >= beta;

		if (std::abs(score) <= 2) // draw score
			score = 0;

		score = std::clamp(score, alpha, beta);
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

		if (upperbound)
			std::cout << " upperbound";
		if (lowerbound)
			std::cout << " lowerbound";

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

	auto Searcher::finalReport(const ThreadData &mainThread,
		const PvList &pv, i32 depthCompleted, f64 time, Score score) -> void
	{
		report(mainThread, pv, depthCompleted, time, score);
		std::cout << "bestmove " << uci::moveToString(pv.moves[0]) << std::endl;
	}
}
