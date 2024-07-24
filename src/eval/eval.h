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

#include "../types.h"

#include <array>

#include "nnue.h"
#include "../position/position.h"
#include "../core.h"
#include "../correction.h"

namespace stormphrax::eval
{
	// black, white
	using Contempt = std::array<Score, 2>;

	template <bool RoughenAndCorrect = true>
	inline auto adjustEval(const Position &pos,
		const CorrectionHistoryTable *correction, i32 eval)
	{
		if constexpr (RoughenAndCorrect)
			eval = eval / 8 * 8 + 2 - static_cast<Score>(pos.key() % 4);

		eval = eval * (200 - pos.halfmove()) / 200;

		if constexpr (RoughenAndCorrect)
			eval = correction->correct(pos, eval);

		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}

	inline auto staticEval(const Position &pos, NnueState &nnueState, const Contempt &contempt = {})
	{
		auto eval = nnueState.evaluate(pos.bbs(), pos.kings(), pos.toMove());
		eval += contempt[static_cast<i32>(pos.toMove())];
		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}

	template <bool RoughenAndCorrect = true>
	inline auto adjustedStaticEval(const Position &pos, NnueState &nnueState,
		const CorrectionHistoryTable *correction, const Contempt &contempt = {})
	{
		const auto eval = staticEval(pos, nnueState, contempt);
		return adjustEval<RoughenAndCorrect>(pos, correction, eval);
	}

	inline auto staticEvalOnce(const Position &pos, const Contempt &contempt = {})
	{
		auto eval = NnueState::evaluateOnce(pos.bbs(), pos.kings(), pos.toMove());
		eval += contempt[static_cast<i32>(pos.toMove())];
		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}
}
