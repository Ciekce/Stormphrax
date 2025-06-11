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

#include "../3rdparty/fathom/tbprobe.h"
#include "../limit/limit.h"
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
            util::signal::addCtrlCHandler([] { s_stop.store(true, std::memory_order::seq_cst); });
        }

        class DatagenNodeLimiter final : public limit::ISearchLimiter {
        public:
            explicit DatagenNodeLimiter(u32 threadId) :
                    m_threadId{threadId} {}
            ~DatagenNodeLimiter() final = default;

            [[nodiscard]] bool stop(const search::SearchData& data, bool allowSoftTimeout) final {
                if (data.nodes >= m_hardNodeLimit) {
                    std::cout << "thread " << m_threadId << ": stopping search after " << data.nodes
                              << " nodes (limit: " << m_hardNodeLimit << ")" << std::endl;
                    return true;
                }

                return allowSoftTimeout && data.nodes >= m_softNodeLimit;
            }

            [[nodiscard]] bool stopped() const final {
                // doesn't matter
                return false;
            }

            inline void setSoftNodeLimit(usize nodes) {
                m_softNodeLimit = nodes;
            }

            inline void setHardNodeLimit(usize nodes) {
                m_hardNodeLimit = nodes;
            }

        private:
            u32 m_threadId;
            usize m_softNodeLimit{};
            usize m_hardNodeLimit{};
        };

        [[nodiscard]] std::optional<Outcome> probeTb(const Position& pos) {
            if (pos.bbs().occupancy().popcount() > TB_LARGEST || pos.halfmove() != 0
                || pos.castlingRooks() != CastlingRooks{})
            {
                return {};
            }

            switch (tb::probe(pos)) {
                case tb::ProbeResult::Failed:
                    return {};
                case tb::ProbeResult::Win:
                    return pos.toMove() == Color::Black ? Outcome::WhiteLoss : Outcome::WhiteWin;
                case tb::ProbeResult::Draw:
                    return Outcome::Draw;
                case tb::ProbeResult::Loss:
                    return pos.toMove() == Color::Black ? Outcome::WhiteWin : Outcome::WhiteLoss;
            }
        }

        constexpr usize VerificationHardNodeLimit = 25165814;

        constexpr usize DatagenSoftNodeLimit = 24000;
        constexpr usize DatagenHardNodeLimit = 8388608;

        constexpr Score VerificationScoreLimit = 500;

        constexpr Score WinAdjMinScore = 1250;
        constexpr Score DrawAdjMaxScore = 10;

        constexpr u32 DrawAdjMinPlies = 70;

        constexpr u32 WinAdjPlyCount = 5;
        constexpr u32 DrawAdjPlyCount = 10;

        constexpr i32 ReportInterval = 512;

        template <OutputFormat Format>
        void runThread(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir) {
            const auto outFile = outDir / (std::to_string(id) + "." + Format::Extension);
            std::ofstream out{outFile, std::ios::binary | std::ios::app};

            if (!out) {
                std::cerr << "failed to open output file " << outFile << std::endl;
                return;
            }

            util::rng::Jsf64Rng rng{seed};

            auto limiterPtr = std::make_unique<DatagenNodeLimiter>(id);
            auto& limiter = *limiterPtr;

            search::Searcher searcher{};
            searcher.setLimiter(std::move(limiterPtr));

            auto thread = std::make_unique<search::ThreadData>();
            thread->datagen = true;

            auto& pos = thread->rootPos;

            const auto resetSearch = [&searcher, &thread]() {
                searcher.newGame();

                thread->search = search::SearchData{};

                thread->history.clear();
                thread->correctionHistory.clear();

                thread->keyHistory.clear();
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
                            thread->keyHistory.push_back(pos.key());
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

                thread->nnueState.reset(pos.bbs(), pos.kings());

                searcher.setDatagenMaxDepth(10);
                limiter.setSoftNodeLimit(std::numeric_limits<usize>::max());
                limiter.setHardNodeLimit(VerificationHardNodeLimit);

                const auto [firstScore, normFirstScore] = searcher.runDatagenSearch(*thread);

                searcher.setDatagenMaxDepth(MaxDepth);
                limiter.setSoftNodeLimit(DatagenSoftNodeLimit);
                limiter.setHardNodeLimit(DatagenHardNodeLimit);

                if (std::abs(normFirstScore) > VerificationScoreLimit) {
                    --game;
                    continue;
                }

                resetSearch();

                u32 winPlies{};
                u32 lossPlies{};
                u32 drawPlies{};

                std::optional<Outcome> outcome{};

                while (true) {
                    const auto [score, normScore] = searcher.runDatagenSearch(*thread);
                    thread->search = search::SearchData{};

                    const auto move = thread->rootPv.moves[0];

                    if (!move) {
                        if (pos.isCheck()) {
                            outcome = pos.toMove() == Color::Black ? Outcome::WhiteWin : Outcome::WhiteLoss;
                        } else {
                            outcome = Outcome::Draw; // stalemate
                        }

                        break;
                    }

                    assert(pos.boards().pieceAt(move.src()) != Piece::None);

                    if (std::abs(score) > ScoreWin) {
                        outcome = score > 0 ? Outcome::WhiteWin : Outcome::WhiteLoss;
                    } else {
                        if (normScore > WinAdjMinScore) {
                            ++winPlies;
                            lossPlies = 0;
                            drawPlies = 0;
                        } else if (normScore < -WinAdjMinScore) {
                            winPlies = 0;
                            ++lossPlies;
                            drawPlies = 0;
                        } else if (thread->rootPos.plyFromStartpos() >= DrawAdjMinPlies
                                   && std::abs(normScore) < DrawAdjMaxScore)
                        {
                            winPlies = 0;
                            lossPlies = 0;
                            ++drawPlies;
                        } else {
                            winPlies = 0;
                            lossPlies = 0;
                            drawPlies = 0;
                        }

                        if (winPlies >= WinAdjPlyCount) {
                            outcome = Outcome::WhiteWin;
                        } else if (lossPlies >= WinAdjPlyCount) {
                            outcome = Outcome::WhiteLoss;
                        } else if (drawPlies >= DrawAdjPlyCount) {
                            outcome = Outcome::Draw;
                        }
                    }

                    const bool filtered = pos.isCheck() || pos.isNoisy(move);

                    thread->keyHistory.push_back(pos.key());
                    pos = pos.applyMove<NnueUpdateAction::Apply>(move, &thread->nnueState);

                    assert(eval::staticEvalOnce(pos) == eval::staticEval(pos, thread->nnueState));

                    if (pos.isDrawn(0, thread->keyHistory)) {
                        outcome = Outcome::Draw;
                        output.push(true, move, 0);
                        break;
                    }

                    if (const auto tbOutcome = probeTb(pos)) {
                        static constexpr auto Scores = std::array{
                            -ScoreTbWin,
                            0,
                            ScoreTbWin,
                        };

                        outcome = *tbOutcome;
                        output.push(true, move, Scores[static_cast<i32>(*tbOutcome)]);
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

                if (((game + 1) % ReportInterval) == 0 || s_stop.load(std::memory_order::seq_cst)) {
                    const auto time = startTime.elapsed();
                    std::cout << "thread " << id << ": wrote " << totalPositions << " positions from " << (game + 1)
                              << " games in " << time << " sec (" << (static_cast<f64>(totalPositions) / time)
                              << " positions/sec)" << std::endl;
                }
            }
        }

        template void runThread<Marlinformat>(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir);
        template void runThread<Viriformat>(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir);
        template void runThread<Fen>(u32 id, bool dfrc, u64 seed, const std::filesystem::path& outDir);
    } // namespace

    i32 run(
        const std::function<void()>& printUsage,
        const std::string& format,
        bool dfrc,
        const std::string& output,
        i32 threads,
        std::optional<std::string> tbPath
    ) {
        std::function<decltype(runThread<Marlinformat>)> threadFunc{};

        if (format == "marlinformat") {
            threadFunc = runThread<Marlinformat>;
        } else if (format == "viriformat") {
            threadFunc = runThread<Viriformat>;
        } else if (format == "fen") {
            threadFunc = runThread<Fen>;
        } else {
            std::cerr << "invalid output format " << format << std::endl;
            printUsage();
            return 1;
        }

        opts::mutableOpts().chess960 = dfrc;

        if (tbPath) {
            std::cout << "looking for TBs in \"" << *tbPath << "\"" << std::endl;

            if (!tb_init(tbPath->c_str())) {
                std::cerr << "Failed to initialize Fathom" << std::endl;
                return 2;
            }

            if (TB_LARGEST > 0) {
                std::cout << "Found up to " << TB_LARGEST << "-man TBs" << std::endl;
                opts::mutableOpts().syzygyEnabled = true;
            } else {
                std::cerr << "No TBs found" << std::endl;
                return 2;
            }
        }

        const auto baseSeed = util::rng::generateSingleSeed();
        std::cout << "base seed: " << baseSeed << std::endl;

        util::rng::SeedGenerator seedGenerator{baseSeed};

        const std::filesystem::path outDir{output};

        initCtrlCHandler();

        std::vector<std::thread> theThreads{};
        theThreads.reserve(threads);

        std::cout << "generating on " << threads << " threads" << std::endl;

        for (u32 i = 0; i < threads; ++i) {
            const auto seed = seedGenerator.nextSeed();
            theThreads.emplace_back([&, i, seed]() { threadFunc(i, dfrc, seed, outDir); });
        }

        for (auto& thread : theThreads) {
            thread.join();
        }

        if (tbPath) {
            tb_free();
        }

        std::cout << "done" << std::endl;

        return 0;
    }
} // namespace stormphrax::datagen
