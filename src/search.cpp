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

#include "search.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include "3rdparty/pyrrhic/tbprobe.h"
#include "limit/trivial.h"
#include "opts.h"
#include "see.h"
#include "stats.h"
#include "uci.h"

namespace stormphrax::search {
    using namespace stormphrax::tunable;

    using util::Instant;

    namespace {
        constexpr f64 kWidenReportDelay = 1.0;
        constexpr f64 kCurrmoveReportDelay = 2.5;

        // [improving][clamped depth]
        constexpr auto kLmpTable = [] {
            util::MultiArray<i32, 2, 16> result{};

            for (i32 improving = 0; improving < 2; ++improving) {
                for (i32 depth = 0; depth < 16; ++depth) {
                    result[improving][depth] = (3 + depth * depth) / (2 - improving);
                }
            }

            return result;
        }();

        [[nodiscard]] constexpr Score drawScore(usize nodes) {
            return 2 - static_cast<Score>(nodes % 4);
        }

        inline void generateLegal(MoveList& moves, const Position& pos) {
            ScoredMoveList generated{};
            generateAll(generated, pos);

            for (const auto [move, _s] : generated) {
                if (pos.isLegal(move)) {
                    moves.push(move);
                }
            }
        }

        [[nodiscard]] constexpr bool isWin(Score score) {
            return std::abs(score) > kScoreWin;
        }
    } // namespace

    Searcher::Searcher(usize ttSizeMib) :
            m_ttable{ttSizeMib}, m_startTime{Instant::now()} {
        m_threadData.resize(1);
        m_threads.emplace_back([this] { run(0); });
        m_initBarrier.arriveAndWait();
    }

    void Searcher::newGame() {
        // Finalisation (init) clears the TT, so don't clear it twice
        if (!m_ttable.finalize()) {
            m_ttable.clear();
        }

        for (auto& thread : m_threadData) {
            thread->history.clear();
            thread->correctionHistory.clear();
        }
    }

    void Searcher::ensureReady() {
        m_ttable.finalize();
    }

    void Searcher::startSearch(
        const Position& pos,
        std::span<const u64> keyHistory,
        Instant startTime,
        i32 maxDepth,
        std::span<Move> moves,
        std::unique_ptr<limit::ISearchLimiter> limiter,
        bool infinite
    ) {
        if (!m_limiter && !limiter) {
            eprintln("mising limiter");
            return;
        }

        const auto initStart = Instant::now();

        if (m_ttable.finalize()) {
            const auto initTime = initStart.elapsed();
            println(
                "info string No ucinewgame or isready before go, lost {} ms to TT initialization",
                static_cast<u32>(initTime * 1000.0)
            );
        }

        m_resetBarrier.arriveAndWait();

        m_infinite = infinite;
        m_maxDepth = maxDepth;

        m_minRootScore = -kScoreInf;
        m_maxRootScore = kScoreInf;

        std::optional<tb::ProbeResult> rootProbeResult{};

        if (!moves.empty()) {
            m_rootMoveList.resize(moves.size());
            std::ranges::copy(moves, m_rootMoveList.begin());

            m_rootStatus = RootStatus::kSearchmoves;
        } else {
            std::tie(m_rootStatus, rootProbeResult) = initRootMoveList(pos);

            if (m_rootStatus == RootStatus::kNoLegalMoves) {
                println("info string no legal moves");
                return;
            }
        }

        assert(!m_rootMoveList.empty());

        m_multiPv = std::min<u32>(g_opts.multiPv, m_rootMoveList.size());

        if (limiter) {
            m_limiter = std::move(limiter);
        }

        // Cap search time if we have one legal move, or if we're in a TB draw at root
        if (m_rootMoveList.size() == 1 || (rootProbeResult && *rootProbeResult == tb::ProbeResult::kDraw)) {
            m_limiter->stopEarly();
        }

        const auto contempt = g_opts.contempt;

        m_contempt[static_cast<i32>(pos.stm())] = contempt;
        m_contempt[static_cast<i32>(pos.nstm())] = -contempt;

        m_setupInfo.rootPos = pos;

        m_setupInfo.keyHistorySize = util::pad<usize{256}>(keyHistory.size());
        m_setupInfo.keyHistory = keyHistory;

        m_startTime = startTime;

        m_stop.store(false, std::memory_order::seq_cst);
        m_runningThreads.store(static_cast<i32>(m_threads.size()));

        m_searching.store(true, std::memory_order::relaxed);

        m_idleBarrier.arriveAndWait();
        m_setupBarrier.arriveAndWait();
    }

    void Searcher::stop() {
        m_stop.store(true, std::memory_order::relaxed);
        waitForStop();
    }

    void Searcher::waitForStop() {
        std::unique_lock lock{m_stopMutex};
        if (m_runningThreads.load() > 0) {
            m_stopSignal.wait(lock, [this] { return m_runningThreads.load(std::memory_order::seq_cst) == 0; });
        }
    }

    std::pair<Score, Score> Searcher::runDatagenSearch(ThreadData& thread) {
        if (initRootMoveList(thread.rootPos).first == RootStatus::kNoLegalMoves) {
            return {-kScoreMate, -kScoreMate};
        }

        m_multiPv = 1;
        m_infinite = false;

        m_stop.store(false, std::memory_order::seq_cst);

        const auto score = searchRoot(thread, false);

        m_ttable.age();

        const auto whitePovScore = thread.rootPos.stm() == Color::kBlack ? -score : score;
        return {whitePovScore, wdl::normalizeScore(whitePovScore, thread.rootPos.classicalMaterial())};
    }

    void Searcher::runBench(BenchData& data, const Position& pos, i32 depth) {
        m_limiter = std::make_unique<limit::InfiniteLimiter>();
        m_infinite = false;

        m_contempt = {};

        m_maxDepth = depth;
        m_multiPv = 1;

        // this struct is a small boulder the size of a large boulder
        // and overflows the stack if not on the heap
        auto thread = std::make_unique<ThreadData>();

        thread->rootPos = pos;
        thread->nnueState.reset(thread->rootPos.bbs(), thread->rootPos.kings());

        if (initRootMoveList(thread->rootPos).first == RootStatus::kNoLegalMoves) {
            return;
        }

        m_stop.store(false, std::memory_order::seq_cst);

        const auto start = Instant::now();

        searchRoot(*thread, false);

        m_ttable.age();

        data.search = thread->search;
        data.time = start.elapsed();
    }

    void Searcher::setThreads(u32 threadCount) {
        if (threadCount == m_threads.size()) {
            return;
        }

        stopThreads();

        m_quit.store(false, std::memory_order::seq_cst);

        m_threads.clear();
        m_threads.shrink_to_fit();
        m_threads.reserve(threadCount);

        m_threadData.clear();
        m_threadData.resize(threadCount);
        m_threadData.shrink_to_fit();

        m_initBarrier.reset(threadCount + 1);

        m_resetBarrier.reset(threadCount + 1);
        m_idleBarrier.reset(threadCount + 1);
        m_setupBarrier.reset(threadCount + 1);

        m_searchEndBarrier.reset(threadCount);

        for (u32 threadId = 0; threadId < threadCount; ++threadId) {
            m_threads.emplace_back([this, threadId] { run(threadId); });
        }

        m_initBarrier.arriveAndWait();
    }

    std::pair<RootStatus, std::optional<tb::ProbeResult>> Searcher::initRootMoveList(const Position& pos) {
        m_rootMoveList.clear();

        if (g_opts.syzygyEnabled
            && pos.bbs().occupancy().popcount() <= std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST)))
        {
            bool success = true;

            const auto result = tb::probeRoot(&m_rootMoveList, pos);

            switch (result) {
                case tb::ProbeResult::kWin:
                    m_minRootScore = kScoreTbWin;
                    break;
                case tb::ProbeResult::kDraw:
                    m_minRootScore = m_maxRootScore = 0;
                    break;
                case tb::ProbeResult::kLoss:
                    m_maxRootScore = -kScoreTbWin;
                    break;
                default:
                    success = false;
                    break;
            }

            if (success) {
                assert(!m_rootMoveList.empty());
                return {RootStatus::kTablebase, result};
            }
        }

        if (m_rootMoveList.empty()) {
            generateLegal(m_rootMoveList, pos);
        }

        return {m_rootMoveList.empty() ? RootStatus::kNoLegalMoves : RootStatus::kGenerated, tb::ProbeResult::kFailed};
    }

    void Searcher::stopThreads() {
        m_quit.store(true, std::memory_order::release);

        m_resetBarrier.arriveAndWait();
        m_idleBarrier.arriveAndWait();

        for (auto& thread : m_threads) {
            thread.join();
        }
    }

    void Searcher::run(u32 threadId) {
        // Ensure thread data is allocated on the correct
        // NUMA node by initialising it from this thread
        m_threadData[threadId] = std::make_unique<ThreadData>();

        auto& thread = *m_threadData[threadId];

        thread.id = threadId;

        m_initBarrier.arriveAndWait();

        while (true) {
            m_resetBarrier.arriveAndWait();
            m_idleBarrier.arriveAndWait();

            if (m_quit.load(std::memory_order::acquire)) {
                return;
            }

            searchRoot(thread, true);
        }
    }

    Score Searcher::searchRoot(ThreadData& thread, bool actualSearch) {
        if (actualSearch) {
            thread.search = SearchData{};
            thread.rootPos = m_setupInfo.rootPos;

            thread.keyHistory.clear();
            thread.keyHistory.reserve(m_setupInfo.keyHistorySize);

            std::ranges::copy(m_setupInfo.keyHistory, std::back_inserter(thread.keyHistory));

            thread.nnueState.reset(thread.rootPos.bbs(), thread.rootPos.kings());

            m_setupBarrier.arriveAndWait();
        }

        assert(!m_rootMoveList.empty());

        thread.rootMoves.clear();
        thread.rootMoves.reserve(m_rootMoveList.size());

        for (const auto move : m_rootMoveList) {
            auto& rootMove = thread.rootMoves.emplace_back();

            rootMove.pv.moves[0] = move;
            rootMove.pv.length = 1;
        }

        auto& searchData = thread.search;

        const bool mainThread = actualSearch && thread.isMainThread();

        PvList rootPv{};

        searchData.nodes = 0;
        thread.stack[0].killers.clear();

        thread.depthCompleted = 0;

        for (i32 depth = 1;; ++depth) {
            searchData.rootDepth = depth;

            for (thread.pvIdx = 0; thread.pvIdx < m_multiPv; ++thread.pvIdx) {
                searchData.seldepth = 0;

                auto delta = initialAspWindow();

                auto alpha = -kScoreInf;
                auto beta = kScoreInf;

                if (depth >= 3) {
                    const auto lastScore = thread.rootMoves[thread.pvIdx].score;

                    alpha = std::max(lastScore - delta, -kScoreInf);
                    beta = std::min(lastScore + delta, kScoreInf);
                }

                Score newScore{};

                i32 aspReduction = 0;

                while (!hasStopped()) {
                    // count the root node
                    searchData.incNodes();

                    const auto aspDepth = std::max(depth - aspReduction, 1); // paranoia
                    newScore = search<true, true>(thread, thread.rootPos, rootPv, aspDepth, 0, 0, alpha, beta, false);

                    std::stable_sort(
                        thread.rootMoves.begin() + thread.pvIdx,
                        thread.rootMoves.end(),
                        [](const RootMove& a, const RootMove& b) { return a.score > b.score; }
                    );

                    if ((newScore > alpha && newScore < beta) || hasStopped()) {
                        break;
                    }

                    if (mainThread && g_opts.multiPv == 1) {
                        const auto time = elapsed();
                        if (time >= kWidenReportDelay) {
                            reportSingle(thread, thread.pvIdx, depth, time);
                        }
                    }

                    if (newScore <= alpha) {
                        aspReduction = 0;

                        beta = (alpha + beta) / 2;
                        alpha = std::max(newScore - delta, -kScoreInf);
                    } else {
                        aspReduction = std::min(aspReduction + 1, 3);
                        beta = std::min(newScore + delta, kScoreInf);
                    }

                    delta += delta * aspWideningFactor() / 16;
                }

                std::ranges::stable_sort(thread.rootMoves, [](const RootMove& a, const RootMove& b) {
                    return a.score > b.score;
                });

                assert(thread.pvMove().pv.length > 0);

                if (hasStopped()) {
                    break;
                }
            }

            if (hasStopped()) {
                break;
            }

            thread.depthCompleted = depth;

            if (depth >= m_maxDepth) {
                if (mainThread && m_infinite) {
                    report(thread, searchData.rootDepth, elapsed());
                }
                break;
            }

            if (mainThread) {
                m_limiter->update(
                    thread.search,
                    thread.pvMove().score,
                    thread.pvMove().pv.moves[0],
                    thread.search.loadNodes()
                );

                if (checkSoftTimeout(thread.search, true)) {
                    break;
                }

                report(thread, searchData.rootDepth, elapsed());
            } else if (checkSoftTimeout(thread.search, thread.isMainThread())) {
                break;
            }
        }

        const auto waitForThreads = [&] {
            {
                const std::unique_lock lock{m_stopMutex};
                --m_runningThreads;
                m_stopSignal.notify_all();
            }

            m_searchEndBarrier.arriveAndWait();
        };

        if (mainThread) {
            if (m_infinite) {
                // don't print bestmove until stopped when go infinite'ing
                while (!hasStopped()) {
                    std::this_thread::yield();
                }
            }

            const std::unique_lock lock{m_searchMutex};

            m_stop.store(true, std::memory_order::seq_cst);
            waitForThreads();

            finalReport();

            m_ttable.age();
            stats::print();

            m_searching.store(false, std::memory_order::relaxed);
        } else {
            waitForThreads();
        }

        return thread.pvMove().score;
    }

    template <bool kPvNode, bool kRootNode>
    Score Searcher::search(
        ThreadData& thread,
        const Position& pos,
        PvList& pv,
        i32 depth,
        i32 ply,
        u32 moveStackIdx,
        Score alpha,
        Score beta,
        bool cutnode
    ) {
        assert(ply >= 0 && ply <= kMaxDepth);
        assert(kRootNode || ply > 0);
        assert(kPvNode || alpha + 1 == beta);

        if (ply > 0 && checkHardTimeout(thread.search, thread.isMainThread())) {
            return 0;
        }

        const auto& boards = pos.boards();
        const auto& bbs = pos.bbs();

        if constexpr (!kRootNode) {
            alpha = std::max(alpha, -kScoreMate + ply);
            beta = std::min(beta, kScoreMate - ply - 1);

            if (alpha >= beta) {
                return alpha;
            }

            if (alpha < 0 && pos.hasCycle(ply, thread.keyHistory)) {
                alpha = drawScore(thread.search.loadNodes());
                if (alpha >= beta) {
                    return alpha;
                }
            }
        }

        if (depth <= 0) {
            return qsearch<kPvNode>(thread, pos, ply, moveStackIdx, alpha, beta);
        }

        thread.search.updateSeldepth(ply);

        const bool inCheck = pos.isCheck();

        if (ply >= kMaxDepth) {
            return inCheck ? 0
                           : eval::adjustedStaticEval(
                                 pos,
                                 thread.contMoves,
                                 ply,
                                 thread.nnueState,
                                 &thread.correctionHistory,
                                 m_contempt
                             );
        }

        const auto us = pos.stm();
        const auto them = oppColor(us);

        assert(!kPvNode || !cutnode);

        const auto* parent = kRootNode ? nullptr : &thread.stack[ply - 1];
        auto& curr = thread.stack[ply];

        assert(!kRootNode || curr.excluded == kNullMove);

        auto& moveStack = thread.moveStack[moveStackIdx];

        ProbedTTableEntry ttEntry{};
        bool ttHit = false;

        if (!curr.excluded) {
            ttHit = m_ttable.probe(ttEntry, pos.key(), ply);

            if (!kPvNode && ttEntry.depth >= depth && (ttEntry.score <= alpha || cutnode)
                && (ttEntry.flag == TtFlag::kExact                                   //
                    || ttEntry.flag == TtFlag::kUpperBound && ttEntry.score <= alpha //
                    || ttEntry.flag == TtFlag::kLowerBound && ttEntry.score >= beta))
            {
                if (ttEntry.score >= beta && ttEntry.move && !pos.isNoisy(ttEntry.move)
                    && pos.isPseudolegal(ttEntry.move))
                {
                    const auto bonus = historyBonus(depth);
                    thread.history.updateQuietScore(
                        thread.conthist,
                        ply,
                        pos.threats(),
                        boards.pieceOn(ttEntry.move.fromSq()),
                        ttEntry.move,
                        bonus
                    );
                }

                return ttEntry.score;
            }
        }

        const auto ttMove =
            (kRootNode && thread.search.rootDepth > 1) ? thread.rootMoves[thread.pvIdx].pv.moves[0] : ttEntry.move;

        const bool ttMoveNoisy = ttMove && pos.isNoisy(ttMove);
        const bool ttpv = kPvNode || ttEntry.wasPv;

        const auto pieceCount = bbs.occupancy().popcount();

        auto syzygyMin = -kScoreMate;
        auto syzygyMax = kScoreMate;

        const auto syzygyPieceLimit = std::min(g_opts.syzygyProbeLimit, static_cast<i32>(TB_LARGEST));

        // Probe the Syzygy tablebases for a WDL result
        // if there are few enough pieces left on the board
        if (!kRootNode && !curr.excluded && g_opts.syzygyEnabled && pieceCount <= syzygyPieceLimit
            && (pieceCount < syzygyPieceLimit || depth >= g_opts.syzygyProbeDepth) && pos.halfmove() == 0
            && pos.castlingRooks() == CastlingRooks{})
        {
            const auto result = tb::probe(pos);

            if (result != tb::ProbeResult::kFailed) {
                thread.search.incTbHits();

                Score score{};
                TtFlag flag{};

                if (result == tb::ProbeResult::kWin) {
                    score = kScoreTbWin - ply;
                    flag = TtFlag::kLowerBound;
                } else if (result == tb::ProbeResult::kLoss) {
                    score = -kScoreTbWin + ply;
                    flag = TtFlag::kUpperBound;
                } else { // draw
                    score = drawScore(thread.search.loadNodes());
                    flag = TtFlag::kExact;
                }

                if (flag == TtFlag::kExact                           //
                    || flag == TtFlag::kUpperBound && score <= alpha //
                    || flag == TtFlag::kLowerBound && score >= beta)
                {
                    m_ttable.put(pos.key(), score, kScoreNone, kNullMove, depth, ply, flag, ttpv);
                    return score;
                }

                if constexpr (kPvNode) {
                    if (flag == TtFlag::kUpperBound) {
                        syzygyMax = score;
                    } else { // lower bound (win)
                        if (score > alpha) {
                            alpha = score;
                        }
                        syzygyMin = score;
                    }
                }
            }
        }

        if (depth >= 3 && !curr.excluded && (kPvNode || cutnode) && (!ttMove || ttEntry.depth + 3 < depth)) {
            --depth;
        }

        Score rawStaticEval{};
        std::optional<Score> complexity{};

        if (!curr.excluded) {
            if (inCheck) {
                rawStaticEval = kScoreNone;
            } else if (ttHit && ttEntry.staticEval != kScoreNone) {
                rawStaticEval = ttEntry.staticEval;
            } else {
                rawStaticEval = eval::staticEval(pos, thread.nnueState, m_contempt);
            }

            if (!ttHit) {
                m_ttable.put(pos.key(), kScoreNone, rawStaticEval, kNullMove, 0, 0, TtFlag::kNone, ttpv);
            }

            if (inCheck) {
                curr.staticEval = kScoreNone;
            } else {
                Score corrDelta{};
                curr.staticEval =
                    eval::adjustEval(pos, thread.contMoves, ply, &thread.correctionHistory, rawStaticEval, &corrDelta);
                complexity = corrDelta;
            }
        }

        const bool improving = [&] {
            if (inCheck) {
                return false;
            }
            if (ply > 1 && thread.stack[ply - 2].staticEval != kScoreNone) {
                return curr.staticEval > thread.stack[ply - 2].staticEval;
            }
            if (ply > 3 && thread.stack[ply - 4].staticEval != kScoreNone) {
                return curr.staticEval > thread.stack[ply - 4].staticEval;
            }
            return true;
        }();

        if (!kPvNode && !inCheck && !curr.excluded) {
            if (parent->reduction >= 3 && parent->staticEval != kScoreNone && curr.staticEval + parent->staticEval <= 0)
            {
                ++depth;
            }

            const auto rfpMargin = [&] {
                auto margin = tunable::rfpMargin() * std::max(depth - improving, 0);
                if (complexity) {
                    margin += *complexity * rfpCorrplexityScale() / 128;
                }
                return margin;
            };

            if (depth <= 6 && curr.staticEval - rfpMargin() >= beta) {
                return !isWin(curr.staticEval) && !isWin(beta) ? (curr.staticEval + beta) / 2 : curr.staticEval;
            }

            if (depth <= 4 && std::abs(alpha) < 2000 && curr.staticEval + razoringMargin() * depth <= alpha) {
                const auto score = qsearch(thread, pos, ply, moveStackIdx, alpha, alpha + 1);
                if (score <= alpha) {
                    return score;
                }
            }

            if (depth >= 4 && ply >= thread.minNmpPly && curr.staticEval >= beta && !parent->move.isNull()
                && !(ttEntry.flag == TtFlag::kUpperBound && ttEntry.score < beta) && !bbs.nonPk(us).empty())
            {
                m_ttable.prefetch(pos.key() ^ keys::color());

                const auto R = 6 + depth / 5;

                const auto score = [&] {
                    const auto [newPos, guard] = thread.applyNullmove(pos, ply);
                    return -search(
                        thread,
                        newPos,
                        curr.pv,
                        depth - R,
                        ply + 1,
                        moveStackIdx,
                        -beta,
                        -beta + 1,
                        !cutnode
                    );
                }();

                if (score >= beta) {
                    if (depth <= 14 || thread.minNmpPly > 0) {
                        return score > kScoreWin ? beta : score;
                    }

                    thread.minNmpPly = ply + (depth - R) * 3 / 4;

                    const auto verifScore =
                        search(thread, pos, curr.pv, depth - R, ply, moveStackIdx + 1, beta - 1, beta, true);

                    thread.minNmpPly = 0;

                    if (verifScore >= beta) {
                        return verifScore;
                    }
                }
            }

            const auto probcutBeta = beta + probcutMargin();
            const auto probcutDepth = std::max(depth - 3, 1);

            if (!ttpv && depth >= 7 && std::abs(beta) < kScoreWin && (!ttMove || ttMoveNoisy)
                && !(ttHit && ttEntry.depth >= probcutDepth && ttEntry.score < probcutBeta))
            {
                const auto seeThreshold = (probcutBeta - curr.staticEval) * probcutSeeScale() / 16;

                auto generator = MoveGenerator::probcut(pos, ttMove, moveStack.movegenData, thread.history);

                while (const auto move = generator.next()) {
                    if (!pos.isLegal(move)) {
                        continue;
                    }

                    if (!see::see(pos, move, seeThreshold)) {
                        continue;
                    }

                    thread.search.incNodes();

                    m_ttable.prefetch(pos.roughKeyAfter(move));

                    const auto [newPos, guard] = thread.applyMove(pos, ply, move);

                    auto score = -qsearch(thread, newPos, ply + 1, moveStackIdx + 1, -probcutBeta, -probcutBeta + 1);

                    if (score >= probcutBeta) {
                        score = -search(
                            thread,
                            newPos,
                            curr.pv,
                            probcutDepth - 1,
                            ply + 1,
                            moveStackIdx + 1,
                            -probcutBeta,
                            -probcutBeta + 1,
                            !cutnode
                        );
                    }

                    if (hasStopped()) {
                        return 0;
                    }

                    if (score >= probcutBeta) {
                        m_ttable.put(
                            pos.key(),
                            score,
                            curr.staticEval,
                            move,
                            probcutDepth,
                            ply,
                            TtFlag::kLowerBound,
                            false
                        );
                        return score;
                    }
                }
            }
        }

        thread.stack[ply + 1].killers.clear();

        moveStack.failLowQuiets.clear();
        moveStack.failLowNoisies.clear();

        auto bestMove = kNullMove;
        auto bestScore = -kScoreInf;

        auto ttFlag = TtFlag::kUpperBound;

        auto generator =
            MoveGenerator::main(pos, moveStack.movegenData, ttMove, curr.killers, thread.history, thread.conthist, ply);

        u32 legalMoves = 0;

        while (const auto move = generator.next()) {
            if (move == curr.excluded) {
                continue;
            }

            if constexpr (kRootNode) {
                if (!thread.isLegalRootMove(move)) {
                    continue;
                }

                assert(pos.isLegal(move));
            } else if (!pos.isLegal(move)) {
                continue;
            }

            const bool quietOrLosing = generator.stage() > MovegenStage::kGoodNoisy;

            const bool noisy = pos.isNoisy(move);
            const auto moving = boards.pieceOn(move.fromSq());

            const auto captured = pos.captureTarget(move);

            const auto baseLmr = g_lmrTable[noisy][depth][legalMoves + 1];

            const auto history = noisy ? thread.history.noisyScore(move, captured, pos.threats())
                                       : thread.history.quietScore(thread.conthist, ply, pos.threats(), moving, move);

            if ((!kRootNode || thread.search.rootDepth == 1) && bestScore > -kScoreWin && (!kPvNode || !thread.datagen))
            {
                const auto lmrDepth = std::max(depth - baseLmr / 128, 0);

                if (!noisy) {
                    if (legalMoves >= kLmpTable[improving][std::min(depth, 15)]) {
                        generator.skipQuiets();
                        continue;
                    }

                    if (lmrDepth <= 5 && history < quietHistPruningMargin() * depth + quietHistPruningOffset()) {
                        generator.skipQuiets();
                        continue;
                    }

                    if (!inCheck && lmrDepth <= 8 && std::abs(alpha) < 2000
                        && curr.staticEval + fpMargin() + depth * fpScale() <= alpha)
                    {
                        generator.skipQuiets();
                        continue;
                    }
                } else if (depth <= 4 && history < noisyHistPruningMargin() * depth * depth + noisyHistPruningOffset())
                {
                    continue;
                }

                const auto seeThreshold =
                    noisy ? seePruningThresholdNoisy() * depth : seePruningThresholdQuiet() * lmrDepth * lmrDepth;

                if (quietOrLosing && !see::see(pos, move, seeThreshold)) {
                    continue;
                }
            }

            if constexpr (kPvNode) {
                curr.pv.length = 0;
            }

            const auto prevNodes = thread.search.loadNodes();

            thread.search.incNodes();
            ++legalMoves;

            if (kRootNode && g_opts.showCurrMove && elapsed() > kCurrmoveReportDelay) {
                println("info depth {} currmove {} currmovenumber {}", depth, move, legalMoves);
            }

            i32 extension{};

            if (!kRootNode && ply < thread.search.rootDepth * 2 && move == ttMove && !curr.excluded) {
                if (depth >= 8 && ttEntry.depth >= depth - 5 && ttEntry.flag != TtFlag::kUpperBound
                    && !isWin(ttEntry.score))
                {
                    const auto sBeta = ttEntry.score - depth * sBetaMargin() / 16;
                    const auto sDepth = (depth - 1) / 2;

                    curr.excluded = move;

                    const auto score =
                        search(thread, pos, curr.pv, sDepth, ply, moveStackIdx + 1, sBeta - 1, sBeta, cutnode);

                    curr.excluded = kNullMove;

                    if (score < sBeta) {
                        if (!kPvNode && score < sBeta - doubleExtMargin()) {
                            extension = 2 + (!ttMoveNoisy && score < sBeta - tripleExtMargin());
                        } else {
                            extension = 1;
                        }
                    } else if (sBeta >= beta) {
                        return sBeta;
                    } else if (cutnode) {
                        extension = -2;
                    } else if (ttEntry.score >= beta) {
                        extension = -1;
                    }
                } else if (depth <= 7 && !inCheck && curr.staticEval <= alpha - ldseMargin()
                           && ttEntry.flag == TtFlag::kLowerBound)
                {
                    extension = 1;
                }
            }

            cutnode |= extension < 0;

            m_ttable.prefetch(pos.roughKeyAfter(move));

            const auto [newPos, guard] = thread.applyMove(pos, ply, move);

            const bool givesCheck = newPos.isCheck();

            Score score{};

            if (newPos.isDrawn(ply, thread.keyHistory)) {
                score = drawScore(thread.search.loadNodes());
            } else {
                auto newDepth = depth + extension - 1;

                if (depth >= 2 && legalMoves >= 2 + kRootNode) {
                    auto r = baseLmr;

                    r += !kPvNode * lmrNonPvReductionScale();
                    r -= ttpv * lmrTtpvReductionScale();
                    r -= history * 128 / (noisy ? lmrNoisyHistoryDivisor() : lmrQuietHistoryDivisor());
                    r -= improving * lmrImprovingReductionScale();
                    r -= givesCheck * lmrCheckReductionScale();
                    r += cutnode * lmrCutnodeReductionScale();
                    r += (ttpv && ttHit && ttEntry.score <= alpha) * lmrTtpvFailLowReductionScale();

                    if (complexity) {
                        const bool highComplexity = *complexity > lmrHighComplexityThreshold();
                        r -= lmrHighComplexityReductionScale() * highComplexity;
                    }

                    r /= 128;

                    // can't use std::clamp because newDepth can be <0
                    const auto reduced = std::min(std::max(newDepth - r, 1), newDepth) + kPvNode;

                    curr.reduction = newDepth - reduced;
                    score =
                        -search(thread, newPos, curr.pv, reduced, ply + 1, moveStackIdx + 1, -alpha - 1, -alpha, true);
                    curr.reduction = 0;

                    if (score > alpha && reduced < newDepth) {
                        const bool doDeeperSearch = score > bestScore + lmrDeeperBase() + lmrDeeperScale() * newDepth;
                        const bool doShallowerSearch = score < bestScore + newDepth;

                        newDepth += doDeeperSearch - doShallowerSearch;

                        score = -search(
                            thread,
                            newPos,
                            curr.pv,
                            newDepth,
                            ply + 1,
                            moveStackIdx + 1,
                            -alpha - 1,
                            -alpha,
                            !cutnode
                        );

                        if (!noisy && (score <= alpha || score >= beta)) {
                            const auto bonus = score <= alpha ? historyPenalty(newDepth) : historyBonus(newDepth);
                            thread.history.updateConthist(thread.conthist, ply, moving, move, bonus);
                        }
                    }
                }
                // if we're skipping LMR for some reason (first move in a non-PV
                // node, or the conditions above for LMR were not met) then do an
                // unreduced zero-window search to check if this move can raise alpha
                else if (!kPvNode || legalMoves > 1)
                {
                    score = -search(
                        thread,
                        newPos,
                        curr.pv,
                        newDepth,
                        ply + 1,
                        moveStackIdx + 1,
                        -alpha - 1,
                        -alpha,
                        !cutnode
                    );
                }

                // if we're in a PV node and
                //   - we're searching the first legal move, or
                //   - alpha was raised by a previous zero-window search,
                // then do a full-window search to get the true score of this node
                if (kPvNode && (legalMoves == 1 || score > alpha)) {
                    if (thread.search.rootDepth >= 9 && newDepth <= 0 && move == ttEntry.move && ttEntry.depth >= 3) {
                        newDepth = 1;
                    }

                    score = -search<
                        true>(thread, newPos, curr.pv, newDepth, ply + 1, moveStackIdx + 1, -beta, -alpha, false);
                }
            }

            if (hasStopped()) {
                return 0;
            }

            if constexpr (kRootNode) {
                if (thread.isMainThread()) {
                    m_limiter->updateMoveNodes(move, thread.search.loadNodes() - prevNodes);
                }

                auto* rootMove = thread.findRootMove(move);

                if (!rootMove) {
                    eprintln("Failed to find root move for {}", move);
                    std::terminate();
                }

                if (legalMoves == 1 || score > alpha) {
                    rootMove->seldepth = thread.search.seldepth;

                    rootMove->displayScore = score;
                    rootMove->score = score;

                    rootMove->upperbound = false;
                    rootMove->lowerbound = false;

                    if (score <= alpha) {
                        rootMove->displayScore = alpha;
                        rootMove->upperbound = true;
                    } else if (score >= beta) {
                        rootMove->displayScore = beta;
                        rootMove->lowerbound = true;
                    }

                    rootMove->pv.update(move, curr.pv);
                } else {
                    rootMove->score = -kScoreInf;
                }
            }

            if (score > bestScore) {
                bestScore = score;
            }

            if (score > alpha) {
                alpha = score;
                bestMove = move;

                if constexpr (kPvNode) {
                    assert(curr.pv.length + 1 <= kMaxDepth);
                    pv.update(move, curr.pv);
                }

                ttFlag = TtFlag::kExact;
            }

            if (score >= beta) {
                ttFlag = TtFlag::kLowerBound;
                break;
            }

            if (move != bestMove) {
                if (noisy) {
                    moveStack.failLowNoisies.push(move);
                } else {
                    moveStack.failLowQuiets.push(move);
                }
            }
        }

        if (legalMoves == 0) {
            if (curr.excluded) {
                return alpha;
            }
            return inCheck ? (-kScoreMate + ply) : 0;
        }

        if (bestMove) {
            const auto historyDepth = depth + (!inCheck && curr.staticEval <= bestScore);

            const auto bonus = historyBonus(historyDepth);
            const auto penalty = historyPenalty(historyDepth);

            if (!pos.isNoisy(bestMove)) {
                curr.killers.push(bestMove);

                thread.history.updateQuietScore(
                    thread.conthist,
                    ply,
                    pos.threats(),
                    pos.boards().pieceOn(bestMove.fromSq()),
                    bestMove,
                    bonus
                );

                for (const auto prevQuiet : moveStack.failLowQuiets) {
                    thread.history.updateQuietScore(
                        thread.conthist,
                        ply,
                        pos.threats(),
                        pos.boards().pieceOn(prevQuiet.fromSq()),
                        prevQuiet,
                        penalty
                    );
                }
            } else {
                const auto captured = pos.captureTarget(bestMove);
                thread.history.updateNoisyScore(bestMove, captured, pos.threats(), bonus);
            }

            // unconditionally update capthist
            for (const auto prevNoisy : moveStack.failLowNoisies) {
                const auto captured = pos.captureTarget(prevNoisy);
                thread.history.updateNoisyScore(prevNoisy, captured, pos.threats(), penalty);
            }
        }

        if (bestScore >= beta && !isWin(bestScore) && !isWin(beta)) {
            bestScore = (bestScore * depth + beta) / (depth + 1);
        }

        bestScore = std::clamp(bestScore, syzygyMin, syzygyMax);

        if (!curr.excluded) {
            if (!inCheck && (bestMove.isNull() || !pos.isNoisy(bestMove))
                && (ttFlag == TtFlag::kExact                                        //
                    || ttFlag == TtFlag::kUpperBound && bestScore < curr.staticEval //
                    || ttFlag == TtFlag::kLowerBound && bestScore > curr.staticEval))
            {
                thread.correctionHistory.update(pos, thread.contMoves, ply, depth, bestScore, curr.staticEval);
            }

            if (!kRootNode || thread.pvIdx == 0) {
                m_ttable.put(pos.key(), bestScore, rawStaticEval, bestMove, depth, ply, ttFlag, ttpv);
            }
        }

        return bestScore;
    }

    template <bool kPvNode>
    Score Searcher::qsearch(
        ThreadData& thread,
        const Position& pos,
        i32 ply,
        u32 moveStackIdx,
        Score alpha,
        Score beta
    ) {
        assert(ply > 0 && ply <= kMaxDepth);

        if (checkHardTimeout(thread.search, thread.isMainThread())) {
            return 0;
        }

        if (alpha < 0 && pos.hasCycle(ply, thread.keyHistory)) {
            alpha = drawScore(thread.search.loadNodes());
            if (alpha >= beta) {
                return alpha;
            }
        }

        const bool inCheck = pos.isCheck();

        if constexpr (kPvNode) {
            thread.search.updateSeldepth(ply);
        }

        if (ply >= kMaxDepth) {
            return inCheck ? 0
                           : eval::adjustedStaticEval(
                                 pos,
                                 thread.contMoves,
                                 ply,
                                 thread.nnueState,
                                 &thread.correctionHistory,
                                 m_contempt
                             );
        }

        ProbedTTableEntry ttEntry{};
        const bool ttHit = m_ttable.probe(ttEntry, pos.key(), ply);

        if (!kPvNode
            && (ttEntry.flag == TtFlag::kExact                                   //
                || ttEntry.flag == TtFlag::kUpperBound && ttEntry.score <= alpha //
                || ttEntry.flag == TtFlag::kLowerBound && ttEntry.score >= beta))
        {
            return ttEntry.score;
        }

        const bool ttpv = kPvNode || ttEntry.wasPv;

        Score rawStaticEval, eval;

        if (inCheck) {
            rawStaticEval = kScoreNone;
            eval = -kScoreMate + ply;
        } else {
            if (ttHit && ttEntry.staticEval != kScoreNone) {
                rawStaticEval = ttEntry.staticEval;
            } else {
                rawStaticEval = eval::staticEval(pos, thread.nnueState, m_contempt);
            }

            if (!ttHit) {
                m_ttable.put(pos.key(), kScoreNone, rawStaticEval, kNullMove, 0, 0, TtFlag::kNone, ttpv);
            }

            const auto staticEval =
                eval::adjustEval(pos, thread.contMoves, ply, &thread.correctionHistory, rawStaticEval);

            if (ttEntry.flag == TtFlag::kExact                                       //
                || ttEntry.flag == TtFlag::kUpperBound && ttEntry.score < staticEval //
                || ttEntry.flag == TtFlag::kLowerBound && ttEntry.score > staticEval)
            {
                eval = ttEntry.score;
            } else {
                eval = staticEval;
            }

            if (eval >= beta) {
                return !isWin(eval) && !isWin(beta) ? (eval + beta) / 2 : eval;
            }

            if (eval > alpha) {
                alpha = eval;
            }
        }

        const auto futility = eval + qsearchFpMargin();

        auto bestMove = kNullMove;
        auto bestScore = eval;

        auto ttFlag = TtFlag::kUpperBound;

        auto generator = MoveGenerator::qsearch(
            pos,
            thread.moveStack[moveStackIdx].movegenData,
            ttEntry.move,
            thread.history,
            thread.conthist,
            ply
        );

        u32 legalMoves = 0;

        while (const auto move = generator.next()) {
            if (!pos.isLegal(move)) {
                continue;
            }

            if (bestScore > -kScoreWin) {
                if (!inCheck && futility <= alpha && !see::see(pos, move, 1)) {
                    if (bestScore < futility) {
                        bestScore = futility;
                    }
                    continue;
                }

                if (legalMoves >= 2) {
                    break;
                }

                if (!see::see(pos, move, qsearchSeeThreshold())) {
                    continue;
                }
            }

            ++legalMoves;

            thread.search.incNodes();

            m_ttable.prefetch(pos.roughKeyAfter(move));

            const auto [newPos, guard] = thread.applyMove(pos, ply, move);

            const auto score = newPos.isDrawn(ply, thread.keyHistory)
                                 ? drawScore(thread.search.loadNodes())
                                 : -qsearch<kPvNode>(thread, newPos, ply + 1, moveStackIdx + 1, -beta, -alpha);

            if (hasStopped()) {
                return 0;
            }

            if (score > -kScoreWin) {
                generator.skipQuiets();
            }

            if (score > bestScore) {
                bestScore = score;
            }

            if (score > alpha) {
                alpha = score;
                bestMove = move;

                ttFlag = TtFlag::kExact;
            }

            if (score >= beta) {
                ttFlag = TtFlag::kLowerBound;
                break;
            }
        }

        // stalemate not handled
        if (inCheck && legalMoves == 0) {
            return -kScoreMate + ply;
        }

        m_ttable.put(pos.key(), bestScore, rawStaticEval, bestMove, 0, ply, ttFlag, ttpv);

        return bestScore;
    }

    void Searcher::reportSingle(const ThreadData& thread, u32 pvIdx, i32 depth, f64 time) {
        const auto& move = thread.rootMoves[pvIdx];

        auto score = move.score == -kScoreInf ? move.displayScore : move.score;
        depth = move.score == -kScoreInf ? std::max(1, depth - 1) : depth;

        usize nodes = 0;

        for (const auto& worker : m_threadData) {
            nodes += worker->search.loadNodes();
        }

        const auto ms = static_cast<usize>(time * 1000.0);
        const auto nps = static_cast<usize>(static_cast<f64>(nodes) / time);

        print("info ");

        if (g_opts.multiPv > 1) {
            print("multipv {} ", pvIdx + 1);
        }

        print("depth {} seldepth {} time {} nodes {} nps {} score ", depth, move.seldepth, ms, nodes, nps);

        if (std::abs(score) <= 2) { // draw score
            score = 0;
        }

        if (pvIdx == 0) {
            score = std::clamp(score, m_minRootScore, m_maxRootScore);
        }

        const auto material = thread.rootPos.classicalMaterial();

        // mates
        if (std::abs(score) >= kScoreMaxMate) {
            if (score > 0) {
                print("mate {}", (kScoreMate - score + 1) / 2);
            } else {
                print("mate {}", -(kScoreMate + score) / 2);
            }
        } else {
            // adjust score to 100cp == 50% win probability
            const auto normScore = wdl::normalizeScore(score, material);
            print("cp {}", normScore);
        }

        if (move.upperbound) {
            print(" upperbound");
        }

        if (move.lowerbound) {
            print(" lowerbound");
        }

        // wdl display
        if (g_opts.showWdl) {
            if (score > kScoreWin) {
                print(" wdl 1000 0 0");
            } else if (score < -kScoreWin) {
                print(" wdl 0 0 1000");
            } else {
                const auto [wdlWin, wdlLoss] = wdl::wdlModel(score, material);
                const auto wdlDraw = 1000 - wdlWin - wdlLoss;

                print(" wdl {} {} {}", wdlWin, wdlDraw, wdlLoss);
            }
        }

        print(" hashfull {}", m_ttable.full());

        if (g_opts.syzygyEnabled) {
            usize tbhits = 0;

            if (m_rootStatus == RootStatus::kTablebase) {
                ++tbhits;
            }

            for (const auto& worker : m_threadData) {
                tbhits += worker->search.loadTbHits();
            }

            print(" tbhits {}", tbhits);
        }

        print(" pv");

        for (u32 i = 0; i < move.pv.length; ++i) {
            print(" {}", move.pv.moves[i]);
        }

        println();
    }

    void Searcher::report(const ThreadData& thread, i32 depth, f64 time) {
        for (u32 pvIdx = 0; pvIdx < m_multiPv; ++pvIdx) {
            reportSingle(thread, pvIdx, depth, time);
        }
    }

    const ThreadData& Searcher::selectThread() {
        return *m_threadData[0];
    }

    void Searcher::finalReport() {
        const auto& bestThread = selectThread();

        report(bestThread, bestThread.depthCompleted, elapsed());
        println("bestmove {}", bestThread.pvMove().pv.moves[0]);
    }
} // namespace stormphrax::search
