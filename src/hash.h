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
		constexpr usize PieceSquares = 12 * 64;
		constexpr usize Color = 1;
		constexpr usize Castling = 16;
		constexpr usize EnPassant = 8;

		constexpr auto Total = PieceSquares + Color + Castling + EnPassant;
	}

	namespace offsets
	{
		constexpr usize PieceSquares = 0;
		constexpr auto Color = PieceSquares + sizes::PieceSquares;
		constexpr auto Castling = Color + sizes::Color;
		constexpr auto EnPassant = Castling + sizes::Castling;
	}

	extern const std::array<u64, sizes::Total> Hashes;

	inline auto pieceSquare(Piece piece, Square square) -> u64
	{
		if (piece == Piece::None || square == Square::None)
			return 0;

		return Hashes[offsets::PieceSquares + static_cast<usize>(square) * 12 + static_cast<usize>(piece)];
	}

	// for flipping
	inline auto color()
	{
		return Hashes[offsets::Color];
	}

	inline auto color(Color c)
	{
		return c == Color::White ? 0 : color();
	}

	inline auto castling(const CastlingRooks &castlingRooks)
	{
		constexpr usize BlackShort = 0x01;
		constexpr usize BlackLong  = 0x02;
		constexpr usize WhiteShort = 0x04;
		constexpr usize WhiteLong  = 0x08;

		usize flags{};

		if (castlingRooks.blackShort != Square::None)
			flags |= BlackShort;
		if (castlingRooks.blackLong  != Square::None)
			flags |= BlackLong;
		if (castlingRooks.whiteShort != Square::None)
			flags |= WhiteShort;
		if (castlingRooks.whiteLong  != Square::None)
			flags |= WhiteLong;

		return Hashes[offsets::Castling + flags];
	}

	inline auto enPassant(u32 file)
	{
		return Hashes[offsets::EnPassant + file];
	}

	inline auto enPassant(Square square) -> u64
	{
		if (square == Square::None)
			return 0;

		return Hashes[offsets::EnPassant + squareFile(square)];
	}
}
