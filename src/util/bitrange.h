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

#include <concepts>

namespace stormphrax::util
{
	template <u32 Offset, u32 Bits>
	constexpr auto getBits(std::unsigned_integral auto field)
	{
		constexpr auto Mask = (1 << Bits) - 1;
		return (field >> Offset) & Mask;
	}

	template <u32 Offset>
	constexpr auto setBits(std::unsigned_integral auto field, decltype(field) value)
	{
		return field | (value << Offset);
	}

	template <u32 Offset, u32 Bits>
	constexpr auto replaceBits(std::unsigned_integral auto field, decltype(field) value)
	{
		constexpr auto Mask = ((1 << Bits) - 1) << Offset;
		return (field & ~Mask) | (value << Offset);
	}
}
