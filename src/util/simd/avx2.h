/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

    #include <immintrin.h>

namespace stormphrax::util::simd {
    using VectorU8 = __m256i;
    using VectorU16 = __m256i;

    using VectorI8 = __m256i;
    using VectorI16 = __m256i;
    using VectorI32 = __m256i;

    constexpr std::uintptr_t kAlignment = sizeof(__m256i);

    constexpr bool kPackNonSequential = true;

    constexpr usize kPackGrouping = 8;
    constexpr auto kPackOrdering = std::array{
        0,
        2,
        1,
        3,
    };

    namespace impl {
        // ================================ u8 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorU8 zeroU8() {
            return _mm256_setzero_si256();
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorU8 loadU8(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return _mm256_load_si256(static_cast<const VectorU8*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeU8(void* ptr, VectorU8 v) {
            assert(isAligned<kAlignment>(ptr));
            _mm256_store_si256(static_cast<VectorU8*>(ptr), v);
        }

        // ================================ u16 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorU16 zeroU16() {
            return _mm256_setzero_si256();
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorU16 loadU16(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return _mm256_load_si256(static_cast<const VectorU16*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeU16(void* ptr, VectorU16 v) {
            assert(isAligned<kAlignment>(ptr));
            _mm256_store_si256(static_cast<VectorU16*>(ptr), v);
        }

        // ================================ i8 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 zeroI8() {
            return _mm256_setzero_si256();
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 set1I8(i8 v) {
            return _mm256_set1_epi8(v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 loadI8(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return _mm256_load_si256(static_cast<const VectorI8*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeI8(void* ptr, VectorI8 v) {
            assert(isAligned<kAlignment>(ptr));
            _mm256_store_si256(static_cast<VectorI8*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 minI8(VectorI8 a, VectorI8 b) {
            return _mm256_min_epi8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 maxI8(VectorI8 a, VectorI8 b) {
            return _mm256_max_epi8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 clampI8(VectorI8 v, VectorI8 min, VectorI8 max) {
            return minI8(maxI8(v, min), max);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 addI8(VectorI8 a, VectorI8 b) {
            return _mm256_add_epi8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 subI8(VectorI8 a, VectorI8 b) {
            return _mm256_sub_epi8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 shiftLeftI8([[maybe_unused]] VectorI8 v, [[maybe_unused]] i32 shift) {
            unimplemented();
        }

        // ================================ i16 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 zeroI16() {
            return _mm256_setzero_si256();
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 set1I16(i16 v) {
            return _mm256_set1_epi16(v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 loadI16(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return _mm256_load_si256(static_cast<const VectorI16*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeI16(void* ptr, VectorI16 v) {
            assert(isAligned<kAlignment>(ptr));
            _mm256_store_si256(static_cast<VectorI16*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 minI16(VectorI16 a, VectorI16 b) {
            return _mm256_min_epi16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 maxI16(VectorI16 a, VectorI16 b) {
            return _mm256_max_epi16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 clampI16(VectorI16 v, VectorI16 min, VectorI16 max) {
            return minI16(maxI16(v, min), max);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 addI16(VectorI16 a, VectorI16 b) {
            return _mm256_add_epi16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 subI16(VectorI16 a, VectorI16 b) {
            return _mm256_sub_epi16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 mulLoI16(VectorI16 a, VectorI16 b) {
            return _mm256_mullo_epi16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 shiftLeftI16(VectorI16 v, i32 shift) {
            return _mm256_slli_epi16(v, shift);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 shiftRightI16(VectorI16 v, i32 shift) {
            return _mm256_srai_epi16(v, shift);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 shiftLeftMulHiI16(VectorI16 a, VectorI16 b, i32 shift) {
            const auto shifted = _mm256_slli_epi16(a, shift);
            return _mm256_mulhi_epi16(shifted, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 mulAddAdjI16(VectorI16 a, VectorI16 b) {
            return _mm256_madd_epi16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 packUnsignedI16(VectorI16 a, VectorI16 b) {
            return _mm256_packus_epi16(a, b);
        }

        // ================================ i32 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 zeroI32() {
            return _mm256_setzero_si256();
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 set1I32(i32 v) {
            return _mm256_set1_epi32(v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 loadI32(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return _mm256_load_si256(static_cast<const VectorI32*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeI32(void* ptr, VectorI32 v) {
            assert(isAligned<kAlignment>(ptr));
            _mm256_store_si256(static_cast<VectorI32*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 minI32(VectorI32 a, VectorI32 b) {
            return _mm256_min_epi32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 maxI32(VectorI32 a, VectorI32 b) {
            return _mm256_max_epi32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 clampI32(VectorI32 v, VectorI32 min, VectorI32 max) {
            return minI32(maxI32(v, min), max);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 addI32(VectorI32 a, VectorI32 b) {
            return _mm256_add_epi32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 subI32(VectorI32 a, VectorI32 b) {
            return _mm256_sub_epi32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 mulLoI32(VectorI32 a, VectorI32 b) {
            return _mm256_mullo_epi32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 shiftLeftI32(VectorI32 v, i32 shift) {
            return _mm256_slli_epi32(v, shift);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 shiftRightI32(VectorI32 v, i32 shift) {
            return _mm256_srai_epi32(v, shift);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorU16 packUnsignedI32(VectorI32 a, VectorI32 b) {
            return _mm256_packus_epi32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline i32 hsumI32(VectorI32 v) {
            const auto high128 = _mm256_extracti128_si256(v, 1);
            const auto low128 = _mm256_castsi256_si128(v);

            const auto sum128 = _mm_add_epi32(high128, low128);

            const auto high64 = _mm_unpackhi_epi64(sum128, sum128);
            const auto sum64 = _mm_add_epi32(sum128, high64);

            const auto high32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
            const auto sum32 = _mm_add_epi32(sum64, high32);

            return _mm_cvtsi128_si32(sum32);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 dpbusdI32(VectorI32 sum, VectorU8 u, VectorI8 i) {
    #if SP_HAS_VNNI256
            return _mm256_dpbusd_epi32(sum, u, i);
    #else
            const auto p = _mm256_maddubs_epi16(u, i);
            const auto w = _mm256_madd_epi16(p, _mm256_set1_epi16(1));
            return _mm256_add_epi32(sum, w);
    #endif
        }

        SP_ALWAYS_INLINE_NDEBUG inline u32 nonzeroMaskU8(VectorU8 v) {
            const auto nz = _mm256_cmpgt_epi32(v, _mm256_setzero_si256());
            return _mm256_movemask_ps(_mm256_castsi256_ps(nz));
        }

        // Depends on addI32
        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 mulAddAdjAccI16(VectorI32 sum, VectorI16 a, VectorI16 b) {
    #if SP_HAS_VNNI256
            return _mm256_dpwssd_epi32(sum, a, b);
    #else
            const auto products = mulAddAdjI16(a, b);
            return addI32(sum, products);
    #endif
        }
    } // namespace impl
} // namespace stormphrax::util::simd

#endif
