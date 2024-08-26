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
#include <bit>

namespace stormphrax::util
{
	template <typename T>
	constexpr auto abs(T v)
	{
		return v < T{0} ? -v : v;
	}

	template <std::integral auto One>
	constexpr auto ilerp(decltype(One) a, decltype(One) b, decltype(One) t)
	{
		return (a * (One - t) + b * t) / One;
	}

	template <std::integral T>
	constexpr auto ceilDiv(T a, T b)
	{
		return (a + b - 1) / b;
	}

	template <std::unsigned_integral T>
	constexpr auto floorLog2(T v)
	{
		return static_cast<T>(8 * sizeof(T) - std::countl_zero(v) - 1);
	}

	template <std::unsigned_integral T>
	constexpr auto ceilLog2(T v)
	{
		return static_cast<T>(8 * sizeof(T) - std::countl_zero(v - 1));
	}
}
