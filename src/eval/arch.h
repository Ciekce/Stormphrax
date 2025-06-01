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

#include "../types.h"

#include <array>

#include "nnue/activation.h"
#include "nnue/output.h"
#include "nnue/features.h"
#include "nnue/arch/singlelayer.h"
#include "nnue/arch/multilayer.h"

namespace stormphrax::eval
{
	// current arch: (768x16->1792)x2->(8->32->1)x8, mirrored
	// pairwise clipped ReLU -> squared ReLU -> squared ReLU

	constexpr i32 FtQ = 255;
	constexpr i32 L1Q = 128;

	constexpr u32 FtScaleBits = 7;

	constexpr u32 L1Size = 1792;
	constexpr u32 L2Size = 16;
	constexpr u32 L3Size = 32;

	using L1Activation = nnue::activation::ClippedReLU;
	using L2Activation = nnue::activation::SquaredClippedReLU;
	using L3Activation = nnue::activation::ClippedReLU;

	constexpr i32 Scale = 400;

	// visually flipped upside down, a1 = 0
	using InputFeatureSet = nnue::features::KingBucketsMirrored<
		nnue::features::MirroredKingSide::Abcd,
		 0,  1,  2,  3,
		 4,  5,  6,  7,
		 8,  9, 10, 11,
		 8,  9, 10, 11,
		12, 12, 13, 13,
		12, 12, 13, 13,
		14, 14, 15, 15,
		14, 14, 15, 15
	>;

	using OutputBucketing = nnue::output::MaterialCount<8>;

	using LayeredArch = nnue::arch::PairwiseMultilayerCReLU<
		L1Size, L2Size, L3Size, FtScaleBits, FtQ, L1Q, L2Activation, L3Activation, OutputBucketing, Scale>;
}
