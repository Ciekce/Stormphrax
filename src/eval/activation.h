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
#include "../util/simd.h"

namespace stormphrax::eval::activation
{
	template <Score Max>
	struct [[maybe_unused]] ClippedReLU
	{
		static constexpr u8 Id = 0;

		static inline auto activate(util::Simd::Register x)
		{
			static const auto max = util::Simd::set1(Max);

			return util::Simd::clamp16(x, util::Simd::zero(), max);
		}

		static constexpr i32 NormalizationK = 1;
	};

	template <Score Max>
	struct [[maybe_unused]] SquaredClippedReLU
	{
		static constexpr u8 Id = 1;

		static inline auto activate(util::Simd::Register x)
		{
			static const auto max = util::Simd::set1(Max);

			const auto clipped = util::Simd::clamp16(x, util::Simd::zero(), max);
			return util::Simd::mul16(clipped, clipped);
		}

		static constexpr i32 NormalizationK = Max;
	};

	template <Score Max>
	struct [[maybe_unused]] ReLU
	{
		static constexpr u8 Id = 2;

		static inline auto activate(util::Simd::Register x)
		{
			return util::Simd::max16(x, util::Simd::zero());
		}

		static constexpr i32 NormalizationK = 1;
	};
}
