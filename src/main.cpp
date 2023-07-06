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
#include "bench.h"
#include "datagen.h"
#include "util/parse.h"

using namespace stormphrax;

auto main(i32 argc, const char *argv[]) -> i32
{
	if (argc > 1)
	{
		const std::string mode{argv[1]};

		if (mode == "bench")
		{
			search::Searcher searcher{16};
			bench::run(searcher);

			return 0;
		}
		else if (mode == "datagen")
		{
			if (argc < 4)
			{
				std::cerr << "usage: " << argv[0] << " datagen <path> <games per thread> [threads]" << std::endl;
				return 1;
			}

			u32 games{};
			if (!util::tryParseU32(games, argv[3]))
			{
				std::cerr << "invalid number of games " << argv[3] << std::endl;
				std::cerr << "usage: " << argv[0] << " datagen <path> <games per thread> [threads]" << std::endl;
				return 1;
			}

			u32 threads = 1;
			if (argc > 4 && !util::tryParseU32(threads, argv[4]))
			{
				std::cerr << "invalid number of threads " << argv[4] << std::endl;
				std::cerr << "usage: " << argv[0] << " datagen <path> <games per thread> [threads]" << std::endl;
				return 1;
			}

			return datagen::run(argv[2], static_cast<i32>(games), static_cast<i32>(threads));
		}
	}

	return uci::run();
}
