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

#include <array>

#include "../../../../arch.h"

#define SP_SPARSE_BENCH_L1_SIZE 0

#if SP_HAS_AVX512 && SP_HAS_VBMI2 && SP_HAS_AVX512VL
    #include "sparse_vbmi2.h"
#else
    #error guard
    #include "sparse_default.h"
#endif

namespace stormphrax::eval::nnue::arch::sparse {
#if SP_SPARSE_BENCH_L1_SIZE > 0
    inline std::array<usize, SP_SPARSE_BENCH_FT_SIZE / 2> g_activationCounts{};
    inline void trackActivations(std::span<const u8, SP_SPARSE_BENCH_FT_SIZE> ftActivations) {
        for (usize i = 0; i < SP_SPARSE_BENCH_FT_SIZE; ++i) {
            if (ftActivations[i] != 0) {
                ++g_activationCounts[i % (SP_SPARSE_BENCH_FT_SIZE / 2)];
            }
        }
    }
#endif
} // namespace stormphrax::eval::nnue::arch::sparse
