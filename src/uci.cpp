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

#include "uci.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "3rdparty/fathom/tbprobe.h"
#include "bench.h"
#include "eval/eval.h"
#include "limit/compound.h"
#include "limit/time.h"
#include "limit/trivial.h"
#include "movegen.h"
#include "opts.h"
#include "perft.h"
#include "position/position.h"
#include "search.h"
#include "ttable.h"
#include "tunable.h"
#include "util/parse.h"
#include "util/split.h"
#include "util/timer.h"
#include "wdl.h"

namespace stormphrax {
    using namespace uci;

    using util::Instant;

    namespace {
        constexpr auto kName = "Stormphrax";
        constexpr auto kVersion = SP_STRINGIFY(SP_VERSION);
        constexpr auto kAuthor = "Ciekce";

#if SP_EXTERNAL_TUNE
        std::vector<tunable::TunableParam>& tunableParams() {
            static auto params = [] {
                std::vector<tunable::TunableParam> params{};
                params.reserve(128);
                return params;
            }();

            return params;
        }

        inline tunable::TunableParam* lookupTunableParam(const std::string& name) {
            for (auto& param : tunableParams()) {
                if (param.lowerName == name) {
                    return &param;
                }
            }

            return nullptr;
        }
#endif

        class UciHandler {
        public:
            UciHandler() = default;
            ~UciHandler();

            i32 run();

        private:
            void handleUci();
            void handleUcinewgame();
            void handleIsready();
            void handlePosition(const std::vector<std::string>& tokens);
            void handleGo(const std::vector<std::string>& tokens, Instant startTime);
            void handleStop();
            void handleSetoption(const std::vector<std::string>& tokens);
            // V ======= NONSTANDARD ======= V
            void handleD();
            void handleFen();
            void handleCheckers();
            void handleThreats();
            void handleEval();
            void handleRawEval();
            void handleRegen();
            void handleMoves();
            void handlePerft(const std::vector<std::string>& tokens);
            void handleSplitperft(const std::vector<std::string>& tokens);
            void handleBench(const std::vector<std::string>& tokens);

            bool m_fathomInitialized{false};

            search::Searcher m_searcher{};

            std::vector<u64> m_keyHistory{};
            Position m_pos{Position::starting()};

            i32 m_moveOverhead{limit::kDefaultMoveOverhead};
        };

        UciHandler::~UciHandler() {
            // can't do this in a destructor, because it will run after tb_free is called
            m_searcher.quit();

            if (m_fathomInitialized) {
                tb_free();
            }
        }

        i32 UciHandler::run() {
            for (std::string line{}; std::getline(std::cin, line);) {
                const auto startTime = Instant::now();

                const auto tokens = split::split(line, ' ');

                if (tokens.empty()) {
                    continue;
                }

                const auto& command = tokens[0];

                if (command == "quit") {
                    return 0;
                } else if (command == "uci") {
                    handleUci();
                } else if (command == "ucinewgame") {
                    handleUcinewgame();
                } else if (command == "isready") {
                    handleIsready();
                } else if (command == "position") {
                    handlePosition(tokens);
                } else if (command == "go") {
                    handleGo(tokens, startTime);
                } else if (command == "stop") {
                    handleStop();
                } else if (command == "setoption") {
                    handleSetoption(tokens);
                    // V ======= NONSTANDARD ======= V
                } else if (command == "d") {
                    handleD();
                } else if (command == "fen") {
                    handleFen();
                } else if (command == "eval") {
                    handleEval();
                } else if (command == "raweval") {
                    handleRawEval();
                } else if (command == "checkers") {
                    handleCheckers();
                } else if (command == "threats") {
                    handleThreats();
                } else if (command == "regen") {
                    handleRegen();
                } else if (command == "moves") {
                    handleMoves();
                } else if (command == "perft") {
                    handlePerft(tokens);
                } else if (command == "splitperft") {
                    handleSplitperft(tokens);
                } else if (command == "bench") {
                    handleBench(tokens);
                }
            }

            return 0;
        }

        void UciHandler::handleUci() {
            static const opts::GlobalOptions defaultOpts{};

#ifdef SP_COMMIT_HASH
            println("id name {} {} {}", kName, kVersion, SP_STRINGIFY(SP_COMMIT_HASH));
#else
            println("id name {} {}", kName, kVersion);
#endif
            println("id author {}", kAuthor);

            println(
                "option name Hash type spin default {} min {} max {}",
                kDefaultTtSizeMib,
                kTtSizeMibRange.min(),
                kTtSizeMibRange.max()
            );
            println("option name Clear Hash type button");
            println(
                "option name Threads type spin default {} min {} max {}",
                opts::kDefaultThreadCount,
                opts::kThreadCountRange.min(),
                opts::kThreadCountRange.max()
            );
            println(
                "option name Contempt type spin default {} min {} max {}",
                opts::kDefaultNormalizedContempt,
                ContemptRange.min(),
                ContemptRange.max()
            );
            println("option name UCI_Chess960 type check default {}", defaultOpts.chess960);
            println("option name UCI_ShowWDL type check default {}", defaultOpts.showWdl);
            println("option name ShowCurrMove type check default {}", defaultOpts.showCurrMove);
            println(
                "option name Move Overhead type spin default {} min {} max {}",
                limit::kDefaultMoveOverhead,
                limit::kMoveOverheadRange.min(),
                limit::kMoveOverheadRange.max()
            );
            println("option name SoftNodes type check default {}", defaultOpts.softNodes);
            println(
                "option name SoftNodeHardLimitMultiplier type spin default {} min {} max {}",
                defaultOpts.softNodeHardLimitMultiplier,
                limit::SoftNodeHardLimitMultiplierRange.min(),
                limit::SoftNodeHardLimitMultiplierRange.max()
            );
            println("option name EnableWeirdTCs type check default {}", defaultOpts.enableWeirdTcs);
            println("option name SyzygyPath type string default <empty>");
            println(
                "option name SyzygyProbeDepth type spin default {} min {} max {}",
                defaultOpts.syzygyProbeDepth,
                search::kSyzygyProbeDepthRange.min(),
                search::kSyzygyProbeDepthRange.max()
            );
            println(
                "option name SyzygyProbeLimit type spin default {} min {} max {}",
                defaultOpts.syzygyProbeLimit,
                search::kSyzygyProbeLimitRange.min(),
                search::kSyzygyProbeLimitRange.max()
            );
            println("option name EvalFile type string default <internal>");

#if SP_EXTERNAL_TUNE
            for (const auto& param : tunableParams()) {
                println(
                    "option name {} type spin default {} min {} max {}",
                    param.name,
                    param.defaultValue,
                    param.range.min(),
                    param.range.max()
                );
            }
#endif

            println("uciok");
        }

        void UciHandler::handleUcinewgame() {
            if (m_searcher.searching()) {
                eprintln("still searching");
            } else {
                m_searcher.newGame();
            }
        }

        void UciHandler::handleIsready() {
            m_searcher.ensureReady();
            println("readyok");
        }

        void UciHandler::handlePosition(const std::vector<std::string>& tokens) {
            if (m_searcher.searching()) {
                eprintln("still searching");
            } else if (tokens.size() > 1) {
                const auto& position = tokens[1];

                usize next = 2;

                if (position == "startpos") {
                    m_pos = Position::starting();
                    m_keyHistory.clear();
                } else if (position == "fen") {
                    std::string fen{};
                    auto itr = std::back_inserter(fen);

                    for (usize i = 0; i < 6 && next < tokens.size(); ++i, ++next) {
                        fmt::format_to(itr, "{} ", tokens[next]);
                    }

                    if (const auto newPos = Position::fromFen(fen)) {
                        m_pos = *newPos;
                        m_keyHistory.clear();
                    } else {
                        return;
                    }
                } else if (position == "frc") {
                    if (!g_opts.chess960) {
                        eprintln("Chess960 not enabled");
                        return;
                    }

                    if (next < tokens.size()) {
                        if (const auto frcIndex = util::tryParseU32(tokens[next++])) {
                            if (const auto newPos = Position::fromFrcIndex(*frcIndex)) {
                                m_pos = *newPos;
                                m_keyHistory.clear();
                            } else {
                                return;
                            }
                        } else {
                            return;
                        }
                    }
                } else if (position == "dfrc") {
                    if (!g_opts.chess960) {
                        eprintln("Chess960 not enabled");
                        return;
                    }

                    if (next < tokens.size()) {
                        if (const auto dfrcIndex = util::tryParseU32(tokens[next++])) {
                            if (const auto newPos = Position::fromDfrcIndex(*dfrcIndex)) {
                                m_pos = *newPos;
                                m_keyHistory.clear();
                            } else {
                                return;
                            }
                        } else {
                            return;
                        }
                    }
                } else {
                    return;
                }

                if (next < tokens.size() && tokens[next++] == "moves") {
                    for (; next < tokens.size(); ++next) {
                        if (const auto move = m_pos.moveFromUci(tokens[next])) {
                            m_keyHistory.push_back(m_pos.key());
                            m_pos = m_pos.applyMove(move);
                        }
                    }
                }
            }
        }

        void UciHandler::handleGo(const std::vector<std::string>& tokens, Instant startTime) {
            if (m_searcher.searching()) {
                eprintln("already searching");
            } else {
                u32 depth = kMaxDepth;
                auto limiter = std::make_unique<limit::CompoundLimiter>();

                MoveList movesToSearch{};

                bool infinite = false;
                bool tournamentTime = false;

                i64 timeRemaining{};
                i64 increment{};
                i32 toGo{};

                for (usize i = 1; i < tokens.size(); ++i) {
                    if (tokens[i] == "depth" && ++i < tokens.size()) {
                        if (!util::tryParseU32(depth, tokens[i])) {
                            eprintln("invalid depth {}", tokens[i]);
                        }
                        continue;
                    }

                    if (tokens[i] == "infinite") {
                        infinite = true;
                        continue;
                    }

                    if (tokens[i] == "nodes" && ++i < tokens.size()) {
                        usize nodes{};
                        if (!util::tryParseSize(nodes, tokens[i])) {
                            eprintln("invalid node count {}", tokens[i]);
                        } else {
                            limiter->addLimiter<limit::NodeLimiter>(nodes);
                        }
                    } else if (tokens[i] == "movetime" && ++i < tokens.size()) {
                        i64 time{};
                        if (!util::tryParseI64(time, tokens[i])) {
                            eprintln("invalid time {}", tokens[i]);
                        } else {
                            time = std::max<i64>(time, 1);
                            limiter->addLimiter<limit::MoveTimeLimiter>(time, m_moveOverhead);
                        }
                    } else if ((tokens[i] == "btime" || tokens[i] == "wtime") && ++i < tokens.size()
                               && tokens[i - 1] == (m_pos.toMove() == Color::kBlack ? "btime" : "wtime"))
                    {
                        tournamentTime = true;

                        i64 time{};
                        if (!util::tryParseI64(time, tokens[i])) {
                            eprintln("invalid time {}", tokens[i]);
                        } else {
                            time = std::max<i64>(time, 1);
                            timeRemaining = static_cast<i64>(time);
                        }
                    } else if ((tokens[i] == "binc" || tokens[i] == "winc") && ++i < tokens.size()
                               && tokens[i - 1] == (m_pos.toMove() == Color::kBlack ? "binc" : "winc"))
                    {
                        tournamentTime = true;

                        i64 time{};
                        if (!util::tryParseI64(time, tokens[i])) {
                            eprintln("invalid time {}", tokens[i]);
                        } else {
                            time = std::max<i64>(time, 1);
                            increment = static_cast<i64>(time);
                        }
                    } else if (tokens[i] == "movestogo" && ++i < tokens.size()) {
                        tournamentTime = true;

                        u32 moves{};
                        if (!util::tryParseU32(moves, tokens[i])) {
                            eprintln("invalid movestogo {}", tokens[i]);
                        } else {
                            moves = std::min<u32>(moves, static_cast<u32>(std::numeric_limits<i32>::max()));
                            toGo = static_cast<i32>(moves);
                        }
                    } else if (tokens[i] == "searchmoves" && i + 1 < tokens.size()) {
                        while (i + 1 < tokens.size()) {
                            const auto& candidate = tokens[i + 1];

                            if (candidate.length() >= 4 && candidate.length() <= 5 && candidate[0] >= 'a'
                                && candidate[0] <= 'h' && candidate[1] >= '1' && candidate[1] <= '8'
                                && candidate[2] >= 'a' && candidate[2] <= 'h' && candidate[3] >= '1'
                                && candidate[3] <= '8'
                                && (candidate.length() < 5 || isValidPromotion(pieceTypeFromChar(candidate[4]))))
                            {
                                const auto move = m_pos.moveFromUci(candidate);

                                if (std::ranges::find(movesToSearch, move) == movesToSearch.end()) {
                                    if (m_pos.isPseudolegal(move) && m_pos.isLegal(move)) {
                                        movesToSearch.push(move);
                                    } else {
                                        println("info string ignoring illegal move {}", candidate);
                                    }
                                }

                                ++i;
                            } else {
                                break;
                            }
                        }
                    }
                }

                if (!movesToSearch.empty()) {
                    print("info string searching moves:");

                    for (const auto move : movesToSearch) {
                        println(" {}", move);
                    }

                    println();
                }

                if (depth == 0) {
                    return;
                } else if (depth > kMaxDepth) {
                    depth = kMaxDepth;
                }

                if (tournamentTime) {
                    if (toGo != 0) {
                        if (g_opts.enableWeirdTcs) {
                            println(
                                "info string Warning: Stormphrax does not officially support cyclic (movestogo) time controls"
                            );
                        } else {
                            println(
                                "info string Cyclic (movestogo) time controls not enabled, see the EnableWeirdTCs option"
                            );
                            println("bestmove 0000");
                            return;
                        }
                    } else if (increment == 0) {
                        if (g_opts.enableWeirdTcs) {
                            println(
                                "info string Warning: Stormphrax does not officially support sudden death (0 increment) time controls"
                            );
                        } else {
                            println(
                                "info string Sudden death (0 increment) time controls not enabled, see the EnableWeirdTCs option"
                            );
                            println("bestmove 0000");
                            return;
                        }
                    }
                }

                if (tournamentTime && timeRemaining > 0) {
                    limiter->addLimiter<limit::TimeManager>(
                        startTime,
                        static_cast<f64>(timeRemaining) / 1000.0,
                        static_cast<f64>(increment) / 1000.0,
                        toGo,
                        static_cast<f64>(m_moveOverhead) / 1000.0
                    );
                }

                m_searcher.startSearch(
                    m_pos,
                    m_keyHistory,
                    startTime,
                    static_cast<i32>(depth),
                    movesToSearch,
                    std::move(limiter),
                    infinite
                );
            }
        }

        void UciHandler::handleStop() {
            if (!m_searcher.searching()) {
                eprintln("not searching");
            } else {
                m_searcher.stop();
            }
        }

        //TODO refactor
        void UciHandler::handleSetoption(const std::vector<std::string>& tokens) {
            usize i = 1;

            for (; i < tokens.size() - 1 && tokens[i] != "name"; ++i) {
                //
            }

            if (++i == tokens.size()) {
                return;
            }

            std::string name{};
            auto nameItr = std::back_inserter(name);

            for (; i < tokens.size() && tokens[i] != "value"; ++i) {
                if (!name.empty()) {
                    fmt::format_to(nameItr, " ");
                }

                fmt::format_to(nameItr, "{}", tokens[i]);
            }

            if (++i == tokens.size()) {
                return;
            }

            std::string value{};
            auto valueItr = std::back_inserter(value);

            for (; i < tokens.size(); ++i) {
                if (!value.empty()) {
                    fmt::format_to(valueItr, " ");
                }

                fmt::format_to(valueItr, "{}", tokens[i]);
            }

            if (!name.empty()) {
                std::transform(name.begin(), name.end(), name.begin(), [](auto c) { return std::tolower(c); });

                if (name == "hash") {
                    if (!value.empty()) {
                        if (const auto newTtSize = util::tryParseSize(value)) {
                            m_searcher.setTtSize(kTtSizeMibRange.clamp(*newTtSize));
                        }
                    }
                } else if (name == "clear hash") {
                    if (m_searcher.searching()) {
                        eprintln("still searching");
                    }

                    m_searcher.newGame();
                } else if (name == "threads") {
                    if (m_searcher.searching()) {
                        eprintln("still searching");
                    }

                    if (!value.empty()) {
                        if (const auto newThreads = util::tryParseU32(value)) {
                            opts::mutableOpts().threads = *newThreads;
                            m_searcher.setThreads(opts::kThreadCountRange.clamp(*newThreads));
                        }
                    }
                } else if (name == "contempt") {
                    if (!value.empty()) {
                        if (const auto newContempt = util::tryParseI32(value)) {
                            opts::mutableOpts().contempt =
                                wdl::unnormalizeScoreMaterial58(ContemptRange.clamp(*newContempt));
                        }
                    }
                } else if (name == "uci_chess960") {
                    if (!value.empty()) {
                        if (const auto newChess960 = util::tryParseBool(value)) {
                            opts::mutableOpts().chess960 = *newChess960;
                        }
                    }
                } else if (name == "uci_showwdl") {
                    if (!value.empty()) {
                        if (const auto newShowWdl = util::tryParseBool(value)) {
                            opts::mutableOpts().showWdl = *newShowWdl;
                        }
                    }
                } else if (name == "showcurrmove") {
                    if (!value.empty()) {
                        if (const auto newShowCurrMove = util::tryParseBool(value)) {
                            opts::mutableOpts().showCurrMove = *newShowCurrMove;
                        }
                    }
                } else if (name == "move overhead") {
                    if (!value.empty()) {
                        if (const auto newMoveOverhead = util::tryParseI32(value)) {
                            m_moveOverhead = limit::kMoveOverheadRange.clamp(*newMoveOverhead);
                        }
                    }
                } else if (name == "softnodes") {
                    if (!value.empty()) {
                        if (const auto newSoftNodes = util::tryParseBool(value)) {
                            opts::mutableOpts().softNodes = *newSoftNodes;
                        }
                    }
                } else if (name == "softnodehardlimitmultiplier") {
                    if (!value.empty()) {
                        if (const auto newSoftNodeHardLimitMultiplier = util::tryParseI32(value)) {
                            opts::mutableOpts().softNodeHardLimitMultiplier =
                                limit::SoftNodeHardLimitMultiplierRange.clamp(*newSoftNodeHardLimitMultiplier);
                        }
                    }
                } else if (name == "enableweirdtcs") {
                    if (!value.empty()) {
                        if (const auto newEnableWeirdTcs = util::tryParseBool(value)) {
                            opts::mutableOpts().enableWeirdTcs = *newEnableWeirdTcs;
                        }
                    }
                } else if (name == "syzygypath") {
                    if (m_searcher.searching()) {
                        eprintln("still searching");
                    }

                    m_fathomInitialized = true;

                    if (value.empty()) {
                        opts::mutableOpts().syzygyEnabled = false;
                        tb_init("");
                    } else {
                        opts::mutableOpts().syzygyEnabled = value != "<empty>";
                        if (!tb_init(value.c_str())) {
                            eprintln("failed to initialize Fathom");
                        }
                    }
                } else if (name == "syzygyprobedepth") {
                    if (!value.empty()) {
                        if (const auto newSyzygyProbeDepth = util::tryParseI32(value)) {
                            opts::mutableOpts().syzygyProbeDepth =
                                search::kSyzygyProbeLimitRange.clamp(*newSyzygyProbeDepth);
                        }
                    }
                } else if (name == "syzygyprobelimit") {
                    if (!value.empty()) {
                        if (const auto newSyzygyProbeLimit = util::tryParseI32(value)) {
                            opts::mutableOpts().syzygyProbeLimit =
                                search::kSyzygyProbeLimitRange.clamp(*newSyzygyProbeLimit);
                        }
                    }
                } else if (name == "evalfile") {
                    if (m_searcher.searching()) {
                        eprintln("still searching");
                    }

                    if (!value.empty()) {
                        if (value == "<internal>") {
                            eval::loadDefaultNetwork();
                            println("info string loaded embedded network {}", eval::defaultNetworkName());
                        } else {
                            eval::loadNetwork(value);
                        }
                    }
                }
#if SP_EXTERNAL_TUNE
                else if (auto* param = lookupTunableParam(name))
                {
                    if (!value.empty() && util::tryParseI32(param->value, value) && param->callback) {
                        param->callback();
                    }
                }
#endif
            }
        }

        void UciHandler::handleD() {
            println();
            println("{}", m_pos);

            println();
            println("Fen: {}", m_pos.toFen());

            println("Key: {:016x}", m_pos.key());
            println("Pawn key: {:016x}", m_pos.pawnKey());

            print("Checkers:");

            auto checkers = m_pos.checkers();
            while (checkers) {
                print(" {}", checkers.popLowestSquare());
            }

            println();

            print("Pinned:");

            auto pinned = m_pos.pinned(m_pos.toMove());
            while (pinned) {
                print(" {}", pinned.popLowestSquare());
            }

            println();

            const auto staticEval = eval::adjustEval<false>(m_pos, {}, 0, nullptr, eval::staticEvalOnce(m_pos));
            const auto normalized = wdl::normalizeScore(staticEval, m_pos.classicalMaterial());
            const auto whitePerspective = m_pos.toMove() == Color::kBlack ? -normalized : normalized;

            println("Static eval: {:+}.{:02}", whitePerspective / 100, std::abs(whitePerspective) % 100);
        }

        void UciHandler::handleFen() {
            println("{}", m_pos.toFen());
        }

        void UciHandler::handleEval() {
            const auto staticEval = eval::adjustEval<false>(m_pos, {}, 0, nullptr, eval::staticEvalOnce(m_pos));
            const auto normalized = wdl::normalizeScore(staticEval, m_pos.classicalMaterial());

            println("Static eval: {:+}.{:02}", normalized / 100, std::abs(normalized) % 100);
        }

        void UciHandler::handleRawEval() {
            const auto score = eval::staticEvalOnce<false>(m_pos);
            println("{}", score);
        }

        void UciHandler::handleCheckers() {
            println("{}", m_pos.checkers());
        }

        void UciHandler::handleThreats() {
            println("{}", m_pos.threats());
        }

        void UciHandler::handleRegen() {
            m_pos.regen();
        }

        void UciHandler::handleMoves() {
            ScoredMoveList moves{};
            generateAll(moves, m_pos);

            for (u32 i = 0; i < moves.size(); ++i) {
                if (i > 0) {
                    print(" {}", moves[i].move);
                } else {
                    print("{}", moves[i].move);
                }
            }

            println();
        }

        void UciHandler::handlePerft(const std::vector<std::string>& tokens) {
            u32 depth = 6;

            if (tokens.size() > 1) {
                if (!util::tryParseU32(depth, tokens[1])) {
                    eprintln("invalid depth {}", tokens[1]);
                    return;
                }
            }

            perft(m_pos, static_cast<i32>(depth));
        }

        void UciHandler::handleSplitperft(const std::vector<std::string>& tokens) {
            u32 depth = 6;

            if (tokens.size() > 1) {
                if (!util::tryParseU32(depth, tokens[1])) {
                    eprintln("invalid depth {}", tokens[1]);
                    return;
                }
            }

            splitPerft(m_pos, static_cast<i32>(depth));
        }

        void UciHandler::handleBench(const std::vector<std::string>& tokens) {
            if (m_searcher.searching()) {
                eprintln("already searching");
                return;
            }

            i32 depth = bench::kDefaultBenchDepth;
            usize ttSize = bench::kDefaultBenchTtSize;

            if (tokens.size() > 1) {
                if (const auto newDepth = util::tryParseU32(tokens[1])) {
                    depth = static_cast<i32>(*newDepth);
                } else {
                    println("info string invalid depth {}", tokens[1]);
                    return;
                }
            }

            if (tokens.size() > 2) {
                if (const auto newThreads = util::tryParseU32(tokens[2])) {
                    if (*newThreads > 1) {
                        println("info string multiple search threads not yet supported, using 1");
                    }
                } else {
                    println("info string invalid thread count {}", tokens[2]);
                    return;
                }
            }

            if (tokens.size() > 3) {
                if (const auto newTtSize = util::tryParseSize(tokens[3])) {
                    ttSize = static_cast<i32>(*newTtSize);
                } else {
                    println("info string invalid tt size {}", tokens[3]);
                    return;
                }
            }

            m_searcher.setTtSize(ttSize);
            println("info string set tt size to {} MiB", ttSize);

            if (depth == 0) {
                depth = 1;
            }

            bench::run(m_searcher, depth);
        }
    } // namespace

#if SP_EXTERNAL_TUNE
    namespace tunable {
        TunableParam& addTunableParam(
            const std::string& name,
            i32 value,
            i32 min,
            i32 max,
            f64 step,
            std::function<void()> callback
        ) {
            auto& params = tunableParams();

            if (params.size() == params.capacity()) {
                eprintln("Tunable vector full, cannot reallocate");
                std::terminate();
            }

            auto lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](auto c) {
                return std::tolower(c);
            });

            return params.emplace_back(
                TunableParam{name, std::move(lowerName), value, value, {min, max}, step, std::move(callback)}
            );
        }
    } // namespace tunable
#endif

    namespace uci {
        i32 run() {
            UciHandler handler{};
            return handler.run();
        }

#if SP_EXTERNAL_TUNE
        namespace {
            void printParams(
                std::span<const std::string> params,
                const std::function<void(const tunable::TunableParam&)>& printParam
            ) {
                if (std::ranges::find(params, "<all>") != params.end()) {
                    for (const auto& param : tunableParams()) {
                        printParam(param);
                    }

                    return;
                }

                for (auto paramName : params) {
                    std::transform(paramName.begin(), paramName.end(), paramName.begin(), [](auto c) {
                        return std::tolower(c);
                    });

                    if (const auto* param = lookupTunableParam(paramName)) {
                        printParam(*param);
                    } else {
                        eprintln("unknown parameter {}", paramName);
                        return;
                    }
                }
            }
        } // namespace

        void printWfTuningParams(std::span<const std::string> params) {
            println("{{");

            bool first = true;
            const auto printParam = [&first](const auto& param) {
                if (!first) {
                    println(",");
                }

                println("  \"{}\": {{", param.name);
                println("    \"value\": {},", param.value);
                println("    \"min_value\": {},", param.range.min());
                println("    \"max_value\": {},", param.range.max());
                println("    \"step\": {}", param.step);
                println("  }}");

                first = false;
            };

            printParams(params, printParam);

            println();
            println("}}");
        }

        void printCttTuningParams(std::span<const std::string> params) {
            bool first = true;
            const auto printParam = [&first](const auto& param) {
                if (!first) {
                    println(",");
                }

                println("\"{}\": \"Integer({}, {})\"", param.name, param.range.min(), param.range.max());

                first = false;
            };

            printParams(params, printParam);

            println();
        }

        void printObTuningParams(std::span<const std::string> params) {
            const auto printParam = [](const auto& param) {
                println(
                    "{}, int, {}.0, {}.0, {}.0, {}, 0.002",
                    param.name,
                    param.value,
                    param.range.min(),
                    param.range.max(),
                    param.step
                );
            };

            printParams(params, printParam);
        }
#endif // SP_EXTERNAL_TUNE
    } // namespace uci
} // namespace stormphrax
