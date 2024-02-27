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

#include <string>
#include <span>

#include "core.h"
#include "move.h"
#include "tunable.h"

namespace stormphrax::uci
{
	constexpr auto ContemptRange = util::Range<i32>{-1000, 1000};

	auto run() -> i32;

	[[nodiscard]] auto moveToString(Move move) -> std::string;

#if SP_EXTERNAL_TUNE
	auto printWfTuningParams(std::span<const std::string> params) -> void;
	auto printCttTuningParams(std::span<const std::string> params) -> void;
	auto printObTuningParams(std::span<const std::string> params) -> void;
#endif

#ifndef NDEBUG
	[[nodiscard]] auto moveAndTypeToString(Move move) -> std::string;
#endif
}
