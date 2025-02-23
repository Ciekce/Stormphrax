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

	using util::Instant;

	namespace
	{
		constexpr f64 WidenReportDelay = 1.0;
		constexpr f64 CurrmoveReportDelay = 2.5;

		// [improving][clamped depth]
		constexpr auto LmpTable = []
		{
			util::MultiArray<i32, 2, 16> result{};

			for (i32 improving = 0; improving < 2; ++improving)
			{
				for (i32 depth = 0; depth < 16; ++depth)
				{
					result[improving][depth] = (3 + depth * depth) / (2 - improving);
				}
			}

			return result;
		}();

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

	Searcher::Searcher(usize ttSizeMib)
		: m_ttable{ttSizeMib},
		  m_startTime{Instant::now()}
	{
		auto &thread = m_threads.emplace_back();

		thread.id = 0;
		thread.thread = std::thread{[this, &thread]
		{
			run(thread);
		}};
	}

	auto Searcher::newGame() -> void
	{
		// Finalisation (init) clears the TT, so don't clear it twice
		if (!m_ttable.finalize())
			m_ttable.clear();

		for (auto &thread : m_threads)
		{
			thread.history.clear();
			thread.correctionHistory.clear();
		}
	}

	auto Searcher::ensureReady() -> void
	{
		m_ttable.finalize();
	}

	auto Searcher::startSearch(const Position &pos, std::span<const u64> keyHistory, Instant startTime,
		i32 maxDepth, std::span<Move> moves, std::unique_ptr<limit::ISearchLimiter> limiter, bool infinite) -> void
	{
		if (!m_limiter && !limiter)
		{
			std::cerr << "missing limiter" << std::endl;
			return;
		}

		const auto initStart = Instant::now();

		if (m_ttable.finalize())
		{
			const auto initTime = initStart.elapsed();

			std::cout
				<< "info string No ucinewgame or isready before go, lost "
				<< static_cast<u32>(initTime * 1000.0)
				<< " ms to TT initialization"
				<< std::endl;
		}

		m_resetBarrier.arriveAndWait();

		m_infinite = infinite;

		m_minRootScore = -ScoreInf;
		m_maxRootScore =  ScoreInf;

		RootStatus status;

		if (!moves.empty())
		{
			m_rootMoves.resize(moves.size());
			std::ranges::copy(moves, m_rootMoves.begin());

			status = RootStatus::Searchmoves;
		}
		else
		{
			status = initRootMoves(pos);

			if (status == RootStatus::NoLegalMoves)
			{
				std::cout << "info string no legal moves" << std::endl;
				return;
			}
		}

		assert(!m_rootMoves.empty());

		if (limiter)
			m_limiter = std::move(limiter);

		const auto contempt = g_opts.contempt;

		m_contempt[static_cast<i32>(pos.  toMove())] =  contempt;
		m_contempt[static_cast<i32>(pos.opponent())] = -contempt;

		const auto keyHistorySize = util::pad<usize{256}>(keyHistory.size());

		for (auto &thread : m_threads)
		{
			thread.maxDepth = maxDepth;
			thread.search = SearchData{};
			thread.rootPos = pos;

			thread.keyHistory.clear();
			thread.keyHistory.reserve(keyHistorySize);

			std::ranges::copy(keyHistory, std::back_inserter(thread.keyHistory));

			thread.nnueState.reset(thread.rootPos.bbs(), thread.rootPos.kings());
		}

		if (status == RootStatus::Tablebase)
			m_threads[0].search.tbhits = 1;

		m_startTime = startTime;

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
		if (initRootMoves(thread.rootPos) == RootStatus::NoLegalMoves)
			return {-ScoreMate, -ScoreMate};

		m_infinite = false;

		m_stop.store(false, std::memory_order::seq_cst);

		const auto score = searchRoot(thread, false);

		m_ttable.age();

		const auto whitePovScore = thread.rootPos.toMove() == Color::Black ? -score : score;
		return {whitePovScore, wdl::normalizeScore(whitePovScore, thread.rootPos.classicalMaterial())};
	}

	auto Searcher::runBench(BenchData &data, const Position &pos, i32 depth) -> void
	{
		m_limiter = std::make_unique<limit::InfiniteLimiter>();
		m_infinite = false;

		m_contempt = {};

		// this struct is a small boulder the size of a large boulder
		// and overflows the stack if not on the heap
		auto thread = std::make_unique<ThreadData>();

		thread->rootPos = pos;
		thread->maxDepth = depth;

		thread->nnueState.reset(thread->rootPos.bbs(), thread->rootPos.kings());

		if (initRootMoves(thread->rootPos) == RootStatus::NoLegalMoves)
			return;

		m_stop.store(false, std::memory_order::seq_cst);

		const auto start = Instant::now();

		searchRoot(*thread, false);

		m_ttable.age();

		data.search = thread->search;
		data.time = start.elapsed();
	}

	auto Searcher::setThreads(u32 threadCount) -> void
	{
		if (threadCount == m_threads.size())
			return;

		stopThreads();

		m_quit.store(false, std::memory_order::seq_cst);

		m_threads.clear();
		m_threads.shrink_to_fit();
		m_threads.reserve(threadCount);

		m_resetBarrier.reset(threadCount + 1);
		m_idleBarrier.reset(threadCount + 1);

		m_searchEndBarrier.reset(threadCount);

		for (u32 threadId = 0; threadId < threadCount; ++threadId)
		{
			auto &thread = m_threads.emplace_back();

			thread.id = threadId;
			thread.thread = std::thread{[this, &thread]
			{
				run(thread);
			}};
		}
	}

	auto Searcher::initRootMoves(const Position &pos) -> RootStatus
	{
		m_rootMoves.clear();

		if (g_opts.syzygyEnabled
			&& pos.bbs().occupancy().popcount()
			<= std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST)))
		{
			bool success = true;

			switch (tb::probeRoot(m_rootMoves, pos))
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
					success = false;
					break;
			}

			if (success)
				return RootStatus::Tablebase;
		}

		if (m_rootMoves.empty())
			generateLegal(m_rootMoves, pos);

		return m_rootMoves.empty() ? RootStatus::NoLegalMoves : RootStatus::Generated;
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
		assert(!m_rootMoves.empty());

		auto &searchData = thread.search;

		const bool mainThread = actualSearch && thread.isMainThread();

		thread.rootPv.moves[0] = NullMove;
		thread.rootPv.length = 0;

		auto score = -ScoreInf;
		PvList pv{};

		searchData.nodes = 0;
		thread.stack[0].killers.clear();

		i32 depthCompleted{};

		for (i32 depth = 1;; ++depth)
		{
			searchData.rootDepth = depth;
			searchData.seldepth = 0;

			// count the root node
			searchData.incNodes();

			auto delta = initialAspWindow();

			auto alpha = -ScoreInf;
			auto beta = ScoreInf;

			if (depth >= 3)
			{
				alpha = std::max(score - delta, -ScoreInf);
				beta  = std::min(score + delta,  ScoreInf);
			}

			Score newScore{};

			i32 aspReduction = 0;

			while (!hasStopped())
			{
				const auto aspDepth = std::max(depth - aspReduction, 1); // paranoia
				newScore = search<true, true>(thread, thread.rootPos,
					thread.rootPv, aspDepth, 0, 0, alpha, beta, false);

				if ((newScore > alpha && newScore < beta) || hasStopped())
					break;

				if (mainThread)
				{
					const auto time = elapsed();
					if (time >= WidenReportDelay)
						report(thread, thread.rootPv, depth, time, newScore, alpha, beta);
				}

				if (newScore <= alpha)
				{
					aspReduction = 0;

					beta = (alpha + beta) / 2;
					alpha = std::max(newScore - delta, -ScoreInf);
				}
				else
				{
					aspReduction = std::min(aspReduction + 1, 3);
					beta = std::min(newScore + delta, ScoreInf);
				}

				delta += delta * aspWideningFactor() / 16;
			}

			assert(thread.rootPv.length > 0);

			if (hasStopped())
				break;

			depthCompleted = depth;

			score = newScore;
			pv = thread.rootPv;

			if (depth >= thread.maxDepth)
			{
				if (mainThread && m_infinite)
					report(thread, pv, searchData.rootDepth, elapsed(), score);
				break;
			}

			if (mainThread)
			{
				m_limiter->update(thread.search, score, pv.moves[0], thread.search.loadNodes());

				if (checkSoftTimeout(thread.search, true))
					break;

				report(thread, pv, searchData.rootDepth, elapsed(), score);
			}
			else if (checkSoftTimeout(thread.search, thread.isMainThread()))
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
			auto time = elapsed();

			if (m_infinite)
			{
				// don't print bestmove until stopped when go infinite'ing
				// this makes handling reports a bit messy, unfortunately
				while (!hasStopped())
				{
					std::this_thread::yield();
				}
			}

			const std::unique_lock lock{m_searchMutex};

			m_stop.store(true, std::memory_order::seq_cst);
			waitForThreads();

			if (!m_infinite)
				time = elapsed();

			finalReport(thread, pv, depthCompleted, time, score);

			m_ttable.age();

			m_searching.store(false, std::memory_order::relaxed);
		}
		else waitForThreads();

		return score;
	}

	template <bool PvNode, bool RootNode>
	auto Searcher::search(ThreadData &thread, const Position &pos, PvList &pv,
		i32 depth, i32 ply, u32 moveStackIdx, Score alpha, Score beta, bool cutnode) -> Score
	{
		assert(ply >= 0 && ply <= MaxDepth);
		assert(RootNode || ply > 0);
		assert(PvNode || alpha + 1 == beta);

		if (ply > 0 && checkHardTimeout(thread.search, thread.isMainThread()))
			return 0;

		const auto &boards = pos.boards();
		const auto &bbs = pos.bbs();

		if constexpr (!RootNode)
		{
			alpha = std::max(alpha, -ScoreMate + ply);
			beta  = std::min( beta,  ScoreMate - ply - 1);

			if (alpha >= beta)
				return alpha;

			if (alpha < 0 && pos.hasCycle(ply, thread.keyHistory))
			{
				alpha = drawScore(thread.search.loadNodes());
				if (alpha >= beta)
					return alpha;
			}
		}

		if (depth <= 0)
			return qsearch<PvNode>(thread, pos, ply, moveStackIdx, alpha, beta);

		thread.search.updateSeldepth(ply + 1);

		const bool inCheck = pos.isCheck();

		if (ply >= MaxDepth)
			return inCheck ? 0 : eval::adjustedStaticEval(pos, thread.contMoves, ply, thread.nnueState, &thread.correctionHistory, m_contempt);

		const auto us = pos.toMove();
		const auto them = oppColor(us);

		assert(!PvNode || !cutnode);

		const auto *parent = RootNode ? nullptr : &thread.stack[ply - 1];
		auto &curr = thread.stack[ply];

		assert(!RootNode || curr.excluded == NullMove);

		auto &moveStack = thread.moveStack[moveStackIdx];

		ProbedTTableEntry ttEntry{};
		bool ttHit = false;

		if (!curr.excluded)
		{
			ttHit = m_ttable.probe(ttEntry, pos.key(), ply);

			if (!PvNode
				&& ttEntry.depth >= depth
				&& (ttEntry.score <= alpha || cutnode))
			{
				if (ttEntry.flag == TtFlag::Exact
					|| ttEntry.flag == TtFlag::UpperBound && ttEntry.score <= alpha
					|| ttEntry.flag == TtFlag::LowerBound && ttEntry.score >= beta)
				{
					if (ttEntry.score >= beta
						&& ttEntry.move
						&& !pos.isNoisy(ttEntry.move)
						&& pos.isPseudolegal(ttEntry.move))
					{
						const auto bonus = historyBonus(depth);
						thread.history.updateQuietScore(thread.conthist, ply,
							pos.threats(), boards.pieceAt(ttEntry.move.src()), ttEntry.move, bonus);
					}

					return ttEntry.score;
				}
				else if (depth <= 6)
					++depth;
			}
		}

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
				thread.search.incTbHits();

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
					score = drawScore(thread.search.loadNodes());
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
					else // lower bound (win)
					{
						if (score > alpha)
							alpha = score;
						syzygyMin = score;
					}
				}
			}
		}

		if (depth >= 3
			&& !curr.excluded
			&& (PvNode || cutnode)
			&& (!ttEntry.move || ttEntry.depth + 3 < depth))
			--depth;

		Score rawStaticEval{};
		std::optional<Score> complexity{};

		if (!curr.excluded)
		{
			if (inCheck)
				rawStaticEval = ScoreNone;
			else if (ttHit && ttEntry.staticEval != ScoreNone)
				rawStaticEval = ttEntry.staticEval;
			else rawStaticEval = eval::staticEval(pos, thread.nnueState, m_contempt);

			if (!ttHit)
				m_ttable.put(pos.key(), ScoreNone, rawStaticEval, NullMove, 0, 0, TtFlag::None, ttpv);

			if (inCheck)
				curr.staticEval = ScoreNone;
			else
			{
				Score corrDelta{};
				curr.staticEval = eval::adjustEval(pos, thread.contMoves,
					ply, &thread.correctionHistory, rawStaticEval, &corrDelta);
				complexity = corrDelta;
			}
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
			if (depth <= 6
				&& curr.staticEval - rfpMargin() * std::max(depth - improving, 0) >= beta)
				return (curr.staticEval + beta) / 2;

			if (depth <= 4
				&& std::abs(alpha) < 2000
				&& curr.staticEval + razoringMargin() * depth <= alpha)
			{
				const auto score = qsearch(thread, pos, ply, moveStackIdx, alpha, alpha + 1);
				if (score <= alpha)
					return score;
			}

			if (depth >= 4
				&& ply >= thread.minNmpPly
				&& curr.staticEval >= beta
				&& !parent->move.isNull()
				&& !(ttEntry.flag == TtFlag::UpperBound && ttEntry.score < beta)
				&& !bbs.nonPk(us).empty())
			{
				m_ttable.prefetch(pos.key() ^ keys::color());

				const auto R = 4
					+ depth / 5
					+ std::min((curr.staticEval - beta) / nmpEvalReductionScale(), 2)
					+ improving;

				const auto score = [&]
				{
					const auto [newPos, guard] = thread.applyNullmove(pos, ply);
					return -search(thread, newPos, curr.pv, depth - R,
						ply + 1, moveStackIdx, -beta, -beta + 1, !cutnode);
				}();

				if (score >= beta)
				{
					if (depth <= 14 || thread.minNmpPly > 0)
						return score > ScoreWin ? beta : score;

					thread.minNmpPly = ply + (depth - R) * 3 / 4;

					const auto verifScore = search(thread, pos, curr.pv,
						depth - R, ply, moveStackIdx + 1, beta - 1, beta, true);

					thread.minNmpPly = 0;

					if (verifScore >= beta)
						return verifScore;
				}
			}

			const auto probcutBeta = beta + probcutMargin();
			const auto probcutDepth = std::max(depth - 3, 1);

			if (!ttpv
				&& depth >= 7
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

					thread.search.incNodes();

					m_ttable.prefetch(pos.roughKeyAfter(move));

					const auto [newPos, guard] = thread.applyMove(pos, ply, move);

					auto score = -qsearch(thread, newPos, ply + 1, moveStackIdx + 1, -probcutBeta, -probcutBeta + 1);

					if (score >= probcutBeta)
						score = -search(thread, newPos, curr.pv, probcutDepth - 1,
							ply + 1, moveStackIdx + 1, -probcutBeta, -probcutBeta + 1, !cutnode);

					if (hasStopped())
						return 0;

					if (score >= probcutBeta)
					{
						m_ttable.put(keyBefore, score, curr.staticEval,
							move, probcutDepth, ply, TtFlag::LowerBound, false);
						return score;
					}
				}
			}
		}

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
				if (!isLegalRootMove(move))
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
				? thread.history.noisyScore(move, captured, pos.threats())
				: thread.history.quietScore(thread.conthist, ply, pos.threats(), moving, move);

			if ((!RootNode || thread.search.rootDepth == 1)
				&& bestScore > -ScoreWin && (!PvNode || !thread.datagen))
			{
				const auto lmrDepth = std::max(depth - baseLmr / 128, 0);

				if (!noisy)
				{
					if (legalMoves >= LmpTable[improving][std::min(depth, 15)])
					{
						generator.skipQuiets();
						continue;
					}

					if (lmrDepth <= 5
						&& history < quietHistPruningMargin() * depth + quietHistPruningOffset())
					{
						generator.skipQuiets();
						continue;
					}

					if (!inCheck
						&& lmrDepth <= 8
						&& std::abs(alpha) < 2000
						&& curr.staticEval + fpMargin() + depth * fpScale() <= alpha)
					{
						generator.skipQuiets();
						continue;
					}
				}
				else if (depth <= 4
					&& history < noisyHistPruningMargin() * depth * depth + noisyHistPruningOffset())
					continue;

				const auto seeThreshold = noisy
					? seePruningThresholdNoisy() * depth
					: seePruningThresholdQuiet() * lmrDepth * lmrDepth;

				if (quietOrLosing && !see::see(pos, move, seeThreshold))
					continue;
			}

			if constexpr (PvNode)
				curr.pv.length = 0;

			const auto prevNodes = thread.search.loadNodes();

			thread.search.incNodes();
			++legalMoves;

			if (RootNode
				&& g_opts.showCurrMove
				&& elapsed() > CurrmoveReportDelay)
				std::cout << "info depth " << depth
					<< " currmove " << uci::moveToString(move)
					<< " currmovenumber " << legalMoves << std::endl;

			i32 extension{};

			if (!RootNode
				&& depth >= 8
				&& ply < thread.search.rootDepth * 2
				&& move == ttEntry.move
				&& !curr.excluded
				&& ttEntry.depth >= depth - 5
				&& ttEntry.flag != TtFlag::UpperBound)
			{
				const auto sBeta = std::max(-ScoreInf + 1, ttEntry.score - depth * sBetaMargin() / 16);
				const auto sDepth = (depth - 1) / 2;

				curr.excluded = move;

				const auto score = search(thread, pos, curr.pv,
					sDepth, ply, moveStackIdx + 1, sBeta - 1, sBeta, cutnode);

				curr.excluded = NullMove;

				if (score < sBeta)
				{
					if (!PvNode && score < sBeta - doubleExtMargin())
						extension = 2 + (!ttMoveNoisy && score < sBeta - tripleExtMargin());
					else extension = 1;
				}
				else if (sBeta >= beta)
					return sBeta;
				else if (cutnode)
					extension = -2;
				else if (ttEntry.score >= beta)
					extension = -1;
			}

			cutnode |= extension < 0;

			m_ttable.prefetch(pos.roughKeyAfter(move));

			const auto [newPos, guard] = thread.applyMove(pos, ply, move);

			const bool givesCheck = newPos.isCheck();

			Score score{};

			if (newPos.isDrawn(true, thread.keyHistory))
				score = drawScore(thread.search.loadNodes());
			else
			{
				auto newDepth = depth + extension - 1;

				if (depth >= 2
					&& legalMoves >= 2 + RootNode
					&& quietOrLosing)
				{
					auto r = baseLmr;

					r += !PvNode * lmrNonPvReductionScale();
					r -= ttpv * lmrTtpvReductionScale();
					r -= history * 128 / lmrHistoryDivisor();
					r -= improving * lmrImprovingReductionScale();
					r -= givesCheck * lmrCheckReductionScale();
					r += cutnode * lmrCutnodeReductionScale();

					if (complexity)
					{
						const bool highComplexity = *complexity > lmrHighComplexityThreshold();
						r -= lmrHighComplexityReductionScale() * highComplexity;
					}

					r /= 128;

					// can't use std::clamp because newDepth can be <0
					const auto reduced = std::min(std::max(newDepth - r, 1), newDepth + (PvNode || cutnode));
					score = -search(thread, newPos, curr.pv, reduced,
						ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, true);

					if (score > alpha && reduced < newDepth)
					{
						const bool doDeeperSearch = score > bestScore + lmrDeeperBase() + lmrDeeperScale() * newDepth;
						const bool doShallowerSearch = score < bestScore + newDepth;

						newDepth += doDeeperSearch - doShallowerSearch;

						score = -search(thread, newPos, curr.pv, newDepth,
							ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, !cutnode);

						if (!noisy && (score <= alpha || score >= beta))
						{
							const auto bonus = score <= alpha ? historyPenalty(newDepth) : historyBonus(newDepth);
							thread.history.updateConthist(thread.conthist, ply, moving, move, bonus);
						}
					}
				}
				// if we're skipping LMR for some reason (first move in a non-PV
				// node, or the conditions above for LMR were not met) then do an
				// unreduced zero-window search to check if this move can raise alpha
				else if (!PvNode || legalMoves > 1)
					score = -search(thread, newPos, curr.pv, newDepth,
						ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, !cutnode);

				// if we're in a PV node and
				//   - we're searching the first legal move, or
				//   - alpha was raised by a previous zero-window search,
				// then do a full-window search to get the true score of this node
				if (PvNode && (legalMoves == 1 || score > alpha))
					score = -search<true>(thread, newPos, curr.pv,
						newDepth, ply + 1, moveStackIdx + 1, -beta, -alpha, false);
			}

			if (hasStopped())
				return 0;

			if constexpr (RootNode)
			{
				if (thread.isMainThread())
					m_limiter->updateMoveNodes(move, thread.search.loadNodes() - prevNodes);
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
			const auto historyDepth = depth + (curr.staticEval <= alpha);

			const auto bonus = historyBonus(historyDepth);
			const auto penalty = historyPenalty(historyDepth);

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
				thread.history.updateNoisyScore(bestMove, captured, pos.threats(), bonus);
			}

			// unconditionally update capthist
			for (const auto prevNoisy : moveStack.failLowNoisies)
			{
				const auto captured = pos.captureTarget(prevNoisy);
				thread.history.updateNoisyScore(prevNoisy, captured, pos.threats(), penalty);
			}
		}

		bestScore = std::clamp(bestScore, syzygyMin, syzygyMax);

		if (!curr.excluded)
		{
			if (!inCheck
				&& (bestMove.isNull() || !pos.isNoisy(bestMove))
				&& (ttFlag == TtFlag::Exact
					|| ttFlag == TtFlag::UpperBound && bestScore < curr.staticEval
					|| ttFlag == TtFlag::LowerBound && bestScore > curr.staticEval))
				thread.correctionHistory.update(pos, thread.contMoves, ply, depth, bestScore, curr.staticEval);

			m_ttable.put(pos.key(), bestScore, rawStaticEval, bestMove, depth, ply, ttFlag, ttpv);
		}

		return bestScore;
	}

	template <bool PvNode>
	auto Searcher::qsearch(ThreadData &thread, const Position &pos,
		i32 ply, u32 moveStackIdx, Score alpha, Score beta) -> Score
	{
		assert(ply > 0 && ply <= MaxDepth);

		if (checkHardTimeout(thread.search, thread.isMainThread()))
			return 0;

		if (alpha < 0 && pos.hasCycle(ply, thread.keyHistory))
		{
			alpha = drawScore(thread.search.loadNodes());
			if (alpha >= beta)
				return alpha;
		}

		const bool inCheck = pos.isCheck();

		if constexpr (PvNode)
			thread.search.updateSeldepth(ply + 1);

		thread.clearContMove(ply);

		if (ply >= MaxDepth)
			return inCheck ? 0
				: eval::adjustedStaticEval(pos, thread.contMoves, ply,
					thread.nnueState, &thread.correctionHistory, m_contempt);

		ProbedTTableEntry ttEntry{};
		const bool ttHit = m_ttable.probe(ttEntry, pos.key(), ply);

		if (!PvNode
			&& (ttEntry.flag == TtFlag::Exact
				|| ttEntry.flag == TtFlag::UpperBound && ttEntry.score <= alpha
				|| ttEntry.flag == TtFlag::LowerBound && ttEntry.score >= beta))
			return ttEntry.score;

		const bool ttpv = PvNode || ttEntry.wasPv;

		Score rawStaticEval, eval;

		if (inCheck)
		{
			rawStaticEval = ScoreNone;
			eval = -ScoreMate + ply;
		}
		else
		{
			if (ttHit && ttEntry.staticEval != ScoreNone)
				rawStaticEval = ttEntry.staticEval;
			else rawStaticEval = eval::staticEval(pos, thread.nnueState, m_contempt);

			if (!ttHit)
				m_ttable.put(pos.key(), ScoreNone, rawStaticEval, NullMove, 0, 0, TtFlag::None, ttpv);

			const auto staticEval = eval::adjustEval(pos, thread.contMoves,
				ply, &thread.correctionHistory, rawStaticEval);

			if (ttEntry.flag == TtFlag::Exact
				|| ttEntry.flag == TtFlag::UpperBound && ttEntry.score < staticEval
				|| ttEntry.flag == TtFlag::LowerBound && ttEntry.score > staticEval)
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

		auto generator = MoveGenerator::qsearch(pos, thread.moveStack[moveStackIdx].movegenData,
			ttEntry.move, thread.history, thread.conthist, ply);

		u32 legalMoves = 0;

		while (const auto move = generator.next())
		{
			if (!pos.isLegal(move))
				continue;

			if (bestScore > -ScoreWin)
			{
				if (!inCheck
					&& futility <= alpha
					&& !see::see(pos, move, 1))
				{
					if (bestScore < futility)
						bestScore = futility;
					continue;
				}

				if (legalMoves >= 2)
					break;

				if (!see::see(pos, move, qsearchSeeThreshold()))
					continue;
			}

			++legalMoves;

			thread.search.incNodes();

			m_ttable.prefetch(pos.roughKeyAfter(move));

			const auto [newPos, guard] = thread.applyMove(pos, ply, move);

			const auto score = newPos.isDrawn(false, thread.keyHistory)
				? drawScore(thread.search.loadNodes())
				: -qsearch<PvNode>(thread, newPos, ply + 1, moveStackIdx + 1, -beta, -alpha);

			if (hasStopped())
				return 0;

			if (score > -ScoreWin)
				generator.skipQuiets();

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

		// stalemate not handled
		if (inCheck && legalMoves == 0)
			return -ScoreMate + ply;

		m_ttable.put(pos.key(), bestScore, rawStaticEval, bestMove, 0, ply, ttFlag, ttpv);

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
			nodes += thread.search.loadNodes();
			seldepth = std::max(seldepth, thread.search.loadSeldepth());
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

		const auto material = mainThread.rootPos.classicalMaterial();

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
			const auto normScore = wdl::normalizeScore(score, material);
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
				const auto [wdlWin, wdlLoss] = wdl::wdlModel(score, material);
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
				tbhits += thread.search.loadTbHits();
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
