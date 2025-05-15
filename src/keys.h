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

#include <array>

#include "core.h"
#include "util/rng.h"
#include "util/cemath.h"

namespace stormphrax::keys
{
	constexpr i32 HalfmoveOffset = 8;
	constexpr i32 HalfmoveStep = 8;

	namespace sizes
	{
		constexpr usize PieceSquares = 12 * 64;
		constexpr usize Color = 1;
		constexpr usize Castling = 16;
		constexpr usize EnPassant = 8;
		constexpr usize Halfmove = util::ceilDiv(100 - HalfmoveOffset + 1, HalfmoveStep);

		constexpr auto Total = PieceSquares + Color + Castling + EnPassant + Halfmove;
	}

	namespace offsets
	{
		constexpr usize PieceSquares = 0;
		constexpr auto Color = PieceSquares + sizes::PieceSquares;
		constexpr auto Castling = Color + sizes::Color;
		constexpr auto EnPassant = Castling + sizes::Castling;
		constexpr auto Halfmove = EnPassant + sizes::EnPassant;
	}

	constexpr auto Keys = []
	{
		constexpr auto Seed = U64(0xD06C659954EC904A);

		std::array<u64, sizes::Total> keys{};

		util::rng::Jsf64Rng rng{Seed};

		for (auto &key : keys)
		{
			key = rng.nextU64();
		}

		return keys;
	}();

	inline auto pieceSquare(Piece piece, Square square) -> u64
	{
		if (piece == Piece::None || square == Square::None)
			return 0;

		return Keys[offsets::PieceSquares + static_cast<usize>(square) * 12 + static_cast<usize>(piece)];
	}

	// for flipping
	inline auto color()
	{
		return Keys[offsets::Color];
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

		if (castlingRooks.black().kingside  != Square::None)
			flags |= BlackShort;
		if (castlingRooks.black().queenside != Square::None)
			flags |= BlackLong;
		if (castlingRooks.white().kingside  != Square::None)
			flags |= WhiteShort;
		if (castlingRooks.white().queenside != Square::None)
			flags |= WhiteLong;

		return Keys[offsets::Castling + flags];
	}

	inline auto enPassant(u32 file)
	{
		return Keys[offsets::EnPassant + file];
	}

	inline auto enPassant(Square square) -> u64
	{
		if (square == Square::None)
			return 0;

		return Keys[offsets::EnPassant + squareFile(square)];
	}

	inline auto halfmove(u16 halfmoveClock) -> u64
	{
		const auto keyIdx = std::clamp(halfmoveClock - HalfmoveOffset, 0, 100) / HalfmoveStep;
		return Keys[offsets::Halfmove + keyIdx];
	}
}
