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

#include "perft.h"

#include <iostream>

#include "movegen.h"
#include "uci.h"
#include "util/timer.h"

namespace stormphrax
{
	namespace
	{
		auto doPerft(Position &pos, i32 depth) -> usize
		{
			if (depth == 0)
				return 1;

			--depth;

			ScoredMoveList moves{};
			generateAll(moves, pos);

			usize total{};

			for (const auto [move, score] : moves)
			{
				if (!pos.isLegal(move))
					continue;

				const auto guard = pos.applyMove<false>(move, nullptr);

				total += depth == 0 ? 1 : doPerft(pos, depth);
			}

			return total;
		}
	}

	auto perft(Position &pos, i32 depth) -> void
	{
		std::cout << doPerft(pos, depth) << std::endl;
	}

	auto splitPerft(Position &pos, i32 depth) -> void
	{
		--depth;

		const auto start = util::g_timer.time();

		ScoredMoveList moves{};
		generateAll(moves, pos);

		usize total{};

		for (const auto [move, score] : moves)
		{
			if (!pos.isLegal(move))
				continue;

			const auto guard = pos.applyMove<false>(move, nullptr);

			const auto value = doPerft(pos, depth);

			total += value;
			std::cout << uci::moveToString(move) << '\t' << value << '\n';
		}

		const auto time = util::g_timer.time() - start;
		const auto nps = static_cast<usize>(static_cast<f64>(total) / time);

		std::cout << "\ntotal " << total << '\n';
		std::cout << nps << " nps" << std::endl;
	}
}
