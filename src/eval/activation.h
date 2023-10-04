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

#include <algorithm>

#include "../core.h"

namespace stormphrax::eval::activation
{
	template <i32 Max>
	struct [[maybe_unused]] ClippedReLU
	{
		static constexpr u8 Id = 0;

		static inline auto activate(i16 x) -> i32
		{
			return std::clamp(static_cast<i32>(x), 0, Max);
		}

		static constexpr i32 NormalizationK = 1;
	};

	template <i32 Max>
	struct [[maybe_unused]] SquaredClippedReLU
	{
		static constexpr u8 Id = 1;

		static inline auto activate(i16 x) -> i32
		{
			const auto clipped = std::clamp(static_cast<i32>(x), 0, Max);
			return clipped * clipped;
		}

		static constexpr i32 NormalizationK = Max;
	};

	template <i32 Max>
	struct [[maybe_unused]] ReLU
	{
		static constexpr u8 Id = 2;

		static inline auto activate(i16 x) -> i32
		{
			return std::max(static_cast<i32>(x), 0);
		}

		static constexpr i32 NormalizationK = 1;
	};
}
