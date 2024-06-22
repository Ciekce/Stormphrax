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

#include <new>

#if defined(SP_NATIVE)
	// cannot expand a macro to defined()
	#if __BMI2__ && defined(SP_FAST_PEXT)
		#define SP_HAS_BMI2 1
	#else
		#define SP_HAS_BMI2 0
	#endif
	#define SP_HAS_VNNI512 __AVX512VNNI__
	#define SP_HAS_AVX512 __AVX512F__
	#define SP_HAS_AVX2 __AVX2__
	#define SP_HAS_NEON __neon__
	#define SP_HAS_BMI1 __BMI__
	#define SP_HAS_POPCNT __POPCNT__
	#define SP_HAS_SSE41 __SSE4_1__
#elif defined(SP_VNNI512)
	#define SP_HAS_BMI2 1
	#define SP_HAS_VNNI512 1
	#define SP_HAS_AVX512 1
	#define SP_HAS_AVX2 1
	#define SP_HAS_NEON 0
	#define SP_HAS_BMI1 1
	#define SP_HAS_POPCNT 1
	#define SP_HAS_SSE41 1
#elif defined(SP_AVX512)
	#define SP_HAS_BMI2 1
	#define SP_HAS_VNNI512 0
	#define SP_HAS_AVX512 1
	#define SP_HAS_AVX2 1
	#define SP_HAS_NEON 0
	#define SP_HAS_BMI1 1
	#define SP_HAS_POPCNT 1
	#define SP_HAS_SSE41 1
#elif defined(SP_AVX2_BMI2)
	#define SP_HAS_BMI2 1
	#define SP_HAS_VNNI512 0
	#define SP_HAS_AVX512 0
	#define SP_HAS_AVX2 1
	#define SP_HAS_NEON 0
	#define SP_HAS_BMI1 1
	#define SP_HAS_POPCNT 1
	#define SP_HAS_SSE41 1
#elif defined(SP_AVX2)
	#define SP_HAS_BMI2 0
	#define SP_HAS_VNNI512 0
	#define SP_HAS_AVX512 0
	#define SP_HAS_AVX2 1
	#define SP_HAS_NEON 0
	#define SP_HAS_BMI1 1
	#define SP_HAS_POPCNT 1
	#define SP_HAS_SSE41 1
#elif defined(SP_SSE41_POPCNT)
	#define SP_HAS_BMI2 0
	#define SP_HAS_VNNI512 0
	#define SP_HAS_AVX512 0
	#define SP_HAS_AVX2 0
	#define SP_HAS_NEON 0
	#define SP_HAS_BMI1 0
	#define SP_HAS_POPCNT 1
	#define SP_HAS_SSE41 1
#else
#error no arch specified
#endif

namespace stormphrax
{
#ifdef __cpp_lib_hardware_interference_size
	constexpr auto CacheLineSize = std::hardware_destructive_interference_size;
#else
	constexpr usize CacheLineSize = 64;
#endif
}
