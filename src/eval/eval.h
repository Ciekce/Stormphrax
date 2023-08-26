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

#pragma once

#include "../types.h"

#include "nnue.h"
#include "../position/position.h"

namespace stormphrax::eval
{
	inline auto scaleEval(const Position &pos, i32 eval)
	{
		return eval * (200 - pos.halfmove()) / 200;
	}

	inline auto staticEval(const Position &pos, NnueState &nnueState)
	{
		const auto nnueEval = nnueState.evaluate(pos.toMove());
		return scaleEval(pos, nnueEval);
	}

	inline auto staticEvalOnce(const Position &pos)
	{
		const auto nnueEval = NnueState::evaluateOnce(pos.boards(), pos.toMove());
		return scaleEval(pos, nnueEval);
	}
}
