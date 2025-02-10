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

#include "../types.h"

#include <array>

#include "../core.h"
#include "tapered.h"
#include "../util/cemath.h"

namespace stormphrax::eval
{
#define S(Mg, Eg) TaperedScore{(Mg), (Eg)}

	namespace values
	{
		constexpr auto Pawn = S(89, 100);
		constexpr auto Knight = S(382, 331);
		constexpr auto Bishop = S(402, 362);
		constexpr auto Rook = S(512, 647);
		constexpr auto Queen = S(1109, 1246);

		constexpr auto King = S(0, 0);

		constexpr auto BaseValues = std::array {
			Pawn,
			Knight,
			Bishop,
			Rook,
			Queen,
			King,
			TaperedScore{}
		};

		constexpr auto Values = std::array {
			Pawn,
			Pawn,
			Knight,
			Knight,
			Bishop,
			Bishop,
			Rook,
			Rook,
			Queen,
			Queen,
			King,
			King,
			TaperedScore{}
		};
	}
#undef S

	namespace psqt
	{
		extern const std::array<std::array<TaperedScore, 64>, 12> g_pieceSquareTables;
	}

	inline auto pieceSquareValue(Piece piece, Square square)
	{
		return psqt::g_pieceSquareTables[static_cast<i32>(piece)][static_cast<i32>(square)];
	}

	constexpr auto pieceValue(PieceType piece)
	{
		return values::BaseValues[static_cast<i32>(piece)];
	}

	constexpr auto pieceValue(Piece piece)
	{
		return values::Values[static_cast<i32>(piece)];
	}

	constexpr auto Phase = std::array {
		0, 0, 1, 1, 2, 2, 2, 2, 4, 4, 0, 0,
	};

	struct MaterialState
	{
		static constexpr i32 MaxPhase = 24;

		TaperedScore material{};
		i32 phase{};

		constexpr auto subAdd(Piece piece, Square src, Square dst)
		{
			material -= pieceSquareValue(piece, src);
			material += pieceSquareValue(piece, dst);
		}

		constexpr auto add(Piece piece, Square square)
		{
			phase += Phase[static_cast<i32>(piece)];
			material += pieceSquareValue(piece, square);
		}

		constexpr auto sub(Piece piece, Square square)
		{
			phase -= Phase[static_cast<i32>(piece)];
			material -= pieceSquareValue(piece, square);
		}

		[[nodiscard]] inline auto interp(TaperedScore tapered) const
		{
			return util::ilerp<MaxPhase>(tapered.endgame(), tapered.midgame(), std::min(phase, MaxPhase));
		}

		[[nodiscard]] constexpr auto operator==(const MaterialState &other) const -> bool = default;
	};
}
