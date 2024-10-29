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

#if SP_HAS_AVX512 || SP_HAS_AVX2 || SP_HAS_SSE41

    #include <immintrin.h>

namespace stormphrax::util::simd::impl::x64 {
    #if SP_HAS_SSE41
    SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32Sse41(__m128i v) -> i32 {
        const auto high64 = _mm_unpackhi_epi64(v, v);
        const auto sum64 = _mm_add_epi32(v, high64);

        const auto high32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        const auto sum32 = _mm_add_epi32(sum64, high32);

        return _mm_cvtsi128_si32(sum32);
    }
    #endif

    #if SP_HAS_AVX2
    SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32Avx2(__m256i v) -> i32 {
        const auto high128 = _mm256_extracti128_si256(v, 1);
        const auto low128 = _mm256_castsi256_si128(v);

        const auto sum128 = _mm_add_epi32(high128, low128);

        return hsumI32Sse41(sum128);
    }
    #endif

    #if SP_HAS_AVX512
    SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32Avx512(__m512i v) -> i32 {
        const auto high256 = _mm512_extracti64x4_epi64(v, 1);
        const auto low256 = _mm512_castsi512_si256(v);

        const auto sum256 = _mm256_add_epi32(high256, low256);

        return hsumI32Avx2(sum256);
    }
    #endif
} // namespace stormphrax::util::simd::impl::x64

#endif
