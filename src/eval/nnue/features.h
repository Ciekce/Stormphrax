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

#include "../../types.h"

#include <algorithm>

#include "../../core.h"

//TODO literally what is going on in here
namespace stormphrax::eval::nnue::features
{
	namespace internal
	{
		constexpr u32 PieceStride = 64;
		constexpr u32 ColorStride = 6 * PieceStride;
		constexpr u32 BucketStride = 2 * ColorStride;
	}

	struct [[maybe_unused]] SingleBucket
	{
		static constexpr u32 RefreshTableCount = 1;
		static constexpr u32 BucketCount = 1;

		static constexpr auto getRefreshTableIdx([[maybe_unused]] Color c, [[maybe_unused]] Square kingSq)
		{
			return 0;
		}

		static constexpr auto getBucket([[maybe_unused]] Color c, [[maybe_unused]] Square kingSq)
		{
			return 0;
		}

		static constexpr auto refreshRequired([[maybe_unused]] Color c,
			[[maybe_unused]] Square prevKingSq, [[maybe_unused]] Square kingSq)
		{
			return false;
		}

		[[nodiscard]] static constexpr auto featureIndex(Color c, Piece piece, Square sq, Square king) -> u32
		{
			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			using namespace internal;

			const auto type = static_cast<u32>(pieceType(piece));

			const u32 color = pieceColor(piece) == c ? 0 : 1;

			if (c == Color::Black)
				sq = flipSquareRank(sq);

			const auto bucketOffset = getBucket(c, king) * BucketStride;
			return bucketOffset + color * ColorStride + type * PieceStride + static_cast<u32>(sq);
		}
	};

	template <u32... BucketIndices>
	struct [[maybe_unused]] KingBuckets
	{
		static_assert(sizeof...(BucketIndices) == 64);

	private:
		static constexpr auto Buckets = std::array{BucketIndices...};

	public:
		static constexpr auto RefreshTableCount = *std::ranges::max_element(Buckets) + 1;
		static constexpr auto BucketCount = RefreshTableCount;

		static_assert(BucketCount > 1, "use SingleBucket for single-bucket arches");

		static constexpr auto getRefreshTableIdx(Color c, Square kingSq)
		{
			if (c == Color::Black)
				kingSq = flipSquareRank(kingSq);
			return Buckets[static_cast<i32>(kingSq)];
		}

		static constexpr auto getBucket(Color c, Square kingSq)
		{
			return getRefreshTableIdx(c, kingSq);
		}

		static constexpr auto refreshRequired(Color c, Square prevKingSq, Square kingSq)
		{
			assert(c != Color::None);

			assert(prevKingSq != Square::None);
			assert(kingSq != Square::None);
			assert(prevKingSq != kingSq);

			if (c == Color::Black)
			{
				prevKingSq = flipSquareRank(prevKingSq);
				kingSq = flipSquareRank(kingSq);
			}

			return Buckets[static_cast<i32>(prevKingSq)] != Buckets[static_cast<i32>(kingSq)];
		}

		[[nodiscard]] static constexpr auto featureIndex(Color c, Piece piece, Square sq, Square king) -> u32
		{
			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			using namespace internal;

			const auto type = static_cast<u32>(pieceType(piece));

			const u32 color = pieceColor(piece) == c ? 0 : 1;

			if (c == Color::Black)
				sq = flipSquareRank(sq);

			const auto bucketOffset = getBucket(c, king) * BucketStride;
			return bucketOffset + color * ColorStride + type * PieceStride + static_cast<u32>(sq);
		}
	};

	template <u32... BucketIndices>
	struct [[maybe_unused]] KingBucketsMirrored
	{
		static_assert(sizeof...(BucketIndices) == 32);

	private:
		static constexpr auto Buckets = []
		{
			const auto halfBuckets = std::array{BucketIndices...};
			const auto mirrorOffset = *std::ranges::max_element(halfBuckets) + 1;

			std::array<u32, 64> buckets{};

			for (u32 file = 0; file < 8; ++file)
			{
				for (u32 rank = 0; rank < 8; ++rank)
				{
					if (file > 3)
						buckets[rank * 8 + file] = mirrorOffset + halfBuckets[rank * 4 + (7 - file)];
					else buckets[rank * 8 + file] = halfBuckets[rank * 4 + file];
				}
			}

			return buckets;
		}();

	public:
		static constexpr auto RefreshTableCount = (*std::ranges::max_element(Buckets) + 1);
		static constexpr auto BucketCount = RefreshTableCount / 2;

		static_assert(BucketCount > 1, "use SingleBucket for single-bucket arches");

		static constexpr auto getRefreshTableIdx(Color c, Square kingSq)
		{
			if (c == Color::Black)
				kingSq = flipSquareRank(kingSq);
			return Buckets[static_cast<i32>(kingSq)];
		}

		static constexpr auto getBucket(Color c, Square kingSq)
		{
			return getRefreshTableIdx(c, kingSq) % BucketCount;
		}

		static constexpr auto refreshRequired(Color c, Square prevKingSq, Square kingSq)
		{
			assert(c != Color::None);

			assert(prevKingSq != Square::None);
			assert(kingSq != Square::None);
			assert(prevKingSq != kingSq);

			if (c == Color::Black)
			{
				prevKingSq = flipSquareRank(prevKingSq);
				kingSq = flipSquareRank(kingSq);
			}

			return Buckets[static_cast<i32>(prevKingSq)] != Buckets[static_cast<i32>(kingSq)];
		}

		[[nodiscard]] static constexpr auto featureIndex(Color c, Piece piece, Square sq, Square king) -> u32
		{
			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			using namespace internal;

			if (squareFile(king) > 3)
				sq = flipSquareFile(sq);

			const auto type = static_cast<u32>(pieceType(piece));

			const u32 color = pieceColor(piece) == c ? 0 : 1;

			if (c == Color::Black)
				sq = flipSquareRank(sq);

			const auto bucketOffset = getBucket(c, king) * BucketStride;
			return bucketOffset + color * ColorStride + type * PieceStride + static_cast<u32>(sq);
		}
	};
}
