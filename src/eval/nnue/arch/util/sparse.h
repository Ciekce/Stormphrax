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

#include "../../../../types.h"

#include <bit>
#include <limits>

#include "../../../../util/multi_array.h"
#include "../../../../util/simd.h"

namespace stormphrax::eval::nnue::arch::sparse {
    template <u32 kL1Size>
    class SparseContext {
    public:
        inline void update(util::simd::VectorU8 a, util::simd::VectorU8 b) {
            const auto mask =
                (util::simd::nonzeroMask<u8>(b) << util::simd::kChunkSize<i32>) | util::simd::nonzeroMask<u8>(a);

            for (u32 output = 0; output < kSparseOutputsPerChunk; ++output) {
                const auto byte = (mask >> (output * 8)) & 0xFF;
                const auto nonzero = load(&kNonZeroIndices[byte]);
                const auto indices = add(m_base, nonzero);
                ustore(&m_indices[m_count], indices);
                m_base = add(m_base, set1(8));
                m_count += std::popcount(byte);
            }
        }

        [[nodiscard]] inline usize count() const {
            return m_count;
        }

        [[nodiscard]] inline usize chunk(usize idx) const {
            return m_indices[idx];
        }

    private:
        static constexpr usize kAlignment = 16;

        alignas(kAlignment) static constexpr auto kNonZeroIndices = [] {
            constexpr usize kCount = std::numeric_limits<u8>::max() + 1;

            util::MultiArray<u16, kCount, 8> indices{};

            for (usize i = 0; i < kCount; ++i) {
                usize count = 0;

                for (u8 v = i; v != 0; v &= v - 1) {
                    const auto lsb = std::countr_zero(v);
                    indices[i][count++] = lsb;
                }
            }

            return indices;
        }();

        static constexpr auto kI8ChunkSizeI32 = sizeof(i32) / sizeof(i8);

        static constexpr u32 kSparseChunks = kL1Size / kI8ChunkSizeI32;
        static constexpr u32 kSparseChunkSize = std::max<u32>(util::simd::kChunkSize<i32>, 8) * 2;
        static constexpr u32 kSparseOutputsPerChunk = kSparseChunkSize / 8;

#if SP_HAS_NEON
        using Vector128I16 = uint16x8_t;

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 zero() {
            return vdupq_n_u16(0);
        }

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 set1(i16 v) {
            return vdupq_n_u16(v);
        }

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 load(const void* ptr) {
            assert(util::isAligned<kAlignment>(ptr));
            return vld1q_u16(reinterpret_cast<const i16*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG static void ustore(void* ptr, Vector128I16 v) {
            return vst1q_u16(reinterpret_cast<i16*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 add(Vector128I16 a, Vector128I16 b) {
            return vaddq_u16(a, b);
        }
#else
        using Vector128I16 = __m128i;

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 zero() {
            return _mm_setzero_si128();
        }

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 set1(i16 v) {
            return _mm_set1_epi16(v);
        }

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 load(const void* ptr) {
            assert(util::isAligned<kAlignment>(ptr));
            return _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG static void ustore(void* ptr, Vector128I16 v) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG static Vector128I16 add(Vector128I16 a, Vector128I16 b) {
            return _mm_add_epi16(a, b);
        }
#endif

        util::simd::Array<u16, kSparseChunks> m_indices;

        Vector128I16 m_base = zero();
        usize m_count = 0;
    };
} // namespace stormphrax::eval::nnue::arch::sparse
