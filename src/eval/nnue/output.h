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

#include "../../types.h"

#include <type_traits>
#include <concepts>

#include "../../position/boards.h"
#include "../../util/bits.h"

namespace stormphrax::eval::nnue::output
{
	template <typename T>
	concept OutputBucketing = requires
	{
		{ T::BucketCount } -> std::same_as<const u32 &>;
		{ T::getBucket(BitboardSet{}) } -> std::same_as<u32>;
	};

	struct [[maybe_unused]] Single
	{
		static constexpr u32 BucketCount = 1;

		static constexpr auto getBucket(const BitboardSet &) -> u32
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

		static inline auto getBucket(const BitboardSet &bbs) -> u32
		{
			constexpr auto Div = 32 / Count;
			return (bbs.occupancy().popcount() - 2) / Div;
		}
	};

	struct [[maybe_unused]] Ocb
	{
		static constexpr u32 BucketCount = 2;

		static inline auto getBucket(const BitboardSet &bbs) -> u32
		{
			return (!bbs.blackBishops().empty()
					&& !bbs.whiteBishops().empty()
					&& (bbs.blackBishops() & boards::LightSquares).empty()
						!= (bbs.whiteBishops() & boards::LightSquares).empty())
				? 1 : 0;
		}
	};

	template <OutputBucketing L, OutputBucketing R>
		requires (!std::is_same_v<L, Single> && !std::is_same_v<R, Single>)
	struct [[maybe_unused]] Combo
	{
		static constexpr u32 BucketCount = L::BucketCount * R::BucketCount;

		static inline auto getBucket(const BitboardSet &bbs) -> u32
		{
			return L::getBucket(bbs) * R::BucketCount + R::getBucket(bbs);
		}
	};
}
