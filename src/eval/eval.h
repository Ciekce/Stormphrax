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
#include "../see.h"

namespace stormphrax::eval
{
	// black, white
	using Contempt = std::array<Score, 2>;

	template <bool Correct = true>
	inline auto adjustEval(const Position &pos, std::span<search::PlayedMove> moves,
		i32 ply, const CorrectionHistoryTable *correction, i32 eval, i32 *corrDelta = nullptr)
	{
		eval = eval * (200 - pos.halfmove()) / 200;

		if constexpr (Correct)
		{
			const auto corrected = correction->correct(pos, moves, ply, eval);

			if (corrDelta)
				*corrDelta = std::abs(eval - corrected);

			eval = corrected;
		}

		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}

	template <bool Scale>
	inline auto adjustStatic(const Position &pos, const Contempt &contempt, Score eval)
	{
		if constexpr (Scale)
		{
			const auto bbs = pos.bbs();

			const auto npMaterial
				= see::values::Knight * bbs.knights().popcount()
				+ see::values::Bishop * bbs.bishops().popcount()
				+ see::values::Rook   * bbs.rooks  ().popcount()
				+ see::values::Queen  * bbs.queens ().popcount();

			eval = eval * (26500 + npMaterial) / 32768;
		}

		eval += contempt[static_cast<i32>(pos.toMove())];

		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}

	template <bool Scale = true>
	inline auto staticEval(const Position &pos, NnueState &nnueState, const Contempt &contempt = {})
	{
		auto eval = nnueState.evaluate(pos.bbs(), pos.kings(), pos.toMove());
		return adjustStatic<Scale>(pos, contempt, eval);
	}

	template <bool Correct = true>
	inline auto adjustedStaticEval(const Position &pos,
		std::span<search::PlayedMove> moves, i32 ply, NnueState &nnueState,
		const CorrectionHistoryTable *correction, const Contempt &contempt = {})
	{
		const auto eval = staticEval(pos, nnueState, contempt);
		return adjustEval<Correct>(pos, moves, ply, correction, eval);
	}

	template <bool Scale = true>
	inline auto staticEvalOnce(const Position &pos, const Contempt &contempt = {})
	{
		auto eval = NnueState::evaluateOnce(pos.bbs(), pos.kings(), pos.toMove());
		return adjustStatic<Scale>(pos, contempt, eval);
	}
}
