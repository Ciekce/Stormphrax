/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "../../../../util/simd.h"

namespace stormphrax::eval::nnue::arch::sparse {
    template <u32 kL1Size>
    class SparseContext {
    public:
        inline void update(util::simd::VectorU8 a, util::simd::VectorU8 b) {
            const auto mask = _mm512_kunpackw(util::simd::nonzeroMask<u8>(b), util::simd::nonzeroMask<u8>(a));
            const auto indices = _mm512_maskz_compress_epi16(mask, m_base);

            _mm512_storeu_si512(&m_indices[m_count], indices);

            m_base = _mm512_add_epi16(m_base, _mm512_set1_epi16(32));
            m_count += std::popcount(mask);

            assert(m_count <= m_indices.size());
        }

        [[nodiscard]] inline usize count() const {
            return m_count;
        }

        [[nodiscard]] inline usize chunk(usize idx) const {
            return m_indices[idx];
        }

    private:
        static constexpr auto kI8ChunkSizeI32 = sizeof(i32) / sizeof(i8);

        static constexpr u32 kChunks = kL1Size / kI8ChunkSizeI32;

        util::simd::Array<u16, kChunks> m_indices;

        // clang-format off
        __m512i m_base = _mm512_set_epi16(
            31, 30, 29, 28, 27, 26, 25, 24,
            23, 22, 21, 20, 19, 18, 17, 16,
            15, 14, 13, 12, 11, 10,  9,  8,
             7,  6,  5,  4,  3,  2,  1,  0
        );
        // clang-format on
        usize m_count = 0;
    };
} // namespace stormphrax::eval::nnue::arch::sparse
