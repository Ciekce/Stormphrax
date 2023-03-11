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

namespace polaris::eval
{
#define S(Mg, Eg) TaperedScore{(Mg), (Eg)}

	namespace values
	{
		namespace current
		{
			constexpr auto Pawn = S(90, 100);
			constexpr auto Knight = S(340, 300);
			constexpr auto Bishop = S(350, 321);
			constexpr auto Rook = S(502, 574);
			constexpr auto Queen = S(1115, 1054);
		}

		constexpr auto King = S(std::numeric_limits<Score>::max() / 2, std::numeric_limits<Score>::max() / 2);

		constexpr auto Pawn = current::Pawn;
		constexpr auto Knight = current::Knight;
		constexpr auto Bishop = current::Bishop;
		constexpr auto Rook = current::Rook;
		constexpr auto Queen = current::Queen;

		constexpr std::array BaseValues
		{
			Pawn,
			Knight,
			Bishop,
			Rook,
			Queen,
			King,
			TaperedScore{}
		};

		constexpr std::array Values
		{
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

	namespace pst
	{
		extern const std::array<std::array<TaperedScore, 64>, 12> PieceSquareTables;
	}

	inline TaperedScore pieceSquareValue(Piece piece, Square square)
	{
		return pst::PieceSquareTables[static_cast<i32>(piece)][static_cast<i32>(square)];
	}

	constexpr auto pieceValue(BasePiece piece)
	{
		return values::BaseValues[static_cast<i32>(piece)];
	}

	constexpr auto pieceValue(Piece piece)
	{
		return values::Values[static_cast<i32>(piece)];
	}
}
