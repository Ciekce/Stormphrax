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

#include "datagen.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>

#include <fmt/std.h>

#include "../3rdparty/pyrrhic/tbprobe.h"
#include "../limit.h"
#include "../movegen.h"
#include "../opts.h"
#include "../search.h"
#include "../tb.h"
#include "../util/ctrlc.h"
#include "../util/rng.h"
#include "../util/timer.h"
#include "fen.h"
#include "format.h"
#include "marlinformat.h"
#include "viriformat.h"

namespace stormphrax::datagen {
    using util::Instant;

    namespace {
        std::atomic_bool s_stop{false};

        void initCtrlCHandler() {
            util::signal::setCtrlCHandler([] { s_stop.store(true, std::memory_order::seq_cst); });
        }

        [[nodiscard]] std::optional<Outcome> probeTb(const Position& pos) {
            if (pos.bbs().occupancy().popcount() > TB_LARGEST || pos.halfmove() != 0
                || pos.castlingRooks() != CastlingRooks{})
            {
                return {};
            }

            switch (tb::probe(pos)) {
                case tb::ProbeResult::kFailed:
                    return {};
                case tb::ProbeResult::kWin:
                    return pos.stm() == Colors::kBlack ? Outcome::kWhiteLoss : Outcome::kWhiteWin;
                case tb::ProbeResult::kDraw:
                    return Outcome::kDraw;
                case tb::ProbeResult::kLoss:
                    return pos.stm() == Colors::kBlack ? Outcome::kWhiteWin : Outcome::kWhiteLoss;
            }
        }

        constexpr usize kVerificationHardNodeLimit = 25165814;

        constexpr usize kDatagenSoftNodeLimit = 24000;
        constexpr usize kDatagenHardNodeLimit = 8388608;

        constexpr Score kVerificationScoreLimit = 500;

        constexpr Score kWinAdjMinScore = 1250;
        constexpr Score kDrawAdjMaxScore = 10;

        constexpr u32 kDrawAdjMinPlies = 70;

        constexpr u32 kWinAdjPlyCount = 5;
        constexpr u32 kDrawAdjPlyCount = 10;

        constexpr i32 kReportInterval = 512;

        template <OutputFormat Format>
        void runThread(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir) {
            numa::bindThread(id);

            const auto outFile = outDir / fmt::format("{}.{}", id, Format::kExtension);
            std::ofstream out{outFile, std::ios::binary | std::ios::app};

            if (!out) {
                eprintln("failed to open output file {}", outFile);
                return;
            }

            util::rng::Jsf64Rng rng{seed};

            const auto createLimiter = [](usize hardNodes, usize softNodes = std::numeric_limits<usize>::max()) {
                limit::SearchLimiter limiter{Instant::now()};
                limiter.setHardNodes(hardNodes);
                limiter.setSoftNodes(softNodes);
                return limiter;
            };

            const auto verifLimiter = createLimiter(kVerificationHardNodeLimit);
            const auto datagenLimiter = createLimiter(kDatagenHardNodeLimit, kDatagenSoftNodeLimit);

            search::Searcher searcher{};
            searcher.setSilent(true);

            auto& thread = searcher.take(id);
            thread.datagen = true;

            auto& pos = thread.rootPos;

            const auto resetSearch = [&searcher, &thread]() {
                searcher.newGame();

                thread.search = search::SearchData{};
                thread.keyHistory.clear();
            };

            Format output{};

            const auto startTime = Instant::now();

            usize totalPositions{};

            for (i32 game = 0; !s_stop.load(std::memory_order::seq_cst); ++game) {
                resetSearch();

                if (dfrc) {
                    const auto dfrcIndex = rng.nextU32(960 * 960);
                    pos = *Position::fromDfrcIndex(dfrcIndex);
                } else {
                    pos = Position::starting();
                }

                const auto moveCount = 8 + (rng.nextU32() >> 31);

                bool legalFound = false;

                for (i32 i = 0; i < moveCount; ++i) {
                    ScoredMoveList moves{};
                    generateAll(moves, pos);

                    std::shuffle(moves.begin(), moves.end(), rng);

                    legalFound = false;

                    for (const auto [move, score] : moves) {
                        if (pos.isLegal(move)) {
                            thread.keyHistory.push_back(pos.key());
                            pos = pos.applyMove(move);

                            legalFound = true;
                            break;
                        }
                    }

                    if (!legalFound) {
                        break;
                    }
                }

                if (!legalFound) {
                    // this game was useless, don't count it
                    --game;
                    continue;
                }

                output.start(pos);

                thread.nnueState.reset(pos.boards(), pos.kings());

                searcher.setMaxDepth(10);
                searcher.setLimiter(verifLimiter);

                const auto [firstScore, normFirstScore] = searcher.runDatagenSearch();

                searcher.setMaxDepth(kMaxDepth);
                searcher.setLimiter(datagenLimiter);

                if (std::abs(normFirstScore) > kVerificationScoreLimit) {
                    --game;
                    continue;
                }

                resetSearch();

                u32 winPlies{};
                u32 lossPlies{};
                u32 drawPlies{};

                std::optional<Outcome> outcome{};

                while (true) {
                    const auto [score, normScore] = searcher.runDatagenSearch();
                    thread.search = search::SearchData{};

                    const auto move = thread.rootMoves[0].pv.moves[0];

                    if (!move) {
                        if (pos.isCheck()) {
                            outcome = pos.stm() == Colors::kBlack ? Outcome::kWhiteWin : Outcome::kWhiteLoss;
                        } else {
                            outcome = Outcome::kDraw; // stalemate
                        }

                        break;
                    }

                    assert(pos.boards().pieceOn(move.fromSq()) != Pieces::kNone);

                    if (std::abs(score) > kScoreWin) {
                        outcome = score > 0 ? Outcome::kWhiteWin : Outcome::kWhiteLoss;
                    } else {
                        if (normScore > kWinAdjMinScore) {
                            ++winPlies;
                            lossPlies = 0;
                            drawPlies = 0;
                        } else if (normScore < -kWinAdjMinScore) {
                            winPlies = 0;
                            ++lossPlies;
                            drawPlies = 0;
                        } else if (pos.plyFromStartpos() >= kDrawAdjMinPlies && std::abs(normScore) < kDrawAdjMaxScore)
                        {
                            winPlies = 0;
                            lossPlies = 0;
                            ++drawPlies;
                        } else {
                            winPlies = 0;
                            lossPlies = 0;
                            drawPlies = 0;
                        }

                        if (winPlies >= kWinAdjPlyCount) {
                            outcome = Outcome::kWhiteWin;
                        } else if (lossPlies >= kWinAdjPlyCount) {
                            outcome = Outcome::kWhiteLoss;
                        } else if (drawPlies >= kDrawAdjPlyCount) {
                            outcome = Outcome::kDraw;
                        }
                    }

                    const bool filtered = pos.isCheck() || pos.isNoisy(move);

                    eval::UpdateContext ctx{};
                    thread.keyHistory.push_back(pos.key());
                    pos = pos.applyMove(move, eval::BoardObserver{ctx});
                    thread.nnueState.applyImmediately(ctx, pos.boards());

                    assert(eval::staticEvalOnce(pos) == eval::staticEval(pos, thread.nnueState));

                    if (pos.isDrawn(0, thread.keyHistory)) {
                        outcome = Outcome::kDraw;
                        output.push(true, move, 0);
                        break;
                    }

                    if (const auto tbOutcome = probeTb(pos)) {
                        static constexpr std::array kScores = {
                            -kScoreTbWin,
                            0,
                            kScoreTbWin,
                        };

                        outcome = *tbOutcome;
                        output.push(true, move, kScores[static_cast<i32>(*tbOutcome)]);
                        break;
                    }

                    output.push(filtered, move, score);

                    if (outcome) {
                        break;
                    }
                }

                assert(outcome.has_value());

                const auto positions = output.writeAllWithOutcome(out, *outcome);
                totalPositions += positions;

                if (((game + 1) % kReportInterval) == 0 || s_stop.load(std::memory_order::seq_cst)) {
                    const auto time = startTime.elapsed();
                    println(
                        "thread {}: wrote {} positions from {} games in {} sec ({:.6g} positions/sec)",
                        id,
                        totalPositions,
                        game + 1,
                        time,
                        static_cast<f64>(totalPositions) / time
                    );
                }
            }
        }

        template void runThread<Marlinformat>(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir);
        template void runThread<Viriformat>(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir);
        template void runThread<Fen>(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir);
    } // namespace

    i32 run(
        const std::function<void()>& printUsage,
        std::string_view format,
        bool dfrc,
        std::string_view output,
        i32 threads,
        std::optional<std::string_view> tbPath
    ) {
        if (!eval::isNetworkLoaded()) {
            eprintln("No network loaded");
            return 1;
        }

        std::function<decltype(runThread<Marlinformat>)> threadFunc{};

        if (format == "marlinformat") {
            threadFunc = runThread<Marlinformat>;
        } else if (format == "viriformat") {
            threadFunc = runThread<Viriformat>;
        } else if (format == "fen") {
            threadFunc = runThread<Fen>;
        } else {
            eprintln("invalid output format {}", format);
            printUsage();
            return 1;
        }

        opts::mutableOpts().chess960 = dfrc;
        opts::mutableOpts().evalSharpness = 100;

        if (tbPath) {
            println("looking for TBs in \"{}\"", *tbPath);

            const auto status = tb::init(*tbPath);

            if (status != tb::InitStatus::kSuccess) {
                eprintln("No TBs found");
                return 2;
            }

            opts::mutableOpts().syzygyEnabled = true;
        }

        const auto baseSeed = util::rng::generateSingleSeed();
        println("base seed: {}", baseSeed);

        util::rng::SeedGenerator seedGenerator{baseSeed};

        const std::filesystem::path outDir{output};

        initCtrlCHandler();

        std::vector<std::thread> theThreads{};
        theThreads.reserve(threads);

        println("generating on {} threads", threads);

        for (u32 i = 0; i < threads; ++i) {
            const auto seed = seedGenerator.nextSeed();
            theThreads.emplace_back([&, i, seed]() { threadFunc(i, dfrc, seed, outDir); });
        }

        for (auto& thread : theThreads) {
            thread.join();
        }

        tb::free();

        println("done");

        return 0;
    }
} // namespace stormphrax::datagen
