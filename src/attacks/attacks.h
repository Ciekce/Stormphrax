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

#include <array>

#include "../types.h"
#include "../core.h"
#include "../bitboard.h"
#include "util.h"
#include "../util/bits.h"

#if PS_HAS_BMI2
#include "bmi2/attacks.h"
#else
#include "black_magic/attacks.h"
#endif

namespace polaris::attacks
{
	constexpr std::array<Bitboard, 64> generateKnightAttacks()
	{
		std::array<Bitboard, 64> dst{};

		for (size_t i = 0; i < dst.size(); ++i)
		{
			const auto bit = Bitboard::fromSquare(static_cast<Square>(i));

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
	}

	constexpr std::array<Bitboard, 64> generateKingAttacks()
	{
		std::array<Bitboard, 64> dst{};

		for (size_t i = 0; i < dst.size(); ++i)
		{
			const auto bit = Bitboard::fromSquare(static_cast<Square>(i));

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
	}

	template <Color Us>
	constexpr std::array<Bitboard, 64> generatePawnAttacks()
	{
		std::array<Bitboard, 64> dst{};

		for (size_t i = 0; i < dst.size(); ++i)
		{
			const auto bit = Bitboard::fromSquare(static_cast<Square>(i));

			dst[i] |= bit.shiftUpLeftRelative<Us>();
			dst[i] |= bit.shiftUpRightRelative<Us>();
		}

		return dst;
	}

	constexpr auto KnightAttacks = generateKnightAttacks();
	constexpr auto   KingAttacks = generateKingAttacks();

	constexpr auto BlackPawnAttacks = generatePawnAttacks<Color::Black>();
	constexpr auto WhitePawnAttacks = generatePawnAttacks<Color::White>();

	constexpr Bitboard getKnightAttacks(Square src)
	{
		return KnightAttacks[static_cast<size_t>(src)];
	}

	constexpr Bitboard getKingAttacks(Square src)
	{
		return KingAttacks[static_cast<size_t>(src)];
	}

	constexpr Bitboard getPawnAttacks(Square src, Color color)
	{
		const auto &attacks = color == Color::White ? WhitePawnAttacks : BlackPawnAttacks;
		return attacks[static_cast<size_t>(src)];
	}

	inline Bitboard getQueenAttacks(Square src, Bitboard occupancy)
	{
		return getRookAttacks(src, occupancy)
			| getBishopAttacks(src, occupancy);
	}
}
