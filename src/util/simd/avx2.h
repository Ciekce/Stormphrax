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

#if SP_HAS_AVX2 && !SP_HAS_AVX512

#include "x64common.h"

namespace stormphrax::util::simd
{
	using VectorU8 = __m256i;
	using VectorU16 = __m256i;

	using VectorI8 = __m256i;
	using VectorI16 = __m256i;
	using VectorI32 = __m256i;

	using VectorF32 = __m256;

	constexpr std::uintptr_t Alignment = sizeof(__m256i);

	namespace impl
	{
		// ================================ u8 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroU8() -> VectorU8
		{
			return _mm256_setzero_si256();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadU8(const void *ptr) -> VectorU8
		{
			assert(isAligned<Alignment>(ptr));
			return _mm256_load_si256(static_cast<const VectorU8 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeU8(void *ptr, VectorU8 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm256_store_si256(static_cast<VectorU8 *>(ptr), v);
		}

		// ================================ u16 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroU16() -> VectorU16
		{
			return _mm256_setzero_si256();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadU16(const void *ptr) -> VectorU16
		{
			assert(isAligned<Alignment>(ptr));
			return _mm256_load_si256(static_cast<const VectorU16 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeU16(void *ptr, VectorU16 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm256_store_si256(static_cast<VectorU16 *>(ptr), v);
		}

		// ================================ i8 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI8() -> VectorI8
		{
			return _mm256_setzero_si256();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I8(i8 v) -> VectorI8
		{
			return _mm256_set1_epi8(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI8(const void *ptr) -> VectorI8
		{
			assert(isAligned<Alignment>(ptr));
			return _mm256_load_si256(static_cast<const VectorI8 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI8(void *ptr, VectorI8 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm256_store_si256(static_cast<VectorI8 *>(ptr), v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return _mm256_min_epi8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return _mm256_max_epi8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI8(
			VectorI8 v, VectorI8 min, VectorI8 max) -> VectorI8
		{
			return minI8(maxI8(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return _mm256_add_epi8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return _mm256_sub_epi8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftI8(VectorI8 v, i32 shift) -> VectorI8
		{
			unimplemented();
		}

		// ================================ i16 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI16() -> VectorI16
		{
			return _mm256_setzero_si256();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I16(i16 v) -> VectorI16
		{
			return _mm256_set1_epi16(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI16(const void *ptr) -> VectorI16
		{
			assert(isAligned<Alignment>(ptr));
			return _mm256_load_si256(static_cast<const VectorI16 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI16(void *ptr, VectorI16 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm256_store_si256(static_cast<VectorI16 *>(ptr), v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm256_min_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm256_max_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI16(
			VectorI16 v, VectorI16 min, VectorI16 max) -> VectorI16
		{
			return minI16(maxI16(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm256_add_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm256_sub_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulLoI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm256_mullo_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulHiI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm256_mulhi_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftI16(VectorI16 v, i32 shift) -> VectorI16
		{
			return _mm256_slli_epi16(v, shift);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjI16(VectorI16 a, VectorI16 b) -> VectorI32
		{
			return _mm256_madd_epi16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto packUnsignedI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return _mm256_packus_epi16(a, b);
		}

		// ================================ i32 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI32() -> VectorI32
		{
			return _mm256_setzero_si256();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I32(i32 v) -> VectorI32
		{
			return _mm256_set1_epi32(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI32(const void *ptr) -> VectorI32
		{
			assert(isAligned<Alignment>(ptr));
			return _mm256_load_si256(static_cast<const VectorI32 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI32(void *ptr, VectorI32 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm256_store_si256(static_cast<VectorI32 *>(ptr), v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm256_min_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm256_max_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI32(
			VectorI32 v, VectorI32 min, VectorI32 max) -> VectorI32
		{
			return minI32(maxI32(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm256_add_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm256_sub_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulLoI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return _mm256_mullo_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftI32(VectorI32 v, i32 shift) -> VectorI32
		{
			return _mm256_slli_epi32(v, shift);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto packUnsignedI32(VectorI32 a, VectorI32 b) -> VectorU16
		{
			return _mm256_packus_epi32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32(VectorI32 v) -> i32
		{
			return x64::hsumI32Avx2(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto dpbusdI32(VectorI32 sum, VectorU8 u, VectorI8 i)
		{
			const auto p = _mm256_maddubs_epi16(u, i);
			const auto w = _mm256_madd_epi16(p, _mm256_set1_epi16(1));
			return _mm256_add_epi32(sum, w);
		}

		// Depends on addI32
		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAccI16(VectorI32 sum, VectorI16 a, VectorI16 b) -> VectorI32
		{
			const auto products = mulAddAdjI16(a, b);
			return addI32(sum, products);
		}

		// ================================ f32 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroF32() -> VectorF32
		{
			return _mm256_setzero_si256();
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1F32(f32 v) -> VectorF32
		{
			return _mm256_set1_ps(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadF32(const void *ptr) -> VectorF32
		{
			assert(isAligned<Alignment>(ptr));
			return _mm256_load_ps(static_cast<const f32 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeF32(void *ptr, VectorF32 v)
		{
			assert(isAligned<Alignment>(ptr));
			_mm256_store_ps(static_cast<f32 *>(ptr), v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minF32(VectorF32 a, VectorF32 b) -> VectorF32
		{
			return _mm256_min_ps(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxF32(VectorF32 a, VectorF32 b) -> VectorF32
		{
			return _mm256_max_ps(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampF32(
			VectorF32 v, VectorF32 min, VectorF32 max) -> VectorF32
		{
			return minF32(maxF32(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addF32(VectorF32 a, VectorF32 b) -> VectorF32
		{
			return _mm256_add_ps(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subF32(VectorF32 a, VectorF32 b) -> VectorF32
		{
			return _mm256_sub_ps(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulF32(VectorF32 a, VectorF32 b) -> VectorF32
		{
			return _mm256_mul_ps(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto fmaF32(VectorF32 a, VectorF32 b, VectorF32 c) -> VectorF32
		{
			return _mm256_fmadd_ps(a, b, c);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto hsumF32(VectorF32 v) -> f32
		{
			return x64::hsumF32Avx2(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto castI32F32(VectorI32 v) -> VectorF32
		{
			return _mm256_cvtepi32_ps(v);
		}
	}
}

#endif
