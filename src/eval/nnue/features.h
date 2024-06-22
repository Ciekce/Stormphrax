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
		static constexpr u32 InputSize = 768;

		static constexpr u32 BucketCount = 1;
		static constexpr u32 RefreshTableSize = 1;

		static constexpr bool IsMirrored = false;

		static constexpr auto transformFeatureSquare([[maybe_unused]] Square sq, [[maybe_unused]] Square kingSq)
		{
			return sq;
		}

		static constexpr auto getBucket([[maybe_unused]] Color c, [[maybe_unused]] Square kingSq)
		{
			return 0;
		}

		static constexpr auto getRefreshTableEntry([[maybe_unused]] Color c, [[maybe_unused]] Square kingSq)
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
		static constexpr u32 InputSize = 768;

		static constexpr auto BucketCount = *std::ranges::max_element(Buckets) + 1;
		static constexpr auto RefreshTableSize = BucketCount;

		static constexpr bool IsMirrored = false;

		static_assert(BucketCount > 1, "use SingleBucket for single-bucket arches");

		static constexpr auto transformFeatureSquare(Square sq, [[maybe_unused]] Square kingSq)
		{
			return sq;
		}

		static constexpr auto getBucket(Color c, Square kingSq)
		{
			if (c == Color::Black)
				kingSq = flipSquareRank(kingSq);
			return Buckets[static_cast<i32>(kingSq)];
		}

		static constexpr auto getRefreshTableEntry(Color c, Square kingSq)
		{
			return getBucket(c, kingSq);
		}

		static constexpr auto refreshRequired(Color c, Square prevKingSq, Square kingSq)
		{
			assert(c != Color::None);

			assert(prevKingSq != Square::None);
			assert(kingSq != Square::None);

			if (c == Color::Black)
			{
				prevKingSq = flipSquareRank(prevKingSq);
				kingSq = flipSquareRank(kingSq);
			}

			return Buckets[static_cast<i32>(prevKingSq)] != Buckets[static_cast<i32>(kingSq)];
		}
	};

	using HalfKa [[maybe_unused]] = KingBuckets<
	     0,  1,  2,  3,  4,  5,  6,  7,
		 8,  9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39,
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63
	>;

	template <u32... BucketIndices>
	struct [[maybe_unused]] KingBucketsMirrored
	{
		static_assert(sizeof...(BucketIndices) == 32);

	private:
		static constexpr auto Buckets = []
		{
			constexpr auto HalfBuckets = std::array{BucketIndices...};

			std::array<u32, 64> dst{};

			for (u32 rank = 0; rank < 8; ++rank)
			{
				for (u32 file = 0; file < 4; ++file)
				{
					const auto srcIdx = rank * 4 + file;
					const auto dstIdx = rank * 8 + file;

					dst[dstIdx      ] = HalfBuckets[srcIdx];
					dst[dstIdx ^ 0x7] = HalfBuckets[srcIdx];
				}
			}

			return dst;
		}();

	public:
		static constexpr u32 InputSize = 768;

		static constexpr auto BucketCount = *std::ranges::max_element(Buckets) + 1;
		static constexpr auto RefreshTableSize = BucketCount * 2;

		static constexpr bool IsMirrored = true;

		static constexpr auto transformFeatureSquare(Square sq, Square kingSq)
		{
			const bool flipped = squareFile(kingSq) > 3;
			return flipped ? flipSquareFile(sq) : sq;
		}

		static constexpr auto getBucket(Color c, Square kingSq)
		{
			if (c == Color::Black)
				kingSq = flipSquareRank(kingSq);
			return Buckets[static_cast<i32>(kingSq)];
		}

		static constexpr auto getRefreshTableEntry(Color c, Square kingSq)
		{
			if (c == Color::Black)
				kingSq = flipSquareRank(kingSq);
			const bool flipped = squareFile(kingSq) > 3;
			return Buckets[static_cast<i32>(kingSq)] * 2 + flipped;
		}

		static constexpr auto refreshRequired(Color c, Square prevKingSq, Square kingSq)
		{
			assert(c != Color::None);

			assert(prevKingSq != Square::None);
			assert(kingSq != Square::None);

			const bool prevFlipped = squareFile(prevKingSq) > 3;
			const bool     flipped = squareFile(    kingSq) > 3;

			if (prevFlipped != flipped)
				return true;

			if (flipped)
				kingSq = flipSquareFile(kingSq);

			if (c == Color::Black)
			{
				prevKingSq = flipSquareRank(prevKingSq);
				kingSq = flipSquareRank(kingSq);
			}

			return Buckets[static_cast<i32>(prevKingSq)] != Buckets[static_cast<i32>(kingSq)];
		}
	};

	using HalfKaMirrored [[maybe_unused]] = KingBucketsMirrored<
	     0,  1,  2,  3,
		 4,  5,  6,  7,
		 8,  9, 10, 11,
		12, 13, 14, 15,
		16, 17, 18, 19,
		20, 21, 22, 23,
		24, 25, 26, 27,
		28, 29, 30, 31
	>;
}
