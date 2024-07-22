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

#pragma once

#include "types.h"

#include <cmath>
#include <utility>

#include "core.h"

namespace stormphrax::wdl
{
	[[nodiscard]] auto wdlParams(u32 ply) -> std::pair<f64, f64>;
	[[nodiscard]] auto wdlModel(Score povScore, u32 ply) -> std::pair<i32, i32>; // [win, loss]

	inline auto normalizeScore(Score score)
	{
		return score;
	}

	// for datagen only - only gives a 50% winrate at move 32
	inline auto normalizeScoreMove32(Score score)
	{
		return score;
	}

	inline auto unnormalizeScoreMove32(Score score)
	{
		return score;
	}
}
