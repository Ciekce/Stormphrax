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

#include "../arch.h"

#if SP_HAS_AVX512 || SP_HAS_AVX2 || SP_HAS_SSE41
#include <immintrin.h>
#else
#include <algorithm>
#endif

namespace stormphrax::util
{
#if SP_HAS_AVX512
	#define SP_SIMD_ALIGNMENT 64
	#define SP_SIMD_ALIGNAS alignas(SP_SIMD_ALIGNMENT)

	struct Simd
	{
		using Register = __m512i;
		static constexpr auto RegisterSize = sizeof(Register);

		static inline auto zero()
		{
			return _mm512_setzero_si512();
		}

		static inline auto set1(i16 v)
		{
			return _mm512_set1_epi16(v);
		}

		static inline auto load(const void *ptr)
		{
			return _mm512_load_si512(ptr);
		}

		static inline auto max16(Register a, Register b)
		{
			return _mm512_max_epi16(a, b);
		}

		static inline auto clamp16(Register v, Register min, Register max)
		{
			return _mm512_max_epi16(_mm512_min_epi16(v, max), min);
		}

		static inline auto mul16(Register a, Register b)
		{
			return _mm512_mullo_epi16(a, b);
		}

		static inline auto mulAddAdj16(Register a, Register b)
		{
			return _mm512_madd_epi16(a, b);
		}

		static inline auto add32(Register a, Register b)
		{
			return _mm512_add_epi32(a, b);
		}

		static inline auto sum32(Register v)
		{
			return _mm512_reduce_add_epi32(v);
		}
	};
#elif SP_HAS_AVX2
	#define SP_SIMD_ALIGNMENT 32
	#define SP_SIMD_ALIGNAS alignas(SP_SIMD_ALIGNMENT)

	struct Simd
	{
		using Register = __m256i;
		static constexpr auto RegisterSize = sizeof(Register);

		static inline auto zero()
		{
			return _mm256_setzero_si256();
		}

		static inline auto set1(i16 v)
		{
			return _mm256_set1_epi16(v);
		}

		static inline auto load(const void *ptr)
		{
			return _mm256_load_si256(static_cast<const __m256i *>(ptr));
		}

		static inline auto max16(Register a, Register b)
		{
			return _mm256_max_epi16(a, b);
		}

		static inline auto clamp16(Register v, Register min, Register max)
		{
			return _mm256_max_epi16(_mm256_min_epi16(v, max), min);
		}

		static inline auto mul16(Register a, Register b)
		{
			return _mm256_mullo_epi16(a, b);
		}

		static inline auto mulAddAdj16(Register a, Register b)
		{
			return _mm256_madd_epi16(a, b);
		}

		static inline auto add32(Register a, Register b)
		{
			return _mm256_add_epi32(a, b);
		}

		static inline auto sum32(Register v)
		{
			const auto high = _mm256_extracti128_si256(v, 1);
			const auto low = _mm256_castsi256_si128(v);

			const auto sum = _mm_add_epi32(high, low);

			const auto high64 = _mm_unpackhi_epi64(sum, sum);
			const auto sum64 = _mm_add_epi32(sum, high64);

			const auto high32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
			const auto sum32 = _mm_add_epi32(sum64, high32);

			return _mm_cvtsi128_si32(sum32);
		}
	};
#elif SP_HAS_SSE41
	#define SP_SIMD_ALIGNMENT 16
	#define SP_SIMD_ALIGNAS alignas(SP_SIMD_ALIGNMENT)

	struct Simd
	{
		using Register = __m128i;
		static constexpr auto RegisterSize = sizeof(Register);

		static inline auto zero()
		{
			return _mm_setzero_si128();
		}

		static inline auto set1(i16 v)
		{
			return _mm_set1_epi16(v);
		}

		static inline auto load(const void *ptr)
		{
			return _mm_load_si128(static_cast<const __m128i *>(ptr));
		}

		static inline auto max16(Register a, Register b)
		{
			return _mm_max_epi16(a, b);
		}

		static inline auto clamp16(Register v, Register min, Register max)
		{
			return _mm_max_epi16(_mm_min_epi16(v, max), min);
		}

		static inline auto mul16(Register a, Register b)
		{
			return _mm_mullo_epi16(a, b);
		}

		static inline auto mulAddAdj16(Register a, Register b)
		{
			return _mm_madd_epi16(a, b);
		}

		static inline auto add32(Register a, Register b)
		{
			return _mm_add_epi32(a, b);
		}

		static inline auto sum32(Register v)
		{
			const auto high64 = _mm_unpackhi_epi64(v, v);
			const auto sum64 = _mm_add_epi32(v, high64);

			const auto high32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
			const auto sum32 = _mm_add_epi32(sum64, high32);

			return _mm_cvtsi128_si32(sum32);
		}
	};
#else //TODO neon
	#warning No supported SIMD architecture detected. Things are going to be slow

	#define SP_SIMD_ALIGNMENT 1
	#define SP_SIMD_ALIGNAS

	struct Simd
	{
		using Register = i32;
		static constexpr auto RegisterSize = sizeof(i16);

		static inline auto zero()
		{
			return 0;
		}

		static inline auto set1(i16 v)
		{
			return static_cast<i32>(v);
		}

		static inline auto load(const i16 *ptr)
		{
			return static_cast<i32>(*static_cast<const i16 *>(ptr));
		}

		static inline auto max16(Register a, Register b)
		{
			return std::max(a, b);
		}

		static inline auto clamp16(Register v, Register min, Register max)
		{
			return std::clamp(v, min, max);
		}

		static inline auto mul16(Register a, Register b)
		{
			return a * b;
		}

		static inline auto mulAddAdj16(Register a, Register b)
		{
			return a * b;
		}

		static inline auto add32(Register a, Register b)
		{
			return a + b;
		}

		static inline auto sum32(Register v)
		{
			return v;
		}
	};
#endif
}
