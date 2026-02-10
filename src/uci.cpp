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
#include <cctype>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "3rdparty/pyrrhic/tbprobe.h"
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
#include "tb.h"
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

        inline tunable::TunableParam* lookupTunableParam(std::string_view name) {
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
            ~UciHandler();

            i32 run();

        private:
            void handleUci();
            void handleUcinewgame();
            void handleIsready();
            void handlePosition(std::span<const std::string_view> args);
            void handleGo(std::span<const std::string_view> args, Instant startTime);
            void handleStop();
            void handleSetoption(std::span<const std::string_view> args);
            // V ======= NONSTANDARD ======= V
            void handleD();
            void handleFen();
            void handleCheckers();
            void handleThreats();
            void handleEval();
            void handleRawEval();
            void handleRegen();
            void handleMoves();
            void handlePerft(std::span<const std::string_view> args);
            void handleSplitperft(std::span<const std::string_view> args);
            void handleBench(std::span<const std::string_view> args);
            void handleProbeWdl();
            void handleWait();

            bool m_tbInitialized{false};

            search::Searcher m_searcher{};

            std::vector<u64> m_keyHistory{};
            Position m_pos{Position::starting()};

            i32 m_moveOverhead{limit::kDefaultMoveOverhead};
        };

        UciHandler::~UciHandler() {
            // can't do this in a destructor, because it will run after tb::free() is called
            m_searcher.quit();
            tb::free();
        }

        i32 UciHandler::run() {
            std::vector<std::string_view> tokens{};

            for (std::string line{}; std::getline(std::cin, line);) {
                const auto startTime = Instant::now();

                tokens.clear();
                split::split(tokens, line, ' ');

                if (tokens.empty()) {
                    continue;
                }

                const auto command = tokens[0];
                const auto args = std::span{tokens}.subspan<1>();

                if (command == "quit") {
                    return 0;
                } else if (command == "uci") {
                    handleUci();
                } else if (command == "ucinewgame") {
                    handleUcinewgame();
                } else if (command == "isready") {
                    handleIsready();
                } else if (command == "position") {
                    handlePosition(args);
                } else if (command == "go") {
                    handleGo(args, startTime);
                } else if (command == "stop") {
                    handleStop();
                } else if (command == "setoption") {
                    handleSetoption(args);
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
                    handlePerft(args);
                } else if (command == "splitperft") {
                    handleSplitperft(args);
                } else if (command == "bench") {
                    handleBench(args);
                } else if (command == "probewdl") {
                    handleProbeWdl();
                } else if (command == "wait") {
                    handleWait();
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
                "option name MultiPV type spin default {} min {} max {}",
                defaultOpts.multiPv,
                opts::kMultiPvRange.min(),
                opts::kMultiPvRange.max()
            );
            println(
                "option name Contempt type spin default {} min {} max {}",
                opts::kDefaultNormalizedContempt,
                kContemptRange.min(),
                kContemptRange.max()
            );
            println("option name UCI_Chess960 type check default {}", defaultOpts.chess960);
            println("option name UCI_ShowWDL type check default {}", defaultOpts.showWdl);
            println(
                "option name EvalSharpness type spin default {} min {} max {}",
                defaultOpts.evalSharpness,
                opts::kEvalSharpnessRange.min(),
                opts::kEvalSharpnessRange.max()
            );
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
                limit::kSoftNodeHardLimitMultiplierRange.min(),
                limit::kSoftNodeHardLimitMultiplierRange.max()
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
            println("option name SyzygyProbeRootOnly type check default {}", defaultOpts.syzygyProbeRootOnly);
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
                return;
            }

            m_searcher.newGame();
        }

        void UciHandler::handleIsready() {
            m_searcher.ensureReady();
            println("readyok");
        }

        void UciHandler::handlePosition(std::span<const std::string_view> args) {
            if (m_searcher.searching()) {
                eprintln("still searching");
                return;
            }

            if (args.empty()) {
                return;
            }

            const auto type = args[0];
            args = args.subspan<1>();

            usize next = 0;

            if (type == "startpos") {
                m_pos = Position::starting();
                m_keyHistory.clear();
            } else if (type == "fen") {
                const auto count = std::distance(args.begin(), std::ranges::find(args, "moves"));

                if (count == 0) {
                    eprintln("Missing fen");
                    return;
                }

                const auto parts = args.subspan(0, count);
                const auto newPos = Position::fromFenParts(parts);

                if (!newPos) {
                    return;
                }

                m_pos = *newPos;
                m_keyHistory.clear();

                next += count;
            } else if (type == "frc" || type == "dfrc") {
                if (!g_opts.chess960) {
                    eprintln("Chess960 not enabled");
                    return;
                }

                const auto count = std::distance(args.begin(), std::ranges::find(args, "moves"));

                const bool dfrc = type == "dfrc";

                if (count == 0) {
                    eprintln("Missing {} index", dfrc ? "DFRC" : "FRC");
                    return;
                }

                const auto index = util::tryParse<u32>(args[0]);

                if (!index) {
                    eprintln("Invalid {} index {}", dfrc ? "DFRC" : "FRC", args[0]);
                    return;
                }

                const auto newPos = dfrc ? Position::fromDfrcIndex(*index) : Position::fromFrcIndex(*index);

                if (!newPos) {
                    return;
                }

                m_pos = *newPos;
                m_keyHistory.clear();

                next += count;
            } else {
                eprintln("Invalid position type {}", type);
                return;
            }

            assert(next <= args.size());

            if (next >= args.size() || args[next] != "moves") {
                return;
            }

            for (usize i = next + 1; i < args.size(); ++i) {
                if (const auto move = m_pos.moveFromUci(args[i])) {
                    m_keyHistory.push_back(m_pos.key());
                    m_pos = m_pos.applyMove(move);
                } else {
                    eprintln("Invalid move {}", args[i]);
                    break;
                }
            }
        }

        void UciHandler::handleGo(std::span<const std::string_view> args, Instant startTime) {
            if (m_searcher.searching()) {
                eprintln("already searching");
                return;
            }

            u32 depth = kMaxDepth;
            auto limiter = std::make_unique<limit::CompoundLimiter>();

            MoveList movesToSearch{};

            bool infinite = false;
            bool tournamentTime = false;

            i64 timeRemaining{};
            i64 increment{};
            i32 toGo{};

            for (usize i = 0; i < args.size(); ++i) {
                if (args[i] == "depth" && ++i < args.size()) {
                    if (!util::tryParse<u32>(depth, args[i])) {
                        eprintln("invalid depth {}", args[i]);
                    }
                    continue;
                }

                if (args[i] == "infinite") {
                    infinite = true;
                    continue;
                }

                if (args[i] == "nodes" && ++i < args.size()) {
                    usize nodes{};
                    if (!util::tryParse<usize>(nodes, args[i])) {
                        eprintln("invalid node count {}", args[i]);
                    } else {
                        limiter->addLimiter<limit::NodeLimiter>(nodes);
                    }
                } else if (args[i] == "movetime" && ++i < args.size()) {
                    i64 time{};
                    if (!util::tryParse<i64>(time, args[i])) {
                        eprintln("invalid time {}", args[i]);
                    } else {
                        time = std::max<i64>(time, 1);
                        limiter->addLimiter<limit::MoveTimeLimiter>(time);
                    }
                } else if ((args[i] == "btime" || args[i] == "wtime") && ++i < args.size()
                           && args[i - 1] == (m_pos.stm() == Colors::kBlack ? "btime" : "wtime"))
                {
                    tournamentTime = true;

                    i64 time{};
                    if (!util::tryParse<i64>(time, args[i])) {
                        eprintln("invalid time {}", args[i]);
                    } else {
                        time = std::max<i64>(time, 1);
                        timeRemaining = static_cast<i64>(time);
                    }
                } else if ((args[i] == "binc" || args[i] == "winc") && ++i < args.size()
                           && args[i - 1] == (m_pos.stm() == Colors::kBlack ? "binc" : "winc"))
                {
                    tournamentTime = true;

                    i64 time{};
                    if (!util::tryParse<i64>(time, args[i])) {
                        eprintln("invalid time {}", args[i]);
                    } else {
                        time = std::max<i64>(time, 1);
                        increment = static_cast<i64>(time);
                    }
                } else if (args[i] == "movestogo" && ++i < args.size()) {
                    tournamentTime = true;

                    u32 moves{};
                    if (!util::tryParse<u32>(moves, args[i])) {
                        eprintln("invalid movestogo {}", args[i]);
                    } else {
                        moves = std::min<u32>(moves, static_cast<u32>(std::numeric_limits<i32>::max()));
                        toGo = static_cast<i32>(moves);
                    }
                } else if (args[i] == "searchmoves" && i + 1 < args.size()) {
                    while (i + 1 < args.size()) {
                        const auto& candidate = args[i + 1];

                        if (candidate.length() >= 4 && candidate.length() <= 5 && candidate[0] >= 'a'
                            && candidate[0] <= 'h' && candidate[1] >= '1' && candidate[1] <= '8' && candidate[2] >= 'a'
                            && candidate[2] <= 'h' && candidate[3] >= '1' && candidate[3] <= '8'
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
            }

            if (depth > kMaxDepth) {
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

        void UciHandler::handleStop() {
            if (!m_searcher.searching()) {
                eprintln("not searching");
                return;
            }

            m_searcher.stop();
        }

        //TODO refactor
        void UciHandler::handleSetoption(std::span<const std::string_view> args) {
            if (m_searcher.searching()) {
                eprintln("still searching");
                return;
            }

            if (args.empty()) {
                return;
            }

            usize i = 0;

            for (; i < args.size() - 1 && args[i] != "name"; ++i) {
                //
            }

            if (++i == args.size()) {
                return;
            }

            std::string name{};
            auto nameItr = std::back_inserter(name);

            for (; i < args.size() && args[i] != "value"; ++i) {
                if (!name.empty()) {
                    fmt::format_to(nameItr, " ");
                }

                fmt::format_to(nameItr, "{}", args[i]);
            }

            if (++i == args.size()) {
                return;
            }

            std::string value{};
            auto valueItr = std::back_inserter(value);

            for (; i < args.size(); ++i) {
                if (!value.empty()) {
                    fmt::format_to(valueItr, " ");
                }

                fmt::format_to(valueItr, "{}", args[i]);
            }

            if (!name.empty()) {
                std::transform(name.begin(), name.end(), name.begin(), [](auto c) { return std::tolower(c); });

                if (name == "hash") {
                    if (!value.empty()) {
                        if (const auto newTtSize = util::tryParse<usize>(value)) {
                            m_searcher.setTtSize(kTtSizeMibRange.clamp(*newTtSize));
                        }
                    }
                } else if (name == "clear hash") {
                    if (m_searcher.searching()) {
                        eprintln("still searching");
                    }

                    m_searcher.newGame();
                } else if (name == "threads") {
                    if (!value.empty()) {
                        if (const auto newThreads = util::tryParse<u32>(value)) {
                            opts::mutableOpts().threads = *newThreads;
                            m_searcher.setThreads(opts::kThreadCountRange.clamp(*newThreads));
                        }
                    }
                } else if (name == "multipv") {
                    if (!value.empty()) {
                        if (const auto newMultiPv = util::tryParse<i32>(value)) {
                            opts::mutableOpts().multiPv = opts::kMultiPvRange.clamp(*newMultiPv);
                        }
                    }
                } else if (name == "contempt") {
                    if (!value.empty()) {
                        if (const auto newContempt = util::tryParse<i32>(value)) {
                            opts::mutableOpts().contempt =
                                wdl::unnormalizeScoreMaterial58(kContemptRange.clamp(*newContempt));
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
                } else if (name == "evalsharpness") {
                    if (!value.empty()) {
                        if (const auto newEvalSharpness = util::tryParse<i32>(value)) {
                            opts::mutableOpts().evalSharpness = opts::kEvalSharpnessRange.clamp(*newEvalSharpness);
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
                        if (const auto newMoveOverhead = util::tryParse<i32>(value)) {
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
                        if (const auto newSoftNodeHardLimitMultiplier = util::tryParse<i32>(value)) {
                            opts::mutableOpts().softNodeHardLimitMultiplier =
                                limit::kSoftNodeHardLimitMultiplierRange.clamp(*newSoftNodeHardLimitMultiplier);
                        }
                    }
                } else if (name == "enableweirdtcs") {
                    if (!value.empty()) {
                        if (const auto newEnableWeirdTcs = util::tryParseBool(value)) {
                            opts::mutableOpts().enableWeirdTcs = *newEnableWeirdTcs;
                        }
                    }
                } else if (name == "syzygypath") {
                    m_tbInitialized = true;
                    opts::mutableOpts().syzygyEnabled = tb::init(value) == tb::InitStatus::kSuccess;
                } else if (name == "syzygyprobedepth") {
                    if (!value.empty()) {
                        if (const auto newSyzygyProbeDepth = util::tryParse<i32>(value)) {
                            opts::mutableOpts().syzygyProbeDepth =
                                search::kSyzygyProbeLimitRange.clamp(*newSyzygyProbeDepth);
                        }
                    }
                } else if (name == "syzygyprobelimit") {
                    if (!value.empty()) {
                        if (const auto newSyzygyProbeLimit = util::tryParse<i32>(value)) {
                            opts::mutableOpts().syzygyProbeLimit =
                                search::kSyzygyProbeLimitRange.clamp(*newSyzygyProbeLimit);
                        }
                    }
                } else if (name == "syzygyproberootonly") {
                    if (!value.empty()) {
                        if (const auto newSyzygyProbeRootOnly = util::tryParseBool(value)) {
                            opts::mutableOpts().syzygyProbeRootOnly = *newSyzygyProbeRootOnly;
                        }
                    }
                } else if (name == "evalfile") {
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
                    if (!value.empty() && util::tryParse<i32>(param->value, value) && param->callback) {
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

            auto pinned = m_pos.pinned(m_pos.stm());
            while (pinned) {
                print(" {}", pinned.popLowestSquare());
            }

            println();

            const auto staticEval = eval::adjustEval<false>(m_pos, {}, 0, nullptr, eval::staticEvalOnce(m_pos));

            const auto normalized = wdl::normalizeScore(staticEval, m_pos.classicalMaterial());
            const auto whiteNormalized = m_pos.stm() == Colors::kBlack ? -normalized : normalized;

            const auto unsharpened = wdl::normalizeScore<false>(staticEval, m_pos.classicalMaterial());
            const auto whiteUnsharpened = m_pos.stm() == Colors::kBlack ? -unsharpened : unsharpened;

            println("Static eval: {:+}", static_cast<f64>(whiteNormalized) / 100.0);
            println("Unsharpened eval: {:+}", static_cast<f64>(whiteUnsharpened) / 100.0);
        }

        void UciHandler::handleFen() {
            println("{}", m_pos.toFen());
        }

        void UciHandler::handleEval() {
            const auto staticEval = eval::adjustEval<false>(m_pos, {}, 0, nullptr, eval::staticEvalOnce(m_pos));
            const auto normalized = wdl::normalizeScore(staticEval, m_pos.classicalMaterial());

            println("Static eval: {:+}", static_cast<f64>(normalized) / 100.0);
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

        void UciHandler::handlePerft(std::span<const std::string_view> args) {
            u32 depth = 6;

            if (!args.empty()) {
                if (!util::tryParse(depth, args[0])) {
                    eprintln("invalid depth {}", args[0]);
                    return;
                }
            }

            perft(m_pos, static_cast<i32>(depth));
        }

        void UciHandler::handleSplitperft(std::span<const std::string_view> args) {
            u32 depth = 6;

            if (!args.empty()) {
                if (!util::tryParse(depth, args[0])) {
                    eprintln("invalid depth {}", args[0]);
                    return;
                }
            }

            splitPerft(m_pos, static_cast<i32>(depth));
        }

        void UciHandler::handleBench(std::span<const std::string_view> args) {
            if (m_searcher.searching()) {
                eprintln("already searching");
                return;
            }

            i32 depth = bench::kDefaultBenchDepth;
            usize ttSize = bench::kDefaultBenchTtSize;

            if (args.size() > 0) {
                if (const auto newDepth = util::tryParse<u32>(args[0])) {
                    depth = static_cast<i32>(*newDepth);
                } else {
                    println("info string invalid depth {}", args[0]);
                    return;
                }
            }

            if (args.size() > 1) {
                if (const auto newThreads = util::tryParse<u32>(args[1])) {
                    if (*newThreads > 1) {
                        println("info string multiple search threads not yet supported, using 1");
                    }
                } else {
                    println("info string invalid thread count {}", args[1]);
                    return;
                }
            }

            if (args.size() > 2) {
                if (const auto newTtSize = util::tryParse<usize>(args[2])) {
                    ttSize = static_cast<i32>(*newTtSize);
                } else {
                    println("info string invalid tt size {}", args[2]);
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

        void UciHandler::handleProbeWdl() {
            if (!m_tbInitialized || !g_opts.syzygyEnabled) {
                eprintln("no TBs loaded");
                return;
            }

            if (m_pos.bbs().occupancy().popcount() > TB_LARGEST) {
                eprintln("too many pieces");
                return;
            }

            const auto wdl = tb::probeRoot(nullptr, m_pos);

            switch (wdl) {
                case tb::ProbeResult::kFailed:
                    println("failed");
                    break;
                case tb::ProbeResult::kWin:
                    println("win");
                    break;
                case tb::ProbeResult::kDraw:
                    println("draw");
                    break;
                case tb::ProbeResult::kLoss:
                    println("loss");
                    break;
            }
        }

        void UciHandler::handleWait() {
            m_searcher.waitForStop();
        }
    } // namespace

#if SP_EXTERNAL_TUNE
    namespace tunable {
        TunableParam& addTunableParam(
            std::string_view name,
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

            std::string strName{name};

            auto lowerName = strName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](auto c) {
                return std::tolower(c);
            });

            return params.emplace_back(
                TunableParam{
                    std::move(strName),
                    std::move(lowerName),
                    value,
                    value,
                    {min, max},
                    step,
                    std::move(callback)
                }
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
                std::span<const std::string_view> params,
                const std::function<void(const tunable::TunableParam&)>& printParam
            ) {
                if (std::ranges::find(params, "<all>") != params.end()) {
                    for (const auto& param : tunableParams()) {
                        printParam(param);
                    }

                    return;
                }

                for (const auto paramName : params) {
                    std::string lowerName{paramName};
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](auto c) {
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

        void printWfTuningParams(std::span<const std::string_view> params) {
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

        void printCttTuningParams(std::span<const std::string_view> params) {
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

        void printObTuningParams(std::span<const std::string_view> params) {
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
