/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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
#include "bench.h"
#include "datagen/datagen.h"
#include "util/parse.h"
#include "eval/nnue.h"
#include "tunable.h"
#include "cuckoo.h"
#include "util/ctrlc.h"

#if SP_EXTERNAL_TUNE
#include "util/split.h"
#endif

using namespace stormphrax;

auto main(i32 argc, const char *argv[]) -> i32
{
	util::signal::init();

	tunable::init();
	cuckoo::init();

	eval::loadDefaultNetwork();

	if (argc > 1)
	{
		const std::string mode{argv[1]};

		if (mode == "bench")
		{
			search::Searcher searcher{bench::DefaultBenchTtSize};
			bench::run(searcher);

			return 0;
		}
		else if (mode == "datagen")
		{
			const auto printUsage = [&]()
			{
				std::cerr << "usage: " << argv[0]
					<< " datagen <marlinformat/viriformat/fen> <standard/dfrc> <path> [threads] [game limit per thread]"
					<< std::endl;
			};

			if (argc < 5)
			{
				printUsage();
				return 1;
			}

			bool dfrc = false;

			if (std::string{argv[3]} == "dfrc")
				dfrc = true;
			else if (std::string{argv[3]} != "standard")
			{
				std::cerr << "invalid variant " << argv[3] << std::endl;
				printUsage();
				return 1;
			}

			u32 threads = 1;
			if (argc > 5 && !util::tryParseU32(threads, argv[5]))
			{
				std::cerr << "invalid number of threads " << argv[5] << std::endl;
				printUsage();
				return 1;
			}

			auto games = datagen::UnlimitedGames;
			if (argc > 6 && !util::tryParseU32(games, argv[6]))
			{
				std::cerr << "invalid number of games " << argv[6] << std::endl;
				printUsage();
				return 1;
			}

			return datagen::run(printUsage, argv[2], dfrc, argv[4], static_cast<i32>(threads), games);
		}
#if SP_EXTERNAL_TUNE
		else if (mode == "printwf"
			|| mode == "printctt"
			|| mode == "printob")
		{
			if (argc == 2)
				return 0;

			const auto params = split::split(argv[2], ',');

			if (mode == "printwf")
				uci::printWfTuningParams(params);
			else if (mode == "printctt")
				uci::printCttTuningParams(params);
			else if (mode == "printob")
				uci::printObTuningParams(params);

			return 0;
		}
#endif
	}

	return uci::run();
}
