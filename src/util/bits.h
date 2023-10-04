/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

#ifdef __x86_64__
#include <immintrin.h>
#endif

#include <type_traits>

#include "../arch.h"

namespace stormphrax::util
{
	namespace fallback
	{
		[[nodiscard]] constexpr auto lsb(u64 v)
		{
			return v & -v;
		}

		[[nodiscard]] constexpr auto resetLsb(u64 v)
		{
			return v & (v - 1);
		}

		[[nodiscard]] constexpr auto popcnt(u64 v)
		{
			v -= (v >> 1) & U64(0x5555555555555555);
			v  = (v & U64(0x3333333333333333)) + ((v >> 2) & U64(0x3333333333333333));
			v  = (v + (v >> 4)) & U64(0x0F0F0F0F0F0F0F0F);
			return static_cast<i32>((v * U64(0x0101010101010101)) >> 56);
		}

		[[nodiscard]] constexpr auto ctz(u64 v)
		{
			if (std::is_constant_evaluated())
			{
				if (v == 0)
					return 64;

				i32 cnt{};

				while ((v & 1) == 0)
				{
					++cnt;
					v >>= 1;
				}

				return cnt;
			}

			if (v == 0)
				return 64;

			return __builtin_ctzll(v);
		}

		[[nodiscard]] constexpr auto pext(u64 v, u64 mask)
		{
			u64 dst{};

			for (u64 bit = 1; mask != 0; bit <<= 1)
			{
				if ((v & mask & -mask) != 0)
					dst |= bit;
				mask &= mask - 1;
			}

			return dst;
		}

		[[nodiscard]] constexpr auto pdep(u64 v, u64 mask)
		{
			u64 dst{};

			for (u64 bit = 1; mask != 0; bit <<= 1)
			{
				if ((v & bit) != 0)
					dst |= mask & -mask;
				mask &= mask - 1;
			}

			return dst;
		}
	}

	[[nodiscard]] constexpr auto lsb(u64 v) -> u64
	{
#if SP_HAS_BMI1
		if (std::is_constant_evaluated())
			return fallback::lsb(v);

		return _blsi_u64(v);
#else
		return fallback::lsb(v);
#endif
	}

	[[nodiscard]] constexpr auto resetLsb(u64 v) -> u64
	{
#if SP_HAS_BMI1
		if (std::is_constant_evaluated())
			return fallback::resetLsb(v);

		return _blsr_u64(v);
#else
		return fallback::resetLsb(v);
#endif
	}

	[[nodiscard]] constexpr auto popcnt(u64 v)
	{
#if SP_HAS_POPCNT
		if (std::is_constant_evaluated())
			return fallback::popcnt(v);

		return static_cast<i32>(_mm_popcnt_u64(v));
#else
		return fallback::popcnt(v);
#endif
	}

	[[nodiscard]] constexpr auto ctz(u64 v)
	{
#if SP_HAS_BMI1
		if (std::is_constant_evaluated())
			return fallback::ctz(v);

		return static_cast<i32>(__tzcnt_u64(v));
#else
		return fallback::ctz(v);
#endif
	}

	[[nodiscard]] constexpr auto pext(u64 v, u64 mask) -> u64
	{
#if SP_HAS_BMI2
		if (std::is_constant_evaluated())
			return fallback::pext(v, mask);

		return _pext_u64(v, mask);
#else
		return fallback::pext(v, mask);
#endif
	}

	[[nodiscard]] constexpr auto pdep(u64 v, u64 mask) -> u64
	{
#if SP_HAS_BMI2
		if (std::is_constant_evaluated())
			return fallback::pdep(v, mask);

		return _pdep_u64(v, mask);
#else
		return fallback::pdep(v, mask);
#endif
	}
}
