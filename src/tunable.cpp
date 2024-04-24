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

#include "tunable.h"

#include <cmath>

namespace stormphrax::tunable
{
	std::array<std::array<i32, 256>, 256> g_lmrTable{};

	auto updateLmrTable() -> void
	{
		const auto base = static_cast<f64>(lmrBase()) / 100.0;
		const auto divisor = static_cast<f64>(lmrDivisor()) / 100.0;

		for (i32 depth = 1; depth < 256; ++depth)
		{
			for (i32 moves = 1; moves < 256; ++moves)
			{
				g_lmrTable[depth][moves] = static_cast<i32>(
					base + std::log(static_cast<f64>(depth)) * std::log(static_cast<f64>(moves)) / divisor
				);
			}
		}
	}

	std::array<std::array<i32, 16>, 2> g_lmpTable{};

	auto updateLmpTable() -> void
	{
		const auto base = lmpBaseMoves();

		for (i32 improving = 0; improving < 2; ++improving)
		{
			for (i32 depth = 0; depth < 16; ++depth)
			{
				g_lmpTable[improving][depth] = (base + depth * depth) / (2 - improving);
			}
		}
	}

	auto init() -> void
	{
		updateLmrTable();
		updateLmpTable();
	}
}
