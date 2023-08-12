/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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
#include <algorithm>
#include <cmath>
#include <cassert>

#include "uci.h"
#include "movegen.h"
#include "eval/eval.h"
#include "limit/trivial.h"
#include "opts.h"
#include "3rdparty/fathom/tbprobe.h"

namespace stormphrax::search
{
	using namespace stormphrax::tunable;

	namespace
	{
		constexpr f64 MinReportDelay = 1.0;

		// values from viridithas
		//TODO tune for stormphrax
		constexpr f64 LmrBase = 0.77;
		constexpr f64 LmrDivisor = 2.36;

		[[nodiscard]] auto generateLmrTable()
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

		inline auto drawScore(usize nodes)
		{
			return 2 - static_cast<Score>(nodes % 4);
		}
	}

	Searcher::Searcher(std::optional<usize> hashSize)
		: m_table{hashSize ? *hashSize : DefaultHashSize}
	{
		auto &threadData = m_threads.emplace_back();

		threadData.id = m_nextThreadId++;
		threadData.thread = std::thread{[this, &threadData]
		{
			run(threadData);
		}};
	}

	auto Searcher::newGame() -> void
	{
		m_table.clear();

		for (auto &thread : m_threads)
		{
			std::fill(thread.stack.begin(), thread.stack.end(), SearchStackEntry{});
			thread.history.clear();
		}
	}

	auto Searcher::startSearch(const Position &pos, i32 maxDepth) -> void
	{
		if (!m_limiter)
		{
			std::cerr << "missing limiter" << std::endl;
			return;
		}

		const auto &boards = pos.boards();

		// probe syzygy tb for a move
		if (g_opts.syzygyEnabled
			&& boards.occupancy().popcount() <= std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST)))
		{
			const auto epSq = pos.enPassant();
			const auto result = tb_probe_root(
				boards.whiteOccupancy(),
				boards.blackOccupancy(),
				boards.kings(),
				boards.queens(),
				boards.rooks(),
				boards.bishops(),
				boards.knights(),
				boards.pawns(),
				pos.halfmove(), 0,
				epSq == Square::None ? 0 : static_cast<i32>(epSq),
				pos.toMove() == Color::White,
				nullptr
			);

			if (result != TB_RESULT_FAILED)
			{
				static constexpr auto PromoPieces = std::array {
					BasePiece::None,
					BasePiece::Queen,
					BasePiece::Rook,
					BasePiece::Bishop,
					BasePiece::Knight
				};

				const auto wdl = TB_GET_WDL(result);

				const auto score = wdl == TB_WIN ? ScoreTbWin
					: wdl == TB_LOSS ? -ScoreTbWin
					: 0; // draw

				const auto src = static_cast<Square>(TB_GET_FROM(result));
				const auto dst = static_cast<Square>(TB_GET_TO  (result));

				const auto promo = PromoPieces[TB_GET_PROMOTES(result)];
				const bool ep = TB_GET_EP(result);

				const auto move = ep ? Move::enPassant(src, dst)
					: promo != BasePiece::None ? Move::promotion(src, dst, promo)
					: Move::standard(src, dst);

				// for pv printing
				m_threads[0].pos = pos;

				report(m_threads[0], 1, move, 0.0, score, -ScoreMax, ScoreMax, true);
				std::cout << "bestmove " << uci::moveToString(move) << std::endl;

				return;
			}
		}

		for (auto &thread : m_threads)
		{
			thread.maxDepth = maxDepth;
			thread.search = SearchData{};
			thread.pos = pos;

			thread.nnueState.reset(thread.pos.boards());
		}

		m_stop.store(false, std::memory_order::seq_cst);
		m_runningThreads.store(static_cast<i32>(m_threads.size()));

		m_flag.store(SearchFlag, std::memory_order::seq_cst);
		m_startSignal.notify_all();
	}

	auto Searcher::stop() -> void
	{
		m_stop.store(true, std::memory_order::relaxed);
		m_flag.store(IdleFlag, std::memory_order::seq_cst);

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

	auto Searcher::runDatagenSearch(ThreadData &thread) -> std::tuple<Move, Score, Score>
	{
		m_stop.store(false, std::memory_order::seq_cst);

		const auto result = searchRoot(thread, false);

		m_table.age();

		const auto whitePovScore = thread.pos.toMove() == Color::Black ? -result.second : result.second;
		return {result.first, whitePovScore, uci::normalizeScore(whitePovScore)};
	}

	auto Searcher::runBench(BenchData &data, const Position &pos, i32 depth) -> void
	{
		m_limiter = std::make_unique<limit::InfiniteLimiter>();

		// this struct is a small boulder the size of a large boulder
		// and overflows the stack if not on the heap
		auto threadData = std::make_unique<ThreadData>();

		threadData->pos = pos;
		threadData->maxDepth = depth;

		threadData->nnueState.reset(threadData->pos.boards());

		m_stop.store(false, std::memory_order::seq_cst);

		const auto start = util::g_timer.time();

		searchRoot(*threadData, false);

		const auto time = util::g_timer.time() - start;

		data.search = threadData->search;
		data.time = time;
	}

	auto Searcher::setThreads(u32 threads) -> void
	{
		if (threads != m_threads.size())
		{
			stopThreads();

			m_flag.store(IdleFlag, std::memory_order::seq_cst);

			m_threads.clear();
			m_threads.shrink_to_fit();
			m_threads.reserve(threads);

			m_nextThreadId = 0;

			for (i32 i = 0; i < threads; ++i)
			{
				auto &threadData = m_threads.emplace_back();

				threadData.id = m_nextThreadId++;
				threadData.thread = std::thread{[this, &threadData]
				{
					run(threadData);
				}};
			}
		}
	}

	auto Searcher::stopThreads() -> void
	{
		m_flag.store(QuitFlag, std::memory_order::seq_cst);
		m_startSignal.notify_all();

		for (auto &thread : m_threads)
		{
			thread.thread.join();
		}
	}

	auto Searcher::run(ThreadData &data) -> void
	{
		while (true)
		{
			i32 flag{};

			{
				std::unique_lock lock{m_startMutex};
				m_startSignal.wait(lock, [this, &flag]
				{
					flag = m_flag.load(std::memory_order::seq_cst);
					return flag != IdleFlag;
				});
			}

			if (flag == QuitFlag)
				return;

			searchRoot(data, true);
		}
	}

	auto Searcher::searchRoot(ThreadData &data, bool mainSearchThread) -> std::pair<Move, Score>
	{
		auto &searchData = data.search;

		const bool reportAndUpdate = mainSearchThread && data.id == 0;

		auto score = -ScoreMax;
		Move best{};

		const auto startTime = reportAndUpdate ? util::g_timer.time() : 0.0;
		const auto startDepth = 1 + static_cast<i32>(data.id) % 16;

		i32 depthCompleted{};

		bool hitSoftTimeout = false;

		for (i32 depth = startDepth;
			depth <= data.maxDepth
				&& !(hitSoftTimeout = shouldStop(searchData, true));
			++depth)
		{
			searchData.depth = depth;
			searchData.seldepth = 0;

			const auto prevBest = best;

			bool reportThisIter = reportAndUpdate;

			if (depth < minAspDepth())
			{
				const auto newScore = search(data, depth, 0, 0, -ScoreMax, ScoreMax, false);

				depthCompleted = depth;

				if (depth > 1 && m_stop.load(std::memory_order::relaxed) || !data.search.move)
					break;

				score = newScore;
				best = data.search.move;
			}
			else
			{
				auto aspDepth = depth;

				auto delta = initialAspWindow();

				auto alpha = std::max(score - delta, -ScoreMax);
				auto beta  = std::min(score + delta,  ScoreMax);

				while (!shouldStop(searchData, false))
				{
					aspDepth = std::max(aspDepth, depth - maxAspReduction());

					const auto newScore = search(data, aspDepth, 0, 0, alpha, beta, false);

					const bool stop = m_stop.load(std::memory_order::relaxed);
					if (stop || !searchData.move)
					{
						reportThisIter &= !stop;
						break;
					}

					score = newScore;

					if (reportAndUpdate && (score <= alpha || score >= beta))
					{
						const auto time = util::g_timer.time() - startTime;
						if (time > MinReportDelay)
							report(data, data.search.depth, best ?: searchData.move, time, score, alpha, beta);
					}

					delta += delta / 2;

					if (delta > maxAspWindow())
						delta = ScoreMax;

					if (score >= beta)
					{
						beta = std::min(beta + delta, ScoreMax);
						--aspDepth;
					}
					else if (score <= alpha)
					{
						beta = (alpha + beta) / 2;
						alpha = std::max(alpha - delta, -ScoreMax);
						aspDepth = depth;
					}
					else
					{
						best = searchData.move;
						depthCompleted = depth;
						break;
					}
				}
			}

			if (reportAndUpdate)
				m_limiter->update(data.search, best, data.search.nodes);

			if (reportThisIter && depth < data.maxDepth)
			{
				if (!best)
					best = searchData.move;

				if (best)
					report(data, searchData.depth, best,
						util::g_timer.time() - startTime, score, -ScoreMax, ScoreMax);
				else
				{
					std::cout << "info string no legal moves" << std::endl;
					break;
				}
			}
		}

		if (reportAndUpdate)
		{
			if (mainSearchThread)
				m_searchMutex.lock();

			if (best)
			{
				if (!hitSoftTimeout || !m_limiter->stopped())
					report(data, depthCompleted, best, util::g_timer.time() - startTime, score, -ScoreMax, ScoreMax);
				std::cout << "bestmove " << uci::moveToString(best) << std::endl;
			}
			else std::cout << "info string no legal moves" << std::endl;
		}

		if (mainSearchThread)
		{
			--m_runningThreads;
			m_stopSignal.notify_all();

			if (reportAndUpdate)
			{
				m_table.age();

				m_flag.store(IdleFlag, std::memory_order::relaxed);
				m_searchMutex.unlock();
			}
		}

		return {best, score};
	}

	auto Searcher::search(ThreadData &data, i32 depth,
		i32 ply, u32 moveStackIdx, Score alpha, Score beta, bool cutnode) -> Score
	{
		assert(alpha >= -ScoreMax);
		assert(beta  <=  ScoreMax);
		assert(depth >= 0 && depth <= MaxDepth);
		assert(ply   >= 0 && ply   <= MaxDepth);

		if (depth > 1 && shouldStop(data.search, false))
			return beta;

		auto &pos = data.pos;
		const auto &boards = pos.boards();

		if (ply >= MaxDepth)
			return data.nnueState.evaluate(pos.toMove());

		const bool inCheck = pos.isCheck();

		// check extension
		if (inCheck)
			++depth;

		if (depth <= 0)
			return qsearch(data, ply, moveStackIdx, alpha, beta);

		const auto us = pos.toMove();
		const auto them = oppColor(us);

		const bool root = ply == 0;
		const bool pv = root || beta - alpha > 1;

		auto &stack = data.stack[ply];
		auto &moveStack = data.moveStack[moveStackIdx];

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

		ProbedTTableEntry entry{};
		auto ttMove = NullMove;

		if (!stack.excluded)
		{
			if (m_table.probe(entry, pos.key(), depth, ply, alpha, beta) && !pv)
				return entry.score;
			else if (entry.move && pos.isPseudolegal(entry.move))
				ttMove = entry.move;

			// internal iterative reduction
			if (!inCheck
				&& depth >= minIirDepth()
				&& !stack.excluded
				&& !ttMove
				&& (pv || cutnode))
				--depth;
		}

		const bool ttHit = entry.type != EntryType::None;

		const auto pieceCount = boards.occupancy().popcount();

		auto syzygyMin = -ScoreMate;
		auto syzygyMax =  ScoreMate;

		const auto syzygyPieceLimit = std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST));

		// probe syzygy tb
		if (!root
			&& !stack.excluded
			&& g_opts.syzygyEnabled
			&& pieceCount <= syzygyPieceLimit
			&& (pieceCount < syzygyPieceLimit || depth >= g_opts.syzygyProbeDepth)
			&& pos.halfmove() == 0
			&& pos.castlingRooks() == CastlingRooks{})
		{
			const auto epSq = pos.enPassant();
			const auto wdl = tb_probe_wdl(
				boards.whiteOccupancy(),
				boards.blackOccupancy(),
				boards.kings(),
				boards.queens(),
				boards.rooks(),
				boards.bishops(),
				boards.knights(),
				boards.pawns(),
				0, 0,
				epSq == Square::None ? 0 : static_cast<i32>(epSq),
				us == Color::White
			);

			if (wdl != TB_RESULT_FAILED)
			{
				++data.search.tbhits;

				Score tbScore{};
				EntryType tbEntryType{};

				if (wdl == TB_WIN)
				{
					tbScore = ScoreTbWin - ply;
					tbEntryType = EntryType::Beta;
				}
				else if (wdl == TB_LOSS)
				{
					tbScore = -ScoreTbWin + ply;
					tbEntryType = EntryType::Alpha;
				}
				else // draw
				{
					tbScore = drawScore(data.search.nodes);
					tbEntryType = EntryType::Exact;
				}

				if (tbEntryType == EntryType::Exact
					|| tbEntryType == EntryType::Alpha && tbScore <= alpha
					|| tbEntryType == EntryType::Beta && tbScore >= beta)
				{
					m_table.put(pos.key(), tbScore, NullMove, depth, ply, tbEntryType);
					return tbScore;
				}

				if (pv)
				{
					if (tbEntryType == EntryType::Alpha)
						syzygyMax = tbScore;
					else if (tbEntryType == EntryType::Beta)
					{
						if (tbScore > alpha)
							alpha = tbScore;
						syzygyMin = tbScore;
					}
				}
			}
		}

		// we already have the static eval in a singularity search
		if (!stack.excluded)
		{
			if (!root && !pos.lastMove())
				stack.eval = -data.stack[ply - 1].eval;
			else stack.eval = inCheck ? 0 : data.nnueState.evaluate(pos.toMove());
		}

		stack.currMove = {};

		const bool improving = !inCheck && ply > 1 && stack.eval > data.stack[ply - 2].eval;

		if (!pv && !inCheck && !stack.excluded)
		{
			// reverse futility pruning
			if (depth <= maxRfpDepth()
				&& stack.eval >= beta + rfpMargin() * depth / (improving ? 2 : 1))
				return stack.eval;

			// nullmove pruning
			if (depth >= minNmpDepth()
				&& stack.eval >= beta
				&& !(ttHit && entry.type == EntryType::Alpha && entry.score < beta)
				&& pos.lastMove()
				&& !boards.nonPk(us).empty())
			{
				const auto R = std::min(depth,
					nmpReductionBase()
						+ depth / nmpReductionDepthScale()
						+ std::min((stack.eval - beta) / nmpReductionEvalScale(), maxNmpEvalReduction()));

				const auto guard = pos.applyMove<false>(NullMove, nullptr, &m_table);
				const auto score = -search(data, depth - R, ply + 1, moveStackIdx + 1, -beta, -beta + 1, !cutnode);

				if (score >= beta)
					return score > ScoreWin ? beta : score;
			}
		}

		moveStack.quietsTried.clear();
		moveStack.noisiesTried.clear();

		if (ply > 0)
			stack.doubleExtensions = data.stack[ply - 1].doubleExtensions;

		const i32 minLmrMoves = pv ? 3 : 2;

		const auto prevMove = ply > 0 ? data.stack[ply - 1].currMove : HistoryMove{};
		const auto prevPrevMove = ply > 1 ? data.stack[ply - 2].currMove : HistoryMove{};

		auto best = NullMove;
		auto bestScore = -ScoreMax;

		auto entryType = EntryType::Alpha;

		MoveGenerator generator{pos, stack.killer, moveStack.moves,
			ttMove, prevMove, prevPrevMove, &data.history};

		u32 legalMoves = 0;

		while (const auto move = generator.next())
		{
			if (move == stack.excluded)
				continue;

			const auto prevNodes = data.search.nodes;

			const bool quietOrLosing = generator.stage() >= MovegenStage::Quiet;
			const auto [noisy, captured] = pos.noisyCapturedPiece(move);

			const auto baseLmr = LmrTable[depth][legalMoves + 1];

			if (!root && quietOrLosing && bestScore > -ScoreWin)
			{
				if (!inCheck)
				{
					const auto lmrDepth = std::max(0, depth - baseLmr);

					// late move pruning
					if (!pv
						&& depth <= maxLmpDepth()
						&& legalMoves >= lmpMinMovesBase() + lmrDepth * lmrDepth)
						break;

					// futility pruning
					if (depth <= maxFpDepth()
						&& alpha < ScoreWin
						&& stack.eval + fpMargin() + lmrDepth * fpScale() <= alpha)
						break;
				}

				// see pruning
				if (depth <= maxSeePruningDepth()
					&& !see::see(pos, move, depth * (noisy ? noisySeeThreshold() : quietSeeThreshold())))
					continue;
			}

			const auto movingPiece = boards.pieceAt(move.src());

			const auto guard = pos.applyMove(move, &data.nnueState, &m_table);

			if (!guard)
				continue;

			++data.search.nodes;
			++legalMoves;

			i32 extension{};

			// singular extension
			if (!root
				&& depth >= minSingularityDepth()
				&& ply < data.maxDepth * 2
				&& move == ttMove
				&& !stack.excluded
				&& entry.depth >= depth - singularityDepthMargin()
				&& entry.type != EntryType::Alpha)
			{
				const auto sBeta = std::max(-ScoreMate, entry.score - singularityDepthScale() * depth);
				const auto sDepth = (depth - 1) / 2;

				stack.excluded = move;
				pos.popMove(&data.nnueState);

				const auto score = search(data, sDepth, ply, moveStackIdx + 1, sBeta - 1, sBeta, cutnode);

				stack.excluded = NullMove;
				pos.applyMoveUnchecked(move, &data.nnueState, &m_table);

				if (score < sBeta)
				{
					if (!pv
						&& score < sBeta - doubleExtensionMargin()
						&& stack.doubleExtensions <= doubleExtensionLimit())
					{
						extension = 2;
						++stack.doubleExtensions;
					}
					else extension = 1;
				}
				else if (sBeta >= beta) // multicut
					return sBeta;
				else if (entry.score >= beta)
					extension = -1;
			}

			stack.currMove = {movingPiece, moveActualDst(move)};

			Score score{};

			if (pos.isDrawn(false))
				score = drawScore(data.search.nodes);
			else
			{
				const auto newDepth = depth - 1 + extension;

				if (pv && legalMoves == 1)
					score = -search(data, newDepth, ply + 1, moveStackIdx + 1, -beta, -alpha, false);
				else
				{
					i32 reduction{};

					// lmr
					if (depth >= minLmrDepth()
						&& legalMoves >= minLmrMoves
						&& generator.stage() >= MovegenStage::Quiet)
					{
						auto lmr = baseLmr;

						if (!pv)
							++lmr;

						if (pos.isCheck()) // this move gives check
							--lmr;

						reduction = std::clamp(lmr, 0, depth - 2);
					}

					score = -search(data, newDepth - reduction, ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, true);

					if (score > alpha && reduction > 0)
						score = -search(data, newDepth, ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, !cutnode);

					if (score > alpha && score < beta)
						score = -search(data, newDepth, ply + 1, moveStackIdx + 1, -beta, -alpha, false);
				}
			}

			if (root && data.id == 0)
				m_limiter->updateMoveNodes(move, data.search.nodes - prevNodes);

			if (score > bestScore)
			{
				best = move;
				bestScore = score;

				if (score > alpha)
				{
					if (score >= beta)
					{
						const auto adjustment = depth * depth + depth - 1;

						auto *prevContEntry = (quietOrLosing && prevMove)
							? &data.history.contEntry(prevMove) : nullptr;
						auto *prevPrevContEntry = (!noisy && prevPrevMove)
							? &data.history.contEntry(prevPrevMove) : nullptr;

						if (quietOrLosing)
						{
							stack.killer = move;
							if (prevMove)
								data.history.entry(prevMove).countermove = move;
						}

						if (noisy)
							updateHistoryScore(
								data.history.captureScore(stack.currMove, captured),
								adjustment
							);
						else
						{
							updateHistoryScore(data.history.entry(stack.currMove).score, adjustment);

							if (prevContEntry)
								updateHistoryScore(prevContEntry->score(stack.currMove), adjustment);
							if (prevPrevContEntry)
								updateHistoryScore(prevPrevContEntry->score(stack.currMove), adjustment);

							for (const auto prevQuiet : moveStack.quietsTried)
							{
								updateHistoryScore(data.history.entry(prevQuiet).score, -adjustment);

								if (prevContEntry)
									updateHistoryScore(prevContEntry->score(prevQuiet), -adjustment);
								if (prevPrevContEntry)
									updateHistoryScore(prevPrevContEntry->score(prevQuiet), -adjustment);
							}
						}

						for (const auto [prevNoisy, prevCaptured] : moveStack.noisiesTried)
						{
							updateHistoryScore(
								data.history.captureScore(prevNoisy, prevCaptured),
								-adjustment
							);
						}

						entryType = EntryType::Beta;
						break;
					}

					alpha = score;
					entryType = EntryType::Exact;
				}
			}

			if (noisy)
				moveStack.noisiesTried.push({stack.currMove, captured});
			else moveStack.quietsTried.push(stack.currMove);
		}

		if (legalMoves == 0)
		{
			if (stack.excluded)
				return alpha;
			return inCheck ? (-ScoreMate + ply) : 0;
		}

		bestScore = std::clamp(bestScore, syzygyMin, syzygyMax);

		// increase depth for tt if in check
		// https://chess.swehosting.se/test/1456/
		if (!stack.excluded)
			m_table.put(pos.key(), bestScore, best, inCheck ? depth + 1 : depth, ply, entryType);

		if (root && (!m_stop || !data.search.move))
			data.search.move = best;

		return bestScore;
	}

	auto Searcher::qsearch(ThreadData &data, i32 ply, u32 moveStackIdx, Score alpha, Score beta) -> Score
	{
		if (shouldStop(data.search, false))
			return beta;

		auto &pos = data.pos;

		const auto staticEval = pos.isCheck()
			? -ScoreMate
			: data.nnueState.evaluate(pos.toMove());

		if (staticEval > alpha)
		{
			if (staticEval >= beta)
				return staticEval;

			alpha = staticEval;
		}

		if (ply >= MaxDepth)
			return staticEval;

		const auto us = pos.toMove();

		if (ply > data.search.seldepth)
			data.search.seldepth = ply;

		ProbedTTableEntry entry{};
		auto ttMove = NullMove;

		if (m_table.probe(entry, pos.key(), 0, ply, alpha, beta))
			return entry.score;
		else if (entry.move && pos.isPseudolegal(entry.move))
			ttMove = entry.move;

		auto best = NullMove;
		auto bestScore = staticEval;

		auto entryType = EntryType::Alpha;

		QMoveGenerator generator{pos, NullMove, data.moveStack[moveStackIdx].moves, ttMove};

		while (const auto move = generator.next())
		{
			const auto guard = pos.applyMove(move, &data.nnueState, &m_table);

			if (!guard)
				continue;

			++data.search.nodes;

			const auto score = pos.isDrawn(false)
				? drawScore(data.search.nodes)
				: -qsearch(data, ply + 1, moveStackIdx + 1, -beta, -alpha);

			if (score > bestScore)
			{
				bestScore = score;

				if (score > alpha)
				{
					if (score >= beta)
					{
						entryType = EntryType::Beta;
						break;
					}

					alpha = score;
					entryType = EntryType::Exact;
				}
			}
		}

		m_table.put(pos.key(), bestScore, best, 0, ply, entryType);

		return bestScore;
	}

	auto Searcher::report(const ThreadData &data, i32 depth,
		Move move, f64 time, Score score, Score alpha, Score beta, bool tbRoot) -> void
	{
		usize nodes = 0;

		// in tb positions at root we have searched 0 nodes
		if (!tbRoot)
		{
			// technically a potential race but it doesn't matter
			for (const auto &thread : m_threads)
			{
				nodes += thread.search.nodes;
			}
		}

		const auto ms = tbRoot ? 0 : static_cast<usize>(time * 1000.0);
		const auto nps = tbRoot ? 0 : static_cast<usize>(static_cast<f64>(nodes) / time);

		std::cout << "info depth " << depth << " seldepth " << data.search.seldepth
			<< " time " << ms << " nodes " << nodes << " nps " << nps << " score ";

		score = std::clamp(score, alpha, beta);

		if (std::abs(score) <= 2)
			score = 0;

		// mates
		if (std::abs(score) > ScoreTbWin)
		{
			if (score > 0)
				std::cout << "mate " << ((ScoreMate - score + 1) / 2);
			else std::cout << "mate " << (-(ScoreMate + score) / 2);
		}
		else
		{
			// adjust score to 100cp == 50% win probability
			const auto normScore = uci::normalizeScore(score);
			std::cout << "cp " << normScore;
		}

		if (score == alpha)
			std::cout << " upperbound";
		else if (score == beta)
			std::cout << " lowerbound";

		// wdl display
		if (g_opts.showWdl)
		{
			if (score > ScoreWin)
				std::cout << " wdl 1000 0 0";
			else if (score < -ScoreWin)
				std::cout << " wdl 0 0 1000";
				// tablebase draws at the root
			else if (tbRoot)
				std::cout << " wdl 0 1000 0";
			else
			{
				const auto plyFromStartpos = data.pos.fullmove() * 2 - (data.pos.toMove() == Color::White ? 1 : 0) - 1;

				const auto [wdlWin, wdlLoss] = uci::winRateModel(score, plyFromStartpos);
				const auto wdlDraw = 1000 - wdlWin - wdlLoss;

				std::cout << " wdl " << wdlWin << " " << wdlDraw << " " << wdlLoss;
			}
		}

		std::cout << " hashfull " << m_table.full();

		if (g_opts.syzygyEnabled)
		{
			if (tbRoot)
				std::cout << " tbhits 1";
			else
			{
				usize tbhits = 0;

				// technically a potential race but it doesn't matter
				for (const auto &thread : m_threads)
				{
					tbhits += thread.search.tbhits;
				}

				std::cout << " tbhits " << tbhits;
			}
		}

		std::cout << " pv " << uci::moveToString(move);

		Position pos{data.pos};
		pos.applyMoveUnchecked<false, false>(move, nullptr);

		StaticVector<u64, MaxDepth> positionsHit{};
		positionsHit.push(pos.key());

		while (true)
		{
			const auto pvMove = m_table.probePvMove(pos.key());

			if (pvMove && pos.isPseudolegal(pvMove))
			{
				const bool legal = pos.applyMoveUnchecked<false, false>(pvMove, nullptr);

				if (legal && std::find(positionsHit.begin(), positionsHit.end(), pos.key()) == positionsHit.end())
				{
					std::cout << ' ' << uci::moveToString(pvMove);
					positionsHit.push(pos.key());
				}
				else break;
			}
			else break;
		}

		std::cout << std::endl;
	}
}
