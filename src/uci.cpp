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

#include "uci.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <atomic>
#include <numeric>

#include "util/split.h"
#include "util/parse.h"
#include "position/position.h"
#include "search.h"
#include "movegen.h"
#include "eval/eval.h"
#include "pretty.h"
#include "ttable.h"
#include "limit/trivial.h"
#include "limit/time.h"
#include "perft.h"
#include "bench.h"
#include "opts.h"
#include "tunable.h"
#include "3rdparty/fathom/tbprobe.h"

namespace stormphrax
{
	using namespace uci;

	namespace
	{
		constexpr auto Name = "Stormphrax";
		constexpr auto Version = SP_STRINGIFY(SP_VERSION);
		constexpr auto Author = "Ciekce";

#if SP_TUNE_SEARCH
		tunable::TunableData s_tunable{};
#endif

		class UciHandler
		{
		public:
			UciHandler() = default;
			~UciHandler();

			auto run() -> i32;

		private:
			auto handleUci() -> void;
			auto handleUcinewgame() -> void;
			auto handleIsready() -> void;
			auto handlePosition(const std::vector<std::string> &tokens) -> void;
			auto handleGo(const std::vector<std::string> &tokens) -> void;
			auto handleStop() -> void;
			auto handleSetoption(const std::vector<std::string> &tokens) -> void;
			// V ======= NONSTANDARD ======= V
			auto handleD() -> void;
			auto handleCheckers() -> void;
			auto handleEval() -> void;
			auto handleRegen() -> void;
			auto handleMoves() -> void;
			auto handlePerft(const std::vector<std::string> &tokens) -> void;
			auto handleSplitperft(const std::vector<std::string> &tokens) -> void;
			auto handleBench(const std::vector<std::string> &tokens) -> void;
#ifndef NDEBUG
			auto handleVerify() -> void;
#endif

			bool m_fathomInitialized{false};

			search::Searcher m_searcher{};

			Position m_pos{Position::starting()};

			i32 m_moveOverhead{limit::DefaultMoveOverhead};
		};

		UciHandler::~UciHandler()
		{
			// can't do this in a destructor, because it will run after tb_free is called
			m_searcher.quit();

			if (m_fathomInitialized)
				tb_free();
		}

		auto UciHandler::run() -> i32
		{
			for (std::string line{}; std::getline(std::cin, line);)
			{
				const auto tokens = split::split(line, ' ');

				if (tokens.empty())
					continue;

				const auto &command = tokens[0];

				if (command == "quit")
					return 0;
				else if (command == "uci")
					handleUci();
				else if (command == "ucinewgame")
					handleUcinewgame();
				else if (command == "isready")
					handleIsready();
				else if (command == "position")
					handlePosition(tokens);
				else if (command == "go")
					handleGo(tokens);
				else if (command == "stop")
					handleStop();
				else if (command == "setoption")
					handleSetoption(tokens);
				// V ======= NONSTANDARD ======= V
				else if (command == "d")
					handleD();
				else if (command == "eval")
					handleEval();
				else if (command == "checkers")
					handleCheckers();
				else if (command == "regen")
					handleRegen();
				else if (command == "moves")
					handleMoves();
				else if (command == "perft")
					handlePerft(tokens);
				else if (command == "splitperft")
					handleSplitperft(tokens);
				else if (command == "bench")
					handleBench(tokens);
#ifndef NDEBUG
				else if (command == "verify")
					handleVerify();
#endif
			}

			return 0;
		}

		auto UciHandler::handleUci() -> void
		{
			static const opts::GlobalOptions defaultOpts{};

			std::cout << "id name " << Name << ' ' << Version << '\n';
			std::cout << "id author " << Author << '\n';

			std::cout << "option name Hash type spin default " << DefaultHashSize
				<< " min " << HashSizeRange.min() << " max " << HashSizeRange.max() << '\n';
			std::cout << "option name Clear Hash type button\n";
			std::cout << "option name Threads type spin default " << search::DefaultThreadCount
				<< " min " << search::ThreadCountRange.min() << " max " << search::ThreadCountRange.max() << '\n';
			//TODO
		//	std::cout << "option name Contempt type spin default 0 min -10000 max 10000\n";
			std::cout << "option name UCI_Chess960 type check default "
				<< (defaultOpts.chess960 ? "true" : "false") << '\n';
			std::cout << "option name UCI_ShowWDL type check default "
				<< (defaultOpts.showWdl ? "true" : "false") << '\n';
			std::cout << "option name Move Overhead type spin default " << limit::DefaultMoveOverhead
				<< " min " << limit::MoveOverheadRange.min() << " max " << limit::MoveOverheadRange.max() << '\n';
			std::cout << "option name SyzygyPath type string default <empty>\n";
			std::cout << "option name SyzygyProbeDepth type spin default " << defaultOpts.syzygyProbeDepth
				<< " min " << search::SyzygyProbeDepthRange.min()
				<< " max " << search::SyzygyProbeDepthRange.max() << '\n';
			std::cout << "option name SyzygyProbeLimit type spin default " << defaultOpts.syzygyProbeLimit
				<< " min " << search::SyzygyProbeLimitRange.min()
				<< " max " << search::SyzygyProbeLimitRange.max() << '\n';
			std::cout << "option name EvalFile type string default <internal>" << std::endl;

			std::cout << "uciok" << std::endl;
		}

		auto UciHandler::handleUcinewgame() -> void
		{
			if (m_searcher.searching())
				std::cerr << "still searching" << std::endl;
			else m_searcher.newGame();
		}

		auto UciHandler::handleIsready() -> void
		{
			std::cout << "readyok" << std::endl;
		}

		auto UciHandler::handlePosition(const std::vector<std::string> &tokens) -> void
		{
			if (m_searcher.searching())
				std::cerr << "still searching" << std::endl;
			else if (tokens.size() > 1)
			{
				const auto &position = tokens[1];

				usize next = 2;

				if (position == "startpos")
					m_pos.resetToStarting();
				else if (position == "fen")
				{
					std::ostringstream fen{};

					for (usize i = 0; i < 6 && next < tokens.size(); ++i, ++next)
					{
						fen << tokens[next] << ' ';
					}

					if (!m_pos.resetFromFen(fen.str()))
						return;
				}
				else if (position == "frc")
				{
					if (!g_opts.chess960)
					{
						std::cerr << "Chess960 not enabled" << std::endl;
						return;
					}

					if (next < tokens.size())
					{
						if (const auto frcIndex = util::tryParseU32(tokens[next++]);
							frcIndex && !m_pos.resetFromFrcIndex(*frcIndex))
							return;
					}
				}
				else if (position == "dfrc")
				{
					if (!g_opts.chess960)
					{
						std::cerr << "Chess960 not enabled" << std::endl;
						return;
					}

					if (next < tokens.size())
					{
						if (const auto dfrcIndex = util::tryParseU32(tokens[next++]);
							dfrcIndex && !m_pos.resetFromDfrcIndex(*dfrcIndex))
							return;
					}
				}
				else return;

				if (next < tokens.size() && tokens[next++] == "moves")
				{
					for (; next < tokens.size(); ++next)
					{
						if (const auto move = m_pos.moveFromUci(tokens[next]))
							m_pos.applyMoveUnchecked<false, false>(move, nullptr);
					}
				}
			}
		}

		auto UciHandler::handleGo(const std::vector<std::string> &tokens) -> void
		{
			if (m_searcher.searching())
				std::cerr << "already searching" << std::endl;
			else
			{
				u32 depth = search::MaxDepth;
				std::unique_ptr<limit::ISearchLimiter> limiter{};

				bool tournamentTime = false;

				const auto startTime = util::g_timer.time();

				i64 timeRemaining{};
				i64 increment{};
				i32 toGo{};

				for (usize i = 1; i < tokens.size(); ++i)
				{
					if (tokens[i] == "depth" && ++i < tokens.size())
					{
						if (!util::tryParseU32(depth, tokens[i]))
							std::cerr << "invalid depth " << tokens[i] << std::endl;
					}
					else if (!tournamentTime && !limiter)
					{
						if (tokens[i] == "infinite")
							limiter = std::make_unique<limit::InfiniteLimiter>();
						else if (tokens[i] == "nodes" && ++i < tokens.size())
						{
							std::cout << "info string node limiting currently broken" << std::endl;

							usize nodes{};
							if (!util::tryParseSize(nodes, tokens[i]))
								std::cerr << "invalid node count " << tokens[i] << std::endl;
							else
								limiter = std::make_unique<limit::NodeLimiter>(nodes);
						}
						else if (tokens[i] == "movetime" && ++i < tokens.size())
						{
							i64 time{};
							if (!util::tryParseI64(time, tokens[i]))
								std::cerr << "invalid time " << tokens[i] << std::endl;
							else
							{
								time = std::max<i64>(time, 1);
								limiter = std::make_unique<limit::MoveTimeLimiter>(time, m_moveOverhead);
							}
						}
						else if ((tokens[i] == "btime" || tokens[i] == "wtime") && ++i < tokens.size()
							&& tokens[i - 1] == (m_pos.toMove() == Color::Black ? "btime" : "wtime"))
						{
							tournamentTime = true;

							i64 time{};
							if (!util::tryParseI64(time, tokens[i]))
								std::cerr << "invalid time " << tokens[i] << std::endl;
							else
							{
								time = std::max<i64>(time, 1);
								timeRemaining = static_cast<i64>(time);
							}
						}
						else if ((tokens[i] == "binc" || tokens[i] == "winc") && ++i < tokens.size()
							&& tokens[i - 1] == (m_pos.toMove() == Color::Black ? "binc" : "winc"))
						{
							tournamentTime = true;

							i64 time{};
							if (!util::tryParseI64(time, tokens[i]))
								std::cerr << "invalid time " << tokens[i] << std::endl;
							else
							{
								time = std::max<i64>(time, 1);
								increment = static_cast<i64>(time);
							}
						}
						else if (tokens[i] == "movestogo" && ++i < tokens.size())
						{
							tournamentTime = true;

							u32 moves{};
							if (!util::tryParseU32(moves, tokens[i]))
								std::cerr << "invalid movestogo " << tokens[i] << std::endl;
							else
							{
								moves = std::min<u32>(moves, static_cast<u32>(std::numeric_limits<i32>::max()));
								toGo = static_cast<i32>(moves);
							}
						}
					}
					// yeah I hate the duplication too
					else if (tournamentTime)
					{
						if ((tokens[i] == "btime" || tokens[i] == "wtime") && ++i < tokens.size()
							&& tokens[i - 1] == (m_pos.toMove() == Color::Black ? "btime" : "wtime"))
						{
							i64 time{};
							if (!util::tryParseI64(time, tokens[i]))
								std::cerr << "invalid time " << tokens[i] << std::endl;
							else
							{
								time = std::max<i64>(time, 1);
								timeRemaining = static_cast<i64>(time);
							}
						}
						else if ((tokens[i] == "binc" || tokens[i] == "winc") && ++i < tokens.size()
							&& tokens[i - 1] == (m_pos.toMove() == Color::Black ? "binc" : "winc"))
						{
							i64 time{};
							if (!util::tryParseI64(time, tokens[i]))
								std::cerr << "invalid time " << tokens[i] << std::endl;
							else
							{
								time = std::max<i64>(time, 1);
								increment = static_cast<i64>(time);
							}
						}
						else if (tokens[i] == "movestogo" && ++i < tokens.size())
						{
							u32 moves{};
							if (!util::tryParseU32(moves, tokens[i]))
								std::cerr << "invalid movestogo " << tokens[i] << std::endl;
							else
							{
								moves = std::min<u32>(moves, static_cast<u32>(std::numeric_limits<i32>::max()));
								toGo = static_cast<i32>(moves);
							}
						}
					}
				}

				if (depth == 0)
					return;
				else if (depth > search::MaxDepth)
					depth = search::MaxDepth;

				if (tournamentTime && timeRemaining > 0)
					limiter = std::make_unique<limit::TimeManager>(startTime,
						static_cast<f64>(timeRemaining) / 1000.0,
						static_cast<f64>(increment) / 1000.0,
						toGo, static_cast<f64>(m_moveOverhead) / 1000.0);
				else if (!limiter)
					limiter = std::make_unique<limit::InfiniteLimiter>();

				m_searcher.setLimiter(std::move(limiter));
				m_searcher.startSearch(m_pos, static_cast<i32>(depth));
			}
		}

		auto UciHandler::handleStop() -> void
		{
			if (!m_searcher.searching())
				std::cerr << "not searching" << std::endl;
			else m_searcher.stop();
		}

		//TODO refactor
		auto UciHandler::handleSetoption(const std::vector<std::string> &tokens) -> void
		{
			usize i = 1;

			for (; i < tokens.size() - 1 && tokens[i] != "name"; ++i) {}

			if (++i == tokens.size())
				return;

			bool nameEmpty = true;
			std::ostringstream name{};

			for (; i < tokens.size() && tokens[i] != "value"; ++i)
			{
				if (!nameEmpty)
					name << ' ';
				else nameEmpty = false;

				name << tokens[i];
			}

			if (++i == tokens.size())
				return;

			bool valueEmpty = true;
			std::ostringstream value{};

			for (; i < tokens.size(); ++i)
			{
				if (!valueEmpty)
					value << ' ';
				else valueEmpty = false;

				value << tokens[i];
			}

			auto nameStr = name.str();

			const auto valueStr = value.str();

			if (!nameEmpty)
			{
				std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(),
					[](auto c) { return std::tolower(c); });

				if (nameStr == "hash")
				{
					if (!valueEmpty)
					{
						if (const auto newHashSize = util::tryParseSize(valueStr))
							m_searcher.setHashSize(HashSizeRange.clamp(*newHashSize));
					}
				}
				else if (nameStr == "clear hash")
				{
					if (m_searcher.searching())
						std::cerr << "still searching" << std::endl;

					m_searcher.clearHash();
				}
				else if (nameStr == "threads")
				{
					if (m_searcher.searching())
						std::cerr << "still searching" << std::endl;

					if (!valueEmpty)
					{
						if (const auto newThreads = util::tryParseU32(valueStr))
							m_searcher.setThreads(search::ThreadCountRange.clamp(*newThreads));
					}
				}
				else if (nameStr == "uci_chess960")
				{
					if (!valueEmpty)
					{
						if (const auto newChess960 = util::tryParseBool(valueStr))
							opts::mutableOpts().chess960 = *newChess960;
					}
				}
				else if (nameStr == "uci_showwdl")
				{
					if (!valueEmpty)
					{
						if (const auto newShowWdl = util::tryParseBool(valueStr))
							opts::mutableOpts().showWdl = *newShowWdl;
					}
				}
				else if (nameStr == "move overhead")
				{
					if (!valueEmpty)
					{
						if (const auto newMoveOverhead = util::tryParseI32(valueStr))
							m_moveOverhead = limit::MoveOverheadRange.clamp(*newMoveOverhead);
					}
				}
				else if (nameStr == "syzygypath")
				{
					if (m_searcher.searching())
						std::cerr << "still searching" << std::endl;

					m_fathomInitialized = true;

					if (valueEmpty)
					{
						opts::mutableOpts().syzygyEnabled = false;
						tb_init("");
					}
					else
					{
						opts::mutableOpts().syzygyEnabled = valueStr != "<empty>";
						if (!tb_init(valueStr.c_str()))
							std::cerr << "failed to initialize Fathom" << std::endl;
					}
				}
				else if (nameStr == "syzygyprobedepth")
				{
					if (!valueEmpty)
					{
						if (const auto newSyzygyProbeDepth = util::tryParseI32(valueStr))
							opts::mutableOpts().syzygyProbeDepth = search::SyzygyProbeLimitRange.clamp(*newSyzygyProbeDepth);
					}
				}
				else if (nameStr == "syzygyprobelimit")
				{
					if (!valueEmpty)
					{
						if (const auto newSyzygyProbeLimit = util::tryParseI32(valueStr))
							opts::mutableOpts().syzygyProbeLimit = search::SyzygyProbeLimitRange.clamp(*newSyzygyProbeLimit);
					}
				}
				else if (nameStr == "evalfile")
				{
					if (m_searcher.searching())
						std::cerr << "still searching" << std::endl;

					if (!valueEmpty)
					{
						if (valueStr == "<internal>")
						{
							eval::loadDefaultNetwork();
							std::cout << "info string loaded embedded network "
								<< eval::defaultNetworkName() << std::endl;
						}
						else eval::loadNetwork(valueStr);
					}
				}
#if SP_TUNE_SEARCH
				else if (nameStr == "minaspdepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.minAspDepth, valueStr);
				}
				else if (nameStr == "maxaspreduction")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.maxAspReduction, valueStr);
				}
				else if (nameStr == "initialaspwindow")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.initialAspWindow, valueStr);
				}
				else if (nameStr == "maxaspwindow")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.maxAspWindow, valueStr);
				}
				else if (nameStr == "minnmpdepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.minNmpDepth, valueStr);
				}
				else if (nameStr == "nmpreductionbase")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.nmpReductionBase, valueStr);
				}
				else if (nameStr == "nmpreductiondepthscale")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.nmpReductionDepthScale, valueStr);
				}
				else if (nameStr == "nmpreductionevalscale")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.nmpReductionEvalScale, valueStr);
				}
				else if (nameStr == "maxnmpevalreduction")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.maxNmpEvalReduction, valueStr);
				}
				else if (nameStr == "minlmrdepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.minLmrDepth, valueStr);
				}
				else if (nameStr == "maxrfpdepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.maxRfpDepth, valueStr);
				}
				else if (nameStr == "rfpmargin")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.rfpMargin, valueStr);
				}
				else if (nameStr == "maxseepruningdepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.maxSeePruningDepth, valueStr);
				}
				else if (nameStr == "quietseethreshold")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.quietSeeThreshold, valueStr);
				}
				else if (nameStr == "noisyseethreshold")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.noisySeeThreshold, valueStr);
				}
				else if (nameStr == "minsingularitydepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.minSingularityDepth, valueStr);
				}
				else if (nameStr == "singularitydepthmargin")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.singularityDepthMargin, valueStr);
				}
				else if (nameStr == "singularitydepthscale")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.singularityDepthScale, valueStr);
				}
				else if (nameStr == "maxfpdepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.maxFpDepth, valueStr);
				}
				else if (nameStr == "fpmargin")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.fpMargin, valueStr);
				}
				else if (nameStr == "fpscale")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.fpScale, valueStr);
				}
				else if (nameStr == "miniirdepth")
				{
					if (!valueEmpty)
						util::tryParseI32(s_tunable.minIirDepth, valueStr);
				}
#endif
			}
		}

		auto UciHandler::handleD() -> void
		{
			std::cout << '\n';

			printBoard(std::cout, m_pos);
			std::cout << "\nFen: " << m_pos.toFen() << std::endl;

			std::ostringstream key{};
			key << std::hex << std::setw(16) << std::setfill('0') << m_pos.key();
			std::cout << "Key: " << key.str() << std::endl;

			std::cout << "Checkers:";

			auto checkers = m_pos.checkers();
			while (checkers)
			{
				std::cout << ' ' << squareToString(checkers.popLowestSquare());
			}

			std::cout << std::endl;

			eval::NnueState state{};
			state.reset(m_pos.boards());
			const auto staticEval = state.evaluate(m_pos.toMove());

			std::cout << "Static eval: ";
			printScore(std::cout, m_pos.toMove() == Color::Black ? -staticEval : staticEval);
			std::cout << std::endl;
		}

		auto UciHandler::handleEval() -> void
		{
			eval::NnueState state{};
			state.reset(m_pos.boards());
			printScore(std::cout, state.evaluate(m_pos.toMove()));
			std::cout << std::endl;
		}

		auto UciHandler::handleCheckers() -> void
		{
			std::cout << '\n';
			printBitboard(std::cout, m_pos.checkers());
		}

		auto UciHandler::handleRegen() -> void
		{
			m_pos.regen();
		}

		auto UciHandler::handleMoves() -> void
		{
			ScoredMoveList moves{};
			generateAll(moves, m_pos);

			for (u32 i = 0; i < moves.size(); ++i)
			{
				if (i > 0)
					std::cout << ' ';
				std::cout << moveToString(moves[i].move);
			}

			std::cout << std::endl;
		}

		auto UciHandler::handlePerft(const std::vector<std::string> &tokens) -> void
		{
			u32 depth = 6;

			if (tokens.size() > 1)
			{
				if (!util::tryParseU32(depth, tokens[1]))
				{
					std::cerr << "invalid depth " << tokens[1] << std::endl;
					return;
				}
			}

			perft(m_pos, static_cast<i32>(depth));
		}

		auto UciHandler::handleSplitperft(const std::vector<std::string> &tokens) -> void
		{
			u32 depth = 6;

			if (tokens.size() > 1)
			{
				if (!util::tryParseU32(depth, tokens[1]))
				{
					std::cerr << "invalid depth " << tokens[1] << std::endl;
					return;
				}
			}

			splitPerft(m_pos, static_cast<i32>(depth));
		}

		auto UciHandler::handleBench(const std::vector<std::string> &tokens) -> void
		{
			if (m_searcher.searching())
			{
				std::cerr << "already searching" << std::endl;
				return;
			}

			i32 depth = bench::DefaultBenchDepth;
			usize hash = 16;

			if (tokens.size() > 1)
			{
				if (const auto newDepth = util::tryParseU32(tokens[1]))
					depth = static_cast<i32>(*newDepth);
				else
				{
					std::cout << "info string invalid depth " << tokens[1] << std::endl;
					return;
				}
			}

			if (tokens.size() > 2)
			{
				if (const auto newThreads = util::tryParseU32(tokens[2]))
				{
					if (*newThreads > 1)
						std::cout << "info string multiple search threads not yet supported, using 1" << std::endl;
				}
				else
				{
					std::cout << "info string invalid thread count " << tokens[2] << std::endl;
					return;
				}
			}

			if (tokens.size() > 3)
			{
				if (const auto newHash = util::tryParseSize(tokens[3]))
					hash = static_cast<i32>(*newHash);
				else
				{
					std::cout << "info string invalid hash " << tokens[3] << std::endl;
					return;
				}
			}

			m_searcher.setHashSize(hash);
			std::cout << "info string set hash size to " << hash << std::endl;

			if (depth == 0)
				depth = 1;

			bench::run(m_searcher, depth);
		}

#ifndef NDEBUG
		auto UciHandler::handleVerify() -> void
		{
			if (m_pos.verify())
				std::cout << "info string boards and keys ok" << std::endl;
		}
#endif
	}

#if SP_TUNE_SEARCH
	namespace tunable
	{
		const tunable::TunableData &g_tunable = s_tunable;
	}
#endif

	namespace uci
	{
		auto run() -> i32
		{
			UciHandler handler{};
			return handler.run();
		}

		auto winRateModel(Score povScore, u32 ply) -> std::pair<i32, i32>
		{
			constexpr auto As = std::array {
				7.18898158, -41.53274687, 123.88236544, 182.20533084
			};
			constexpr auto Bs = std::array {
				1.17032511, 4.16373765, -41.01938757, 132.41029355
			};

			static_assert(NormalizationK == static_cast<i32>(std::reduce(As.begin(), As.end())));

			const auto m = std::min(240.0, static_cast<f64>(ply)) / 64.0;

			const auto a = (((As[0] * m + As[1]) * m + As[2]) * m) + As[3];
			const auto b = (((Bs[0] * m + Bs[1]) * m + Bs[2]) * m) + Bs[3];

			const auto x = std::clamp(static_cast<f64>(povScore), -4000.0, 4000.0);

			return {
				static_cast<i32>(std::round(1000.0 / (1.0 + std::exp((a - x) / b)))),
				static_cast<i32>(std::round(1000.0 / (1.0 + std::exp((a + x) / b))))
			};
		}

		auto moveToString(Move move) -> std::string
		{
			if (!move)
				return "0000";

			std::ostringstream str{};

			str << squareToString(move.src());

			const auto type = move.type();

			if (type != MoveType::Castling || g_opts.chess960)
			{
				str << squareToString(move.dst());
				if (type == MoveType::Promotion)
					str << basePieceToChar(move.target());
			}
			else
			{
				const auto dst = move.srcFile() < move.dstFile()
					? toSquare(move.srcRank(), 6)
					: toSquare(move.srcRank(), 2);
				str << squareToString(dst);
			}

			return str.str();
		}

#ifndef NDEBUG
		auto moveAndTypeToString(Move move) -> std::string
		{
			if (!move)
				return "0000";

			std::ostringstream str{};

			if (move.type() != MoveType::Standard)
			{
				switch (move.type())
				{
				case MoveType::Promotion: str << "p:"; break;
				case MoveType::Castling:  str << "c:"; break;
				case MoveType::EnPassant: str << "e:"; break;
				default: __builtin_unreachable();
				}
			}

			str << moveToString(move);

			return str.str();
		}
#endif
	}
}
