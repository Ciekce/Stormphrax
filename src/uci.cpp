/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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
#include "limit.h"
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

            bool m_quit{false};

            bool m_tbInitialized{false};

            search::Searcher m_searcher{};

            std::vector<u64> m_keyHistory{};
            Position m_pos{Position::starting()};

            i32 m_moveOverhead{limit::kDefaultMoveOverheadMs};
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

                if (m_quit) {
                    break;
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
                limit::kDefaultMoveOverheadMs,
                limit::kMoveOverheadRange.min(),
                limit::kMoveOverheadRange.max()
            );
            println("option name SoftNodes type check default {}", defaultOpts.softNodes);
            println(
                "option name SoftNodeHardLimitMultiplier type spin default {} min {} max {}",
                defaultOpts.softNodeHardLimitMultiplier,
                opts::kSoftNodeHardLimitMultiplierRange.min(),
                opts::kSoftNodeHardLimitMultiplierRange.max()
            );
            println("option name EnableWeirdTCs type check default {}", defaultOpts.enableWeirdTcs);
            println("option name Minimal type check default {}", defaultOpts.minimal);
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
            if (!eval::isNetworkLoaded()) {
                eprintln("No network loaded");
                return;
            }

            if (m_searcher.searching()) {
                eprintln("already searching");
                return;
            }

            MoveList movesToSearch{};

            limit::SearchLimiter limiter{startTime};

            bool infinite = false;

            auto maxDepth = kMaxDepth;

            std::optional<f64> wtime{};
            std::optional<f64> btime{};

            std::optional<f64> winc{};
            std::optional<f64> binc{};

            std::optional<u32> movesToGo{};

            for (i32 i = 0; i < args.size(); ++i) {
                const auto limitStr = args[i];

                if (limitStr == "infinite") {
                    infinite = true;
                } else if (limitStr == "depth") {
                    if (++i == args.size()) {
                        eprintln("Missing depth");
                        return;
                    }

                    if (!util::tryParse(maxDepth, args[i])) {
                        eprintln("Invalid depth '{}'", args[i]);
                        return;
                    }
                } else if (limitStr == "nodes") {
                    if (++i == args.size()) {
                        eprintln("Missing node limit");
                        return;
                    }

                    usize nodes{};

                    if (!util::tryParse(nodes, args[i])) {
                        eprintln("Invalid node limit '{}'", args[i]);
                        return;
                    }

                    auto hardNodes = nodes;

                    if (g_opts.softNodes) {
                        hardNodes *= g_opts.softNodeHardLimitMultiplier;
                        limiter.setSoftNodes(nodes);
                    }

                    if (!limiter.setHardNodes(hardNodes)) {
                        eprintln("Duplicate node limits");
                        return;
                    }
                } else if (limitStr == "movetime") {
                    if (++i == args.size()) {
                        eprintln("Missing move time limit");
                        return;
                    }

                    i64 maxTimeMs{};

                    if (!util::tryParse(maxTimeMs, args[i])) {
                        eprintln("Invalid move time limit '{}'", args[i]);
                        return;
                    }

                    maxTimeMs = std::max<i64>(maxTimeMs, 1);

                    const auto maxTimeSec = static_cast<f64>(maxTimeMs) / 1000.0;

                    if (!limiter.setMoveTime(maxTimeSec)) {
                        eprintln("Duplicate movetime limits");
                        return;
                    }
                } else if (limitStr == "btime" || limitStr == "wtime" || limitStr == "binc" || limitStr == "winc") {
                    if (++i == args.size()) {
                        eprintln("Missing {} limit", limitStr);
                        return;
                    }

                    auto& limit = [&]() -> std::optional<f64>& {
                        if (limitStr == "wtime") {
                            return wtime;
                        } else if (limitStr == "btime") {
                            return btime;
                        } else if (limitStr == "winc") {
                            return winc;
                        } else if (limitStr == "binc") {
                            return binc;
                        }
                        __builtin_unreachable();
                    }();

                    i64 ms{};

                    if (!util::tryParse(ms, args[i])) {
                        eprintln("Invalid {} limit '{}'", limitStr, args[i]);
                        return;
                    }

                    if (limit) {
                        eprintln("Duplicate {} limits", limitStr);
                        return;
                    }

                    limit = static_cast<f64>(ms) / 1000.0;
                } else if (limitStr == "movestogo") {
                    if (++i == args.size()) {
                        eprintln("Missing move time limit");
                        return;
                    }

                    u32 mtg{};

                    if (!util::tryParse(mtg, args[i])) {
                        eprintln("Invalid moves to go '{}'", args[i]);
                        return;
                    }

                    if (mtg > 0) {
                        if (movesToGo) {
                            eprintln("Duplicate moves to go");
                            return;
                        }

                        movesToGo = mtg;
                    }
                } else if (limitStr == "searchmoves" && i + 1 < args.size()) {
                    while (i + 1 < args.size()) {
                        const auto& candidate = args[i + 1];

                        if (candidate.length() >= 4 && candidate.length() <= 5 //
                            && candidate[0] >= 'a' && candidate[0] <= 'h' && candidate[1] >= '1' && candidate[1] <= '8'
                            && candidate[2] >= 'a' && candidate[2] <= 'h' && candidate[3] >= '1' && candidate[3] <= '8'
                            && (candidate.length() < 5 || PieceType::fromChar(candidate[4]).isValidPromotion()))
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

            if (maxDepth == 0) {
                return;
            }

            if (maxDepth > kMaxDepth) {
                maxDepth = kMaxDepth;
            }

            const auto time = m_pos.stm() == Colors::kBlack ? btime : wtime;
            const auto inc = m_pos.stm() == Colors::kBlack ? binc : winc;

            if (inc && !time) {
                println("info string Warning: increment given but no time, ignoring");
            }

            if (time) {
                const limit::TimeLimits limits{
                    .remaining = std::max(*time, 0.001),
                    .increment = inc.value_or(0.0),
                    .movesToGo = movesToGo,
                };

                if (movesToGo) {
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
                } else if (limits.increment == 0) {
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

                limiter.setTournamentTime(limits, m_moveOverhead);
            }

            m_searcher.setLimiter(limiter);
            m_searcher.setMaxDepth(maxDepth);

            m_searcher.startSearch(m_pos, m_keyHistory, startTime, movesToSearch, infinite);
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
                                opts::kSoftNodeHardLimitMultiplierRange.clamp(*newSoftNodeHardLimitMultiplier);
                        }
                    }
                } else if (name == "enableweirdtcs") {
                    if (!value.empty()) {
                        if (const auto newEnableWeirdTcs = util::tryParseBool(value)) {
                            opts::mutableOpts().enableWeirdTcs = *newEnableWeirdTcs;
                        }
                    }
                } else if (name == "minimal") {
                    if (!value.empty()) {
                        if (const auto newMinimal = util::tryParseBool(value)) {
                            opts::mutableOpts().minimal = *newMinimal;
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

            const auto staticEval = eval::adjustEval<false>(m_pos, {}, nullptr, eval::staticEvalOnce(m_pos));

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
            const auto staticEval = eval::adjustEval<false>(m_pos, {}, nullptr, eval::staticEvalOnce(m_pos));
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
                    eprintln("invalid depth {}", args[0]);
                    return;
                }
            }

            if (args.size() > 1) {
                if (const auto newTtSize = util::tryParse<usize>(args[2])) {
                    ttSize = static_cast<i32>(*newTtSize);
                } else {
                    eprintln("invalid tt size {}", args[2]);
                    return;
                }
            }

            if (depth == 0) {
                depth = 1;
            }

            bench::run(depth, ttSize);

            m_quit = true;
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
