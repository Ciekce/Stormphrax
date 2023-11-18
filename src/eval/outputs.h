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

#include "../position/boards.h"
#include "../util/bits.h"

namespace stormphrax::eval::outputs
{
	struct [[maybe_unused]] SingleOutput
	{
		static constexpr u32 BucketCount = 1;

		static inline auto getBucket([[maybe_unused]] const PositionBoards &boards) -> u32
		{
			return 0;
		}
	};

	template <u32 Count>
	struct [[maybe_unused]] MaterialCount
	{
		static_assert(Count > 0 && util::resetLsb(Count) == 0);
		static_assert(Count <= 32);

		static constexpr u32 BucketCount = Count;

		static inline auto getBucket(const PositionBoards &boards) -> u32
		{
			constexpr auto Div = 32 / Count;
			return (boards.occupancy().popcount() - 2) / Div;
		}
	};

	struct [[maybe_unused]] Ocb
	{
		static constexpr u32 BucketCount = 2;

		static inline auto getBucket(const PositionBoards &boards) -> u32
		{
			return (!boards.blackBishops().empty()
					&& !boards.whiteBishops().empty()
					&& (boards.blackBishops() & boards::LightSquares).empty()
						!= (boards.whiteBishops() & boards::LightSquares).empty())
				? 1 : 0;
		}
	};

	template <typename L, typename R>
	struct [[maybe_unused]] Combo
	{
		static constexpr u32 BucketCount = L::BucketCount * R::BucketCount;

		static inline auto getBucket(const PositionBoards &boards) -> u32
		{
			return L::getBucket(boards) * R::BucketCount + R::getBucket(boards);
		}
	};
}
