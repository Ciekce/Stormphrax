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
#include <cassert>

#include "../core.h"
#include "../bitboard.h"
#include "util.h"
#include "../util/bits.h"

#if SP_HAS_BMI2
#include "bmi2/attacks.h"
#else
#include "black_magic/attacks.h"
#endif

namespace stormphrax::attacks
{
	constexpr auto KnightAttacks = []
	{
		std::array<Bitboard, 64> dst{};

		for (usize i = 0; i < dst.size(); ++i)
		{
			const auto bit = Bitboard::fromSquare(Square::fromRaw(i));

			auto &attacks = dst[i];

			attacks |= bit.shiftUpUpLeft();
			attacks |= bit.shiftUpUpRight();
			attacks |= bit.shiftUpLeftLeft();
			attacks |= bit.shiftUpRightRight();
			attacks |= bit.shiftDownLeftLeft();
			attacks |= bit.shiftDownRightRight();
			attacks |= bit.shiftDownDownLeft();
			attacks |= bit.shiftDownDownRight();
		}

		return dst;
	}();

	constexpr auto KingAttacks = []
	{
		std::array<Bitboard, 64> dst{};

		for (usize i = 0; i < dst.size(); ++i)
		{
			const auto bit = Bitboard::fromSquare(Square::fromRaw(i));

			auto &attacks = dst[i];

			attacks |= bit.shiftUp();
			attacks |= bit.shiftDown();
			attacks |= bit.shiftLeft();
			attacks |= bit.shiftRight();
			attacks |= bit.shiftUpLeft();
			attacks |= bit.shiftUpRight();
			attacks |= bit.shiftDownLeft();
			attacks |= bit.shiftDownRight();
		}

		return dst;
	}();

	consteval auto generatePawnAttacks(Color us)
	{
		std::array<Bitboard, 64> dst{};

		for (usize i = 0; i < dst.size(); ++i)
		{
			const auto bit = Bitboard::fromSquare(Square::fromRaw(i));

			dst[i] |= bit.shiftUpLeftRelative(us);
			dst[i] |= bit.shiftUpRightRelative(us);
		}

		return dst;
	}

	constexpr auto BlackPawnAttacks = generatePawnAttacks(colors::Black);
	constexpr auto WhitePawnAttacks = generatePawnAttacks(colors::White);

	constexpr auto getKnightAttacks(Square src)
	{
		return KnightAttacks[src.idx()];
	}

	constexpr auto getKingAttacks(Square src)
	{
		return KingAttacks[src.idx()];
	}

	constexpr auto getPawnAttacks(Square src, Color color)
	{
		const auto &attacks = color == colors::White ? WhitePawnAttacks : BlackPawnAttacks;
		return attacks[src.idx()];
	}

	inline auto getQueenAttacks(Square src, Bitboard occupancy)
	{
		return getRookAttacks(src, occupancy)
			| getBishopAttacks(src, occupancy);
	}

	inline auto getNonPawnPieceAttacks(PieceType piece, Square src, Bitboard occupancy = Bitboard{})
	{
		assert(piece != piece_types::None);
		assert(piece != piece_types::Pawn);

		switch (piece.raw())
		{
		case piece_types::Knight.raw(): return getKnightAttacks(src);
		case piece_types::Bishop.raw(): return getBishopAttacks(src, occupancy);
		case piece_types::  Rook.raw(): return getRookAttacks(src, occupancy);
		case piece_types:: Queen.raw(): return getQueenAttacks(src, occupancy);
		case piece_types::  King.raw(): return getKingAttacks(src);
		default: __builtin_unreachable();
		}
	}
}
