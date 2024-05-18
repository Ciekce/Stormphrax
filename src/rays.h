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
#include "bitboard.h"
#include "attacks/util.h"
#include "util/multi_array.h"

namespace stormphrax
{
	consteval auto generateBetweenRays()
	{
		util::MultiArray<Bitboard, 64, 64> dst{};

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
					dst[from][to]
						= attacks::genRookAttacks(srcSquare, dstMask)
						& attacks::genRookAttacks(dstSquare, srcMask);
				else if (bishopAttacks[dstSquare])
					dst[from][to]
						= attacks::genBishopAttacks(srcSquare, dstMask)
						& attacks::genBishopAttacks(dstSquare, srcMask);
			}
		}

		return dst;
	}

	consteval auto generateIntersectingRays()
	{
		util::MultiArray<Bitboard, 64, 64> dst{};

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
					dst[from][to]
						= (srcMask | attacks::genRookAttacks(srcSquare, Bitboard{}))
						& (dstMask | attacks::genRookAttacks(dstSquare, Bitboard{}));
				else if (bishopAttacks[dstSquare])
					dst[from][to]
						= (srcMask | attacks::genBishopAttacks(srcSquare, Bitboard{}))
						& (dstMask | attacks::genBishopAttacks(dstSquare, Bitboard{}));
			}
		}

		return dst;
	}

	constexpr auto BetweenRays = generateBetweenRays();
	constexpr auto IntersectingRays = generateIntersectingRays();

	constexpr auto rayBetween(Square src, Square dst)
	{
		return BetweenRays[static_cast<i32>(src)][static_cast<i32>(dst)];
	}

	constexpr auto rayIntersecting(Square src, Square dst)
	{
		return IntersectingRays[static_cast<i32>(src)][static_cast<i32>(dst)];
	}
}
