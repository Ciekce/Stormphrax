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
	extern const std::array<     u16, bmi2::  RookData.tableSize>   RookAttacks;
	extern const std::array<Bitboard, bmi2::BishopData.tableSize> BishopAttacks;

	inline Bitboard getRookAttacks(Square src, Bitboard occupancy)
	{
		const auto s = static_cast<i32>(src);

		const auto &data = bmi2::RookData.data[s];

		const auto idx = util::pext(occupancy, data.srcMask);
		const auto attacks = util::pdep(RookAttacks[data.offset + idx], data.dstMask);

		return attacks;
	}

	inline Bitboard getBishopAttacks(Square src, Bitboard occupancy)
	{
		const auto s = static_cast<i32>(src);

		const auto &data = bmi2::BishopData.data[s];
		const auto idx = util::pext(occupancy, data.mask);

		return BishopAttacks[data.offset + idx];
	}
}
