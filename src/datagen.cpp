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

#include "datagen.h"

#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
//TODO
#endif

#include "limit/limit.h"
#include "search.h"
#include "movegen.h"
#include "util/rng.h"
#include "opts.h"
#include "util/timer.h"

// abandon hope all ye who enter here
// my search was not written with this in mind

//TODO refactor search so this can be less horrible

namespace stormphrax::datagen
{
	namespace
	{
		std::atomic_bool s_stop{false};

		auto initCtrlCHandler()
		{
#ifdef _WIN32
			if (!SetConsoleCtrlHandler([](DWORD dwCtrlType) -> BOOL
				{
					s_stop.store(true, std::memory_order::seq_cst);
					return TRUE;
				}, TRUE))
				std::cerr << "failed to set ctrl+c handler" << std::endl;
#else
			//TODO
#endif
		}

		class DatagenNodeLimiter final : public limit::ISearchLimiter
		{
		public:
			explicit DatagenNodeLimiter(u32 threadId) : m_threadId{threadId} {}
			~DatagenNodeLimiter() final = default;

			[[nodiscard]] auto stop(const search::SearchData &data, bool allowSoftTimeout) const -> bool final
			{
				if (data.nodes >= m_hardNodeLimit)
				{
					std::cout << "thread " << m_threadId << ": stopping search after "
						<< data.nodes << " nodes (limit: " << m_hardNodeLimit << ")" << std::endl;
					return true;
				}

				return allowSoftTimeout && data.nodes >= m_softNodeLimit;
			}

			inline auto setSoftNodeLimit(usize nodes)
			{
				m_softNodeLimit = nodes;
			}

			inline auto setHardNodeLimit(usize nodes)
			{
				m_hardNodeLimit = nodes;
			}

		private:
			u32 m_threadId;
			usize m_softNodeLimit{};
			usize m_hardNodeLimit{};
		};

		constexpr usize VerificationHardNodeLimit = 25165814;

		constexpr usize DatagenSoftNodeLimit = 5000;
		constexpr usize DatagenHardNodeLimit = 8388608;

		constexpr Score VerificationScoreLimit = 1000;

		constexpr Score WinAdjMinScore = 2500;
		constexpr Score DrawAdjMaxScore = 10;

		constexpr u32 WinAdjMaxPlies = 5;
		constexpr u32 DrawAdjMaxPlies = 10;

		constexpr i32 ReportInterval = 1024;

		enum class Outcome
		{
			WhiteWin = 0,
			Draw,
			WhiteLoss
		};

		constexpr auto OutcomeStrings = std::array {
			"1.0",
			"0.5",
			"0.0"
		};

		auto runThread(u32 id, u32 games, u64 seed, std::mutex &outputMutex, std::ofstream &out)
		{
			Jsf64Rng rng{seed};

			auto limiterPtr = std::make_unique<DatagenNodeLimiter>(id);
			auto &limiter = *limiterPtr;

			search::Searcher searcher{};
			searcher.setLimiter(std::move(limiterPtr));

			auto thread = std::make_unique<search::ThreadData>();

			const auto resetSearch = [&searcher, &thread]()
			{
				searcher.newGame();
				thread->search = search::SearchData{};
				std::fill(thread->stack.begin(), thread->stack.end(), search::SearchStackEntry{});
				thread->history.clear();
			};

			std::vector<std::pair<std::string, Score>> positions{};
			positions.reserve(256);

			const auto startTime = util::g_timer.time();

			usize totalPositions{};

			for (i32 game = 0; game < games && !s_stop.load(std::memory_order::seq_cst); ++game)
			{
				positions.clear();

				resetSearch();

				const auto dfrcIndex = rng.nextU32(960 * 960);
				thread->pos.resetFromDfrcIndex(dfrcIndex);

				const auto moveCount = 8 + (rng.nextU32() >> 31);

				bool legalFound = false;

				for (i32 i = 0; i < moveCount; ++i)
				{
					ScoredMoveList moves{};
					generateAll(moves, thread->pos);

					std::shuffle(moves.begin(), moves.end(), rng);

					legalFound = false;

					for (const auto [move, score] : moves)
					{
						if (thread->pos.applyMoveUnchecked<false>(move, nullptr))
						{
							legalFound = true;
							break;
						}
						else thread->pos.popMove<false>(nullptr);
					}

					if (!legalFound)
						break;
				}

				if (!legalFound)
				{
					// this game was useless, don't count it
					--game;
					continue;
				}

				thread->nnueState.reset(thread->pos.boards());

				thread->maxDepth = 10;
				limiter.setSoftNodeLimit(std::numeric_limits<usize>::max());
				limiter.setHardNodeLimit(VerificationHardNodeLimit);

				const auto [firstMove, firstScore] = searcher.runDatagenSearch(*thread);

				thread->maxDepth = search::MaxDepth;
				limiter.setSoftNodeLimit(DatagenSoftNodeLimit);
				limiter.setHardNodeLimit(DatagenHardNodeLimit);

				if (std::abs(firstScore) > VerificationScoreLimit)
				{
					--game;
					continue;
				}

				resetSearch();

				u32 winPlies{};
				u32 lossPlies{};
				u32 drawPlies{};

				Outcome outcome;

				while (true)
				{
					const auto [move, score] = searcher.runDatagenSearch(*thread);
					thread->search = search::SearchData{};

					if (!move)
					{
						outcome = thread->pos.isCheck()
							? thread->pos.toMove() == Color::Black ? Outcome::WhiteWin : Outcome::WhiteLoss
							: Outcome::Draw; // stalemate
						break;
					}

					if (score > WinAdjMinScore)
					{
						++winPlies;
						lossPlies = 0;
						drawPlies = 0;
					}
					else if (score < -WinAdjMinScore)
					{
						winPlies = 0;
						++lossPlies;
						drawPlies = 0;
					}
					else if (std::abs(score) < DrawAdjMaxScore)
					{
						winPlies = 0;
						lossPlies = 0;
						++drawPlies;
					}
					else
					{
						winPlies = 0;
						lossPlies = 0;
						drawPlies = 0;
					}

					if (winPlies >= WinAdjMaxPlies)
					{
						outcome = Outcome::WhiteWin;
						break;
					}
					else if (lossPlies >= WinAdjMaxPlies)
					{
						outcome = Outcome::WhiteLoss;
						break;
					}
					else if (drawPlies >= DrawAdjMaxPlies)
					{
						outcome = Outcome::Draw;
						break;
					}

					const bool noisy = thread->pos.isNoisy(move);

					thread->pos.applyMoveUnchecked(move, &thread->nnueState);

					if (!noisy
						&& !thread->pos.isCheck()
						&& std::abs(score) < ScoreWin)
					{
						positions.emplace_back(thread->pos.toFen(), score);
					}
				}

				const auto &outcomeStr = OutcomeStrings[static_cast<i32>(outcome)];

				{
					std::unique_lock lock{outputMutex};

					for (const auto &[fen, score]: positions)
					{
						out << fen << " | " << score << " | " << outcomeStr << "\n";
					}

					out.flush();
				}

				totalPositions += positions.size();

				if (game == games - 1
					|| ((game + 1) % ReportInterval) == 0
					|| s_stop.load(std::memory_order::seq_cst))
				{
					const auto time = util::g_timer.time() - startTime;
					std::cout << "thread " << id << ": wrote " << totalPositions << " positions from "
						<< (game + 1) << " games in " << time << " sec ("
						<< (static_cast<f64>(totalPositions) / time) << " positions/sec)" << std::endl;
				}
			}
		}
	}

	auto run(const std::string &output, i32 threads, u32 games) -> i32
	{
		opts::mutableOpts().chess960 = true;

		const auto baseSeed = static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()).count());

		std::mutex outputMutex{};
		std::ofstream out{output, std::ios::binary | std::ios::app};

		if (!out)
		{
			std::cerr << "failed to open output file " << output << std::endl;
			return 1;
		}

		initCtrlCHandler();

		std::vector<std::thread> theThreads{};
		theThreads.reserve(threads);

		if (games == UnlimitedGames)
			std::cout << "generating on " << threads << " threads" << std::endl;
		else std::cout << "generating " << games << " games each on " << threads << " threads" << std::endl;

		for (u32 i = 0; i < threads; ++i)
		{
			theThreads.emplace_back([games, baseSeed, i, &outputMutex, &out]()
			{
				runThread(i, games, baseSeed + i, outputMutex, out);
			});
		}

		for (auto &thread : theThreads)
		{
			thread.join();
		}

		std::cout << "done" << std::endl;

		return 0;
	}
}
