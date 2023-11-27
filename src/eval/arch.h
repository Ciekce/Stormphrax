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

#include <array>

#include "activation.h"

namespace stormphrax::eval
{
	// current arch: (768x4->768)x2->1x8, SquaredClippedReLU

	// perspective
	const auto ArchId = 1;

	constexpr i32 L1Q = 255;
	constexpr i32 OutputQ = 64;

	using Activation = activation::SquaredClippedReLU<L1Q>;

	constexpr u32 InputSize = 768;
	constexpr u32 Layer1Size = 1024;

	constexpr i32 Scale = 400;

	// visually flipped upside down, a1 = 0
	constexpr auto InputBuckets = std::array {
		0, 0, 0, 0, 1, 1, 1, 1,
		0, 0, 0, 0, 1, 1, 1, 1,
		2, 2, 2, 2, 3, 3, 3, 3,
		2, 2, 2, 2, 3, 3, 3, 3,
		2, 2, 2, 2, 3, 3, 3, 3,
		2, 2, 2, 2, 3, 3, 3, 3,
		2, 2, 2, 2, 3, 3, 3, 3,
		2, 2, 2, 2, 3, 3, 3, 3
	};

	constexpr auto OutputBucketCount = 8;
}
