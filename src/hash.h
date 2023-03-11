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

#include <array>

#include "core.h"

namespace polaris::hash
{
	namespace sizes
	{
		constexpr size_t PieceSquares = 12 * 64;
		constexpr size_t Color = 1;
		constexpr size_t Castling = 16;
		constexpr size_t EnPassant = 8;

		constexpr size_t Total = PieceSquares + Color + Castling + EnPassant;
	}

	namespace offsets
	{
		constexpr size_t PieceSquares = 0;
		constexpr size_t Color = PieceSquares + sizes::PieceSquares;
		constexpr size_t Castling = Color + sizes::Color;
		constexpr size_t EnPassant = Castling + sizes::Castling;
	}

	extern const std::array<u64, sizes::Total> Hashes;

	inline u64 pieceSquare(Piece piece, Square square)
	{
		if (piece == Piece::None || square == Square::None)
			return 0;

		return Hashes[offsets::PieceSquares + static_cast<size_t>(square) * 12 + static_cast<size_t>(piece)];
	}

	// for flipping
	inline u64 color()
	{
		return Hashes[offsets::Color];
	}

	inline u64 color(Color c)
	{
		return c == Color::White ? 0 : color();
	}

	inline u64 castling(PositionFlags flags)
	{
		return Hashes[offsets::Castling + static_cast<size_t>(flags & PositionFlags::AllCastling)];
	}

	inline u64 enPassant(u32 file)
	{
		return Hashes[offsets::EnPassant + file];
	}

	inline u64 enPassant(Square square)
	{
		if (square == Square::None)
			return 0;

		return Hashes[offsets::EnPassant + squareFile(square)];
	}
}
