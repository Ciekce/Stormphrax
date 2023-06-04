/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"

#include <string>

#include "core.h"
#include "move.h"
#include "tunable.h"

namespace polaris::uci
{
	constexpr Score NormalizationK = 91;

	i32 run();

	[[nodiscard]] i32 winRateModel(Score povScore, u32 ply);

	[[nodiscard]] std::string moveToString(Move move);

#ifndef NDEBUG
	[[nodiscard]] std::string moveAndTypeToString(Move move);
#endif
}
