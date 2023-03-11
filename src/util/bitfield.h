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

#include "../types.h"

#define ENUM_FLAG_OPERATOR(T,UT,O) \
[[maybe_unused]] inline constexpr auto operator O(T lhs, T rhs) { return static_cast<T>(static_cast<UT>(lhs) O static_cast<UT>(rhs)); } \
[[maybe_unused]] inline constexpr auto operator O##=(T &lhs, T rhs) { return lhs = static_cast<T>(static_cast<UT>(lhs) O static_cast<UT>(rhs)); }
#define ENUM_FLAGS(UT,T) \
enum class T : UT; \
[[maybe_unused]] inline constexpr auto operator ~(T t) { return static_cast<T>(~static_cast<UT>(t)); } \
ENUM_FLAG_OPERATOR(T,UT,|) \
ENUM_FLAG_OPERATOR(T,UT,^) \
ENUM_FLAG_OPERATOR(T,UT,&) \
enum class T : UT

namespace polaris
{
	template <typename T>
	constexpr bool testFlags(T field, T flags)
	{
		return (field & flags) != T::None;
	}

	template <typename T>
	constexpr T setFlags(T field, T flags, bool v)
	{
		if (v)
			field |= flags;
		else field &= ~flags;

		return field;
	}

	template <typename T>
	constexpr T flipFlags(T field, T flags)
	{
		return field ^ flags;
	}
}
