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

#include "../types.h"

#include <array>

#include "nnue/activation.h"
#include "nnue/arch/multilayer.h"
#include "nnue/arch/singlelayer.h"
#include "nnue/features.h"
#include "nnue/output.h"

namespace stormphrax::eval {
    // current arch: (704x16hm->1792)x2->(16x2->32->1)x8
    // pairwise clipped ReLU -> dual clipped + clipped squared ReLU -> clipped ReLU

    constexpr u32 kFtQBits = 8;
    constexpr u32 kL1QBits = 6;

    constexpr u32 kL1Size = 1280;

    using L1Activation = nnue::activation::SquaredClippedReLU;

    constexpr i32 kScale = 400;

    // visually flipped upside down, a1 = 0
    using InputFeatureSet = nnue::features::KingBucketsMirrored<
        nnue::features::MirroredKingSide::kAbcd,
        // clang-format off
         0,  1,  2,  3,
         4,  5,  6,  7,
         8,  9, 10, 11,
         8,  9, 10, 11,
        12, 12, 13, 13,
        12, 12, 13, 13,
        14, 14, 15, 15,
        14, 14, 15, 15
        // clang-format on
        >;

    using OutputBucketing = nnue::output::Single;

    using LayeredArch =
        nnue::arch::SingleLayer<kL1Size, (1 << kFtQBits) - 1, 1 << kL1QBits, L1Activation, OutputBucketing, kScale>;
} // namespace stormphrax::eval
