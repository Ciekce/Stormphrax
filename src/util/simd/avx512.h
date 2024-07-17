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

#include "../../types.h"

#include "../../arch.h"
#include "../align.h"

#if SP_HAS_AVX512

#include "x64common.h"

namespace stormphrax::util::simd
{
	using VectorI16 = __m512i;
	using VectorI32 = __m512i;

	constexpr std::uintptr_t Alignment = sizeof(VectorI16);

	namespace impl
	{
		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI16() -> VectorI16
		{
			return _mm512_setzero_si512();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I16(i16 v) -> VectorI16
		{
			return _mm512_set1_epi16(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI16(const void *ptr) -> VectorI16
		{
			assert(isAligned<Alignment>(ptr));
			return _mm512_load_si512(ptr);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI16(void *ptr, VectorI16 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm512_store_si512(ptr, v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm512_min_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm512_max_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI16(
			VectorI16 v, VectorI16 min, VectorI16 max) -> VectorI16
		{
			return minI16(maxI16(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm512_add_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm512_sub_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm512_mullo_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjI16(VectorI16 a, VectorI16 b) -> VectorI32
		{
			return _mm512_madd_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI32() -> VectorI32
		{
			return _mm512_setzero_si512();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I32(i32 v) -> VectorI32
		{
			return _mm512_set1_epi32(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI32(const void *ptr) -> VectorI32
		{
			assert(isAligned<Alignment>(ptr));
			return _mm512_load_si512(ptr);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI32(void *ptr, VectorI32 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm512_store_si512(ptr, v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm512_min_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm512_max_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI32(
			VectorI32 v, VectorI32 min, VectorI32 max) -> VectorI32
		{
			return minI32(maxI32(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm512_add_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm512_sub_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm512_mullo_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32(VectorI32 v) -> i32
		{
			return x64::hsumI32Avx512(v);
		}

		// Depends on addI32
		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAccI16(VectorI32 sum, VectorI32 a, VectorI32 b) -> VectorI32
		{
#if SP_HAS_AVX512VNNI
			return _mm512_dpwssd_epi32(sum, a, b);
#else
			const auto products = mulAddAdjI16(a, b);
			return addI32(sum, products);
#endif
		}
	}
}

#endif
