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

#include "../../types.h"
#include "../../core.h"
#include "../../bitboard.h"
#include "../util.h"
#include "data.h"
#include "../../util/bits.h"

namespace polaris::attacks
{
	extern const std::array<Bitboard, black_magic::  RookData.tableSize>   RookAttacks;
	extern const std::array<Bitboard, black_magic::BishopData.tableSize> BishopAttacks;

	[[nodiscard]] inline u64 getRookIdx(Bitboard occupancy, Square src)
	{
		const auto s = static_cast<i32>(src);

		const auto &data = black_magic::RookData.data[s];

		const auto magic = black_magic::RookMagics[s];
		const auto shift = black_magic::RookShifts[s];

		return ((occupancy | data.mask) * magic) >> shift;
	}

	[[nodiscard]] inline u64 getBishopIdx(Bitboard occupancy, Square src)
	{
		const auto s = static_cast<i32>(src);

		const auto &data = black_magic::BishopData.data[s];

		const auto magic = black_magic::BishopMagics[s];
		const auto shift = black_magic::BishopShifts[s];

		return ((occupancy | data.mask) * magic) >> shift;
	}

	[[nodiscard]] inline Bitboard getRookAttacks(Square src, Bitboard occupancy)
	{
		const auto s = static_cast<i32>(src);

		const auto &data = black_magic::RookData.data[s];
		const auto idx = getRookIdx(occupancy, src);

		return RookAttacks[data.offset + idx];
	}

	[[nodiscard]] inline Bitboard getBishopAttacks(Square src, Bitboard occupancy)
	{
		const auto s = static_cast<i32>(src);

		const auto &data = black_magic::BishopData.data[s];
		const auto idx = getBishopIdx(occupancy, src);

		return BishopAttacks[data.offset + idx];
	}
}
