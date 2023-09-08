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

#include <array>

#include "nnue.h"
#include "../position/position.h"
#include "../see.h"

namespace stormphrax::eval
{
	// black, white
	using Contempt = std::array<Score, 2>;

	inline auto materialScale(const Position &pos)
	{
		const auto &boards = pos.boards();
		return 22400
				+ boards.knights().popcount() * see::values::Knight
				+ boards.bishops().popcount() * see::values::Bishop
				+ boards.  rooks().popcount() * see::values::Rook
				+ boards. queens().popcount() * see::values::Queen;
	}

	inline auto scaleEval(const Position &pos, i32 eval)
	{
		eval = eval * materialScale(pos) / 32768;
		eval = eval * (200 - pos.halfmove()) / 200;
		return eval;
	}

	inline auto staticEval(const Position &pos, NnueState &nnueState, const Contempt &contempt = {})
	{
		const auto nnueEval = nnueState.evaluate(pos.toMove());
		return scaleEval(pos, nnueEval) + contempt[static_cast<i32>(pos.toMove())];
	}

	inline auto staticEvalOnce(const Position &pos, const Contempt &contempt = {})
	{
		const auto nnueEval = NnueState::evaluateOnce(pos.boards(), pos.toMove());
		return scaleEval(pos, nnueEval) + contempt[static_cast<i32>(pos.toMove())];
	}
}
