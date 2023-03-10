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

#include "types.h"
#include "core.h"
#include "bitboard.h"
#include "attacks/util.h"

namespace polaris
{
	consteval std::array<std::array<Bitboard, 64>, 64> generateRays()
	{
		std::array<std::array<Bitboard, 64>, 64> dst{};

		for (i32 from = 0; from < 64; ++from)
		{
			const auto srcSquare = static_cast<Square>(from);
			const auto srcMask = squareBit(srcSquare);

			const auto   rookAttacks = attacks::EmptyBoardRooks  [from];
			const auto bishopAttacks = attacks::EmptyBoardBishops[from];

			for (i32 to = 0; to < 64; ++to)
			{
				if (from == to)
					continue;

				const auto dstSquare = static_cast<Square>(to);
				const auto dstMask = squareBit(dstSquare);

				if (rookAttacks[dstSquare])
					dst[from][to] = (attacks::genRookAttacks(srcSquare, dstMask)
						& attacks::genRookAttacks(dstSquare, srcMask));
				else if (bishopAttacks[dstSquare])
					dst[from][to] = (attacks::genBishopAttacks(srcSquare, dstMask)
						& attacks::genBishopAttacks(dstSquare, srcMask));
			}
		}

		return dst;
	}

	constexpr auto Rays = generateRays();

	constexpr Bitboard rayTo(Square src, Square dst)
	{
		return Rays[static_cast<i32>(src)][static_cast<i32>(dst)];
	}
}
