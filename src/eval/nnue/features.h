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

#include <algorithm>

#include "../../core.h"

namespace stormphrax::eval::nnue::features
{
	struct [[maybe_unused]] SingleBucket
	{
		static constexpr u32 BucketCount = 1;

		static constexpr auto getBucket([[maybe_unused]] Color c, [[maybe_unused]] Square kingSq)
		{
			return 0;
		}

		static constexpr auto refreshRequired([[maybe_unused]] Color c,
			[[maybe_unused]] Square prevKingSq, [[maybe_unused]] Square kingSq)
		{
			return false;
		}
	};

	template <u32... BucketIndices>
	struct [[maybe_unused]] KingBuckets
	{
		static_assert(sizeof...(BucketIndices) == 64);

	private:
		static constexpr auto Buckets = std::array{BucketIndices...};

	public:
		static constexpr auto BucketCount = *std::ranges::max_element(Buckets) + 1;

		static_assert(BucketCount > 1, "use SingleBucket for single-bucket arches");

		static constexpr auto getBucket(Color c, Square kingSq)
		{
			if (c == Color::Black)
				kingSq = flipSquare(kingSq);
			return Buckets[static_cast<i32>(kingSq)];
		}

		static constexpr auto refreshRequired(Color c, Square prevKingSq, Square kingSq)
		{
			assert(c != Color::None);

			assert(prevKingSq != Square::None);
			assert(kingSq != Square::None);

			if (c == Color::Black)
			{
				prevKingSq = flipSquare(prevKingSq);
				kingSq = flipSquare(kingSq);
			}

			return Buckets[static_cast<i32>(prevKingSq)] != Buckets[static_cast<i32>(kingSq)];
		}
	};
}
