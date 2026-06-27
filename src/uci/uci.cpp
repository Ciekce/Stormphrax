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

#include "../../3rdparty/pyrrhic/tbprobe.h"
#include "../bench.h"
#include "../eval/eval.h"
#include "../limit.h"
#include "../movegen.h"
#include "../opts.h"
#include "../perft.h"
#include "../position.h"
#include "../search.h"
#include "../tb.h"
#include "../ttable.h"
#include "../tunable.h"
#include "../util/parse.h"
#include "../util/split.h"
#include "../util/timer.h"
#include "../wdl.h"
#include "option.h"

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
                params.reserve(256);
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
            UciHandler();
            ~UciHandler();

            i32 run(std::span<const std::string_view> commands);

        private:
            void handleCommand(std::span<std::string_view> tokens, Instant startTime);

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
            void handleMove(std::span<const std::string_view> args);

            void applyMove(Move move);

            void checkNewOption(std::string_view name);

            void registerCheckOption(
                std::string_view name,
                bool* ptr,
                bool nullDefault,
                std::function<void(bool)> callback = nullptr
            );
            void registerSpinOption(
                std::string_view name,
                i32* ptr,
                i32 nullDefault,
                util::Range<i32> range,
                std::function<void(i32)> callback = nullptr
            );
            void registerButtonOption(std::string_view name, std::function<void()> callback);
            void registerStringOption(
                std::string_view name,
                std::string* ptr,
                std::string_view nullDefault,
                std::function<void(std::string_view)> callback = nullptr
            );

            std::vector<option::Option> m_options{};

            bool m_quit{false};

            bool m_tbInitialized{false};

            search::Searcher m_searcher{};

            std::vector<u64> m_keyHistory{};
            Position m_pos{Position::startpos()};
        };

        UciHandler::UciHandler() {
            using namespace opts;

            static const GlobalOptions s_defaultOpts{};

            auto& opts = mutableOpts();

            registerSpinOption("Hash", nullptr, kDefaultTtSizeMib, kTtSizeMibRange, [&](i32 newHash) {
                m_searcher.setTtSize(newHash);
            });
            registerButtonOption("ClearHash", [&] { m_searcher.newGame(); });
            registerSpinOption("Threads", &opts.threads, s_defaultOpts.threads, kThreadCountRange, [&](i32 newThreads) {
                m_searcher.setThreads(newThreads);
            });
            registerSpinOption("MultiPV", &opts.multiPv, s_defaultOpts.multiPv, kMultiPvRange);
            registerSpinOption("Contempt", &opts.contempt, s_defaultOpts.contempt, kContemptRange);
            registerCheckOption("UCI_Chess960", &opts.chess960, s_defaultOpts.chess960);
            registerCheckOption("UCI_ShowWDL", &opts.showWdl, s_defaultOpts.showWdl);
            registerSpinOption("EvalSharpness", &opts.evalSharpness, s_defaultOpts.evalSharpness, kEvalSharpnessRange);
            registerCheckOption("ShowCurrMove", &opts.showCurrMove, s_defaultOpts.showCurrMove);
            registerSpinOption("MoveOverhead", &opts.moveOverhead, s_defaultOpts.moveOverhead, kMoveOverheadRange);
            registerCheckOption("SoftNodes", &opts.softNodes, s_defaultOpts.softNodes);
            registerSpinOption(
                "SoftNodeHardLimitMultiplier",
                &opts.softNodeHardLimitMultiplier,
                s_defaultOpts.softNodeHardLimitMultiplier,
                kSoftNodeHardLimitMultiplierRange
            );
            registerCheckOption("EnableWeirdTCs", &opts.enableWeirdTcs, s_defaultOpts.enableWeirdTcs, [](bool) {
                println("info string Note: EnableWeirdTCs is deprecated, and will be removed in a future release.");
            });
            registerCheckOption("Minimal", &opts.minimal, s_defaultOpts.minimal);
            registerStringOption("SyzygyPath", nullptr, "<empty>", [&](std::string_view value) {
                if (value == "<empty>") {
                    opts.syzygyEnabled = false;
                    if (m_tbInitialized) {
                        tb::free();
                        m_tbInitialized = false;
                    }
                    return;
                }

                opts.syzygyEnabled = tb::init(value) == tb::InitStatus::kSuccess;
                m_tbInitialized = true;
            });
            registerSpinOption(
                "SyzygyProbeDepth",
                &opts.syzygyProbeDepth,
                s_defaultOpts.syzygyProbeDepth,
                search::kSyzygyProbeDepthRange
            );
            registerSpinOption(
                "SyzygyProbeLimit",
                &opts.syzygyProbeLimit,
                s_defaultOpts.syzygyProbeLimit,
                search::kSyzygyProbeLimitRange
            );
            registerCheckOption("SyzygyProbeRootOnly", &opts.syzygyProbeRootOnly, s_defaultOpts.syzygyProbeRootOnly);
        }

        UciHandler::~UciHandler() {
            // can't do this in a destructor, because it will run after tb::free() is called
            m_searcher.quit();
            tb::free();
        }

        i32 UciHandler::run(std::span<const std::string_view> commands) {
            std::vector<std::string_view> tokens{};

            const auto handleLine = [&](std::string_view line) {
                const auto startTime = Instant::now();

                tokens.clear();
                split::split(tokens, line, ' ');

                handleCommand(tokens, startTime);
            };

            for (const auto line : commands) {
                handleLine(line);
            }

            for (std::string line{}; !m_quit && std::getline(std::cin, line);) {
                handleLine(line);
            }

            return 0;
        }

        void UciHandler::handleCommand(std::span<std::string_view> tokens, Instant startTime) {
            if (tokens.empty()) {
                return;
            }

            std::string command{};
            command.reserve(tokens[0].size());
            std::ranges::transform(tokens[0], std::back_inserter(command), [](auto c) { return std::tolower(c); });

            const auto args = std::span{tokens}.subspan<1>();

            if (command == "quit") {
                m_quit = true;
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
            } else if (command == "move") {
                handleMove(args);
            } else {
                eprintln("Unknown command '{}'", command);
            }
        }

        void UciHandler::handleUci() {
#ifdef SP_COMMIT_HASH
            println("id name {} {} {}", kName, kVersion, SP_STRINGIFY(SP_COMMIT_HASH));
#else
            println("id name {} {}", kName, kVersion);
#endif
            println("id author {}", kAuthor);

            for (const auto& option : m_options) {
                option.display();
            }

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
                m_pos = Position::startpos();
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
                const auto move = m_pos.moveFromUci(args[i]);

                if (!move) {
                    eprintln("Invalid move {}", args[i]);
                    break;
                }

                if (!m_pos.isLegal(move)) {
                    eprintln("Illegal move {}", args[i]);
                    break;
                }

                applyMove(move);
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
                        const auto candidate = args[i + 1];
                        const auto move = m_pos.moveFromUci(candidate);

                        if (!move) {
                            break;
                        }

                        if (std::ranges::find(movesToSearch, move) == movesToSearch.end()) {
                            if (m_pos.isLegal(move)) {
                                movesToSearch.push(move);
                            } else {
                                println("info string ignoring illegal move {}", candidate);
                            }
                        }

                        ++i;
                    }
                }
            }

            if (!movesToSearch.empty()) {
                print("info string searching moves:");

                for (const auto move : movesToSearch) {
                    print(" {}", move);
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

                limiter.setTournamentTime(limits);
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

        void UciHandler::handleSetoption(std::span<const std::string_view> args) {
            if (m_searcher.searching()) {
                eprintln("still searching");
                return;
            }

            if (args.size() < 2 || args[0] != "name") {
                return;
            }

            const auto valueIdx = std::distance(args.begin(), std::ranges::find(args, "value"));

            if (valueIdx == 1) {
                eprintln("Missing option name");
                return;
            }

            if (valueIdx > 2) {
                std::string str{};
                auto itr = std::back_inserter(str);

                bool first = true;
                for (usize i = 2; i < valueIdx; ++i) {
                    if (!first) {
                        fmt::format_to(itr, " {}", args[i]);
                    } else {
                        fmt::format_to(itr, "{}", args[i]);
                        first = false;
                    }
                }

                eprintln("Warning: spaces in option names not supported, skipping \"{}\"", str);
            }

            const auto name = args[1];

            std::string value{};

            if (valueIdx < args.size()) {
                auto itr = std::back_inserter(value);

                bool first = true;
                for (usize i = valueIdx + 1; i < args.size(); ++i) {
                    if (!first) {
                        fmt::format_to(itr, " {}", args[i]);
                    } else {
                        fmt::format_to(itr, "{}", args[i]);
                        first = false;
                    }
                }
            }

            const auto id = option::Option::toId(name);
            for (auto& option : m_options) {
                if (option.id() == id) {
                    option.set(value);
                    return;
                }
            }

#if SP_EXTERNAL_TUNE
            if (auto* param = lookupTunableParam(name)) {
                if (!value.empty() && util::tryParse<i32>(param->value, value) && param->callback) {
                    param->callback();
                }
                return;
            }
#endif

            eprintln("Unknown option '{}'", name);
        }

        void UciHandler::handleD() {
            println();
            println("{}", m_pos);

            println();
            println("Fen: {}", m_pos.toFen());

            println("Key: {:016x}", m_pos.key());
            println("Pawn key: {:016x}", m_pos.pawnKey());

            print("Checkers:");
            for (const auto checker : m_pos.checkers()) {
                print(" {}", checker);
            }
            println();

            print("White pinned:");
            for (const auto pinned : m_pos.pinned(Colors::kWhite)) {
                print(" {}", pinned);
            }
            println();

            print("Black pinned:");
            for (const auto pinned : m_pos.pinned(Colors::kBlack)) {
                print(" {}", pinned);
            }
            println();

            const auto staticEval = eval::adjustEval<false>(m_pos, {}, {}, nullptr, eval::staticEvalOnce(m_pos));

            const auto normalized = wdl::normalizeScore(staticEval, m_pos.classicalMaterial());
            const auto whiteNormalized = m_pos.stm() == Colors::kBlack ? -normalized : normalized;

            const auto unsharpened = wdl::normalizeScore<false>(staticEval, m_pos.classicalMaterial());
            const auto whiteUnsharpened = m_pos.stm() == Colors::kBlack ? -unsharpened : unsharpened;

            println("Static eval (white-relative): {:+}", static_cast<f64>(whiteNormalized) / 100.0);
            println("Unsharpened eval: {:+}", static_cast<f64>(whiteUnsharpened) / 100.0);
        }

        void UciHandler::handleFen() {
            println("{}", m_pos.toFen());
        }

        void UciHandler::handleEval() {
            const auto staticEval = eval::adjustEval<false>(m_pos, {}, {}, nullptr, eval::staticEvalOnce(m_pos));
            const auto normalized = wdl::normalizeScore(staticEval, m_pos.classicalMaterial());

            println("Static eval: {:+}", static_cast<f64>(normalized) / 100.0);
        }

        void UciHandler::handleRawEval() {
            const auto score = eval::staticEvalOnce(m_pos);
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

            if (m_pos.occ().popcount() > TB_LARGEST) {
                eprintln("too many pieces");
                return;
            }

            const auto [wdl, dtzSucceeded] = tb::probeRoot(m_pos, m_keyHistory);

            switch (wdl) {
                case search::GameResult::kNone:
                    print("failed");
                    break;
                case search::GameResult::kWin:
                    print("win");
                    break;
                case search::GameResult::kDraw:
                    print("draw");
                    break;
                case search::GameResult::kLoss:
                    print("loss");
                    break;
            }

            if (wdl != search::GameResult::kNone && !dtzSucceeded) {
                print(" (DTZ probe failed)");
            }

            println();
        }

        void UciHandler::handleWait() {
            m_searcher.waitForStop();
        }

        void UciHandler::handleMove(std::span<const std::string_view> args) {
            if (args.empty()) {
                return;
            }

            const auto move = m_pos.moveFromUci(args[0]);

            if (!move) {
                eprintln("Invalid move {}", args[0]);
                return;
            }

            if (!m_pos.isLegal(move)) {
                eprintln("Illegal move {}", args[0]);
                return;
            }

            applyMove(move);
        }

        void UciHandler::applyMove(Move move) {
            m_keyHistory.push_back(m_pos.key());
            m_pos = m_pos.applyMove(move);

            // keep a few prior keys around for contcorr purposes
            // bit hacky
            if (m_pos.halfmove() == 0 && m_keyHistory.size() > 6) {
                std::copy(m_keyHistory.end() - 6, m_keyHistory.end(), m_keyHistory.begin());
                m_keyHistory.resize(6);
            }
        }

        void UciHandler::checkNewOption(std::string_view name) {
            if (std::ranges::find(name, ' ') != name.end()) {
                eprintln("Option name \"{}\" must not contain any spaces!", name);
                std::terminate();
            }

            const auto id = option::Option::toId(name);

            for (const auto& option : m_options) {
                if (option.id() == id) {
                    eprintln("Duplicate option {}!", name);
                    std::terminate();
                }
            }
        }

        void UciHandler::registerCheckOption(
            std::string_view name,
            bool* ptr,
            bool nullDefault,
            std::function<void(bool)> callback
        ) {
            checkNewOption(name);
            m_options.emplace_back(name, ptr, nullDefault, std::move(callback));
        }

        void UciHandler::registerSpinOption(
            std::string_view name,
            i32* ptr,
            i32 nullDefault,
            util::Range<i32> range,
            std::function<void(i32)> callback
        ) {
            checkNewOption(name);
            m_options.emplace_back(name, ptr, nullDefault, std::move(callback), range);
        }

        void UciHandler::registerButtonOption(std::string_view name, std::function<void()> callback) {
            checkNewOption(name);
            m_options.emplace_back(name, std::move(callback));
        }

        void UciHandler::registerStringOption(
            std::string_view name,
            std::string* ptr,
            std::string_view nullDefault,
            std::function<void(std::string_view)> callback
        ) {
            checkNewOption(name);
            m_options.emplace_back(name, ptr, nullDefault, std::move(callback));
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

            std::string lowerName{};
            lowerName.reserve(strName.size());
            std::ranges::transform(strName, std::back_inserter(lowerName), [](auto c) { return std::tolower(c); });

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
        i32 run(std::span<const std::string_view> commands) {
            UciHandler handler{};
            return handler.run(commands);
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
                    std::string lowerName{};
                    lowerName.reserve(paramName.size());
                    std::ranges::transform(paramName, std::back_inserter(lowerName), [](auto c) {
                        return std::tolower(c);
                    });

                    if (const auto* param = lookupTunableParam(paramName)) {
                        printParam(*param);
                    } else {
                        eprintln("unknown parameter '{}'", paramName);
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
