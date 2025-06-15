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

#include "../../types.h"

#include <algorithm>

#include "../../core.h"

namespace stormphrax::eval::nnue::features {
    struct [[maybe_unused]] SingleBucket {
        static constexpr u32 kInputSize = 768;

        static constexpr u32 kBucketCount = 1;
        static constexpr u32 kRefreshTableSize = 1;

        static constexpr bool kIsMirrored = false;
        static constexpr bool kMergedKings = false;

        static constexpr Square transformFeatureSquare([[maybe_unused]] Square sq, [[maybe_unused]] Square kingSq) {
            return sq;
        }

        static constexpr u32 getBucket([[maybe_unused]] Color c, [[maybe_unused]] Square kingSq) {
            return 0;
        }

        static constexpr u32 getRefreshTableEntry([[maybe_unused]] Color c, [[maybe_unused]] Square kingSq) {
            return 0;
        }

        static constexpr bool refreshRequired(
            [[maybe_unused]] Color c,
            [[maybe_unused]] Square prevKingSq,
            [[maybe_unused]] Square kingSq
        ) {
            return false;
        }
    };

    template <u32... kBucketIndices>
    struct [[maybe_unused]] KingBuckets {
        static_assert(sizeof...(kBucketIndices) == 64);

    private:
        static constexpr auto kBuckets = std::array{kBucketIndices...};

    public:
        static constexpr u32 kInputSize = 768;

        static constexpr auto kBucketCount = *std::ranges::max_element(kBuckets) + 1;
        static constexpr auto kRefreshTableSize = kBucketCount;

        static constexpr bool kIsMirrored = false;
        static constexpr bool kMergedKings = false;

        static_assert(kBucketCount > 1, "use SingleBucket for single-bucket arches");

        static constexpr Square transformFeatureSquare(Square sq, [[maybe_unused]] Square kingSq) {
            return sq;
        }

        static constexpr u32 getBucket(Color c, Square kingSq) {
            if (c == Color::kBlack) {
                kingSq = flipSquareRank(kingSq);
            }
            return kBuckets[static_cast<i32>(kingSq)];
        }

        static constexpr u32 getRefreshTableEntry(Color c, Square kingSq) {
            return getBucket(c, kingSq);
        }

        static constexpr bool refreshRequired(Color c, Square prevKingSq, Square kingSq) {
            assert(c != Color::kNone);

            assert(prevKingSq != Square::kNone);
            assert(kingSq != Square::kNone);

            if (c == Color::kBlack) {
                prevKingSq = flipSquareRank(prevKingSq);
                kingSq = flipSquareRank(kingSq);
            }

            return kBuckets[static_cast<i32>(prevKingSq)] != kBuckets[static_cast<i32>(kingSq)];
        }
    };

    using HalfKa [[maybe_unused]] = KingBuckets<
        // clang-format off
         0,  1,  2,  3,  4,  5,  6,  7,
         8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63
        // clang-format on
        >;

    enum class MirroredKingSide {
        kAbcd,
        kEfgh,
    };

    template <MirroredKingSide kSide, u32... kBucketIndices>
    struct [[maybe_unused]] KingBucketsMirrored {
        static_assert(sizeof...(kBucketIndices) == 32);

    private:
        static constexpr auto kBuckets = [] {
            constexpr auto kHalfBuckets = std::array{kBucketIndices...};

            std::array<u32, 64> dst{};

            for (u32 rank = 0; rank < 8; ++rank) {
                for (u32 file = 0; file < 4; ++file) {
                    const auto srcIdx = rank * 4 + file;
                    const auto dstIdx = rank * 8 + file;

                    dst[dstIdx] = kHalfBuckets[srcIdx];
                    dst[dstIdx ^ 0x7] = kHalfBuckets[srcIdx];
                }
            }

            return dst;
        }();

        static constexpr bool shouldFlip(Square kingSq) {
            if constexpr (kSide == MirroredKingSide::kAbcd) {
                return squareFile(kingSq) > 3;
            } else {
                return squareFile(kingSq) <= 3;
            }
        }

    public:
        static constexpr u32 kInputSize = 768;

        static constexpr auto kBucketCount = *std::ranges::max_element(kBuckets) + 1;
        static constexpr auto kRefreshTableSize = kBucketCount * 2;

        static constexpr bool kIsMirrored = true;
        static constexpr bool kMergedKings = false;

        static constexpr Square transformFeatureSquare(Square sq, Square kingSq) {
            const bool flipped = shouldFlip(kingSq);
            return flipped ? flipSquareFile(sq) : sq;
        }

        static constexpr u32 getBucket(Color c, Square kingSq) {
            if (c == Color::kBlack) {
                kingSq = flipSquareRank(kingSq);
            }
            return kBuckets[static_cast<i32>(kingSq)];
        }

        static constexpr u32 getRefreshTableEntry(Color c, Square kingSq) {
            if (c == Color::kBlack) {
                kingSq = flipSquareRank(kingSq);
            }
            const bool flipped = shouldFlip(kingSq);
            return kBuckets[static_cast<i32>(kingSq)] * 2 + flipped;
        }

        static constexpr bool refreshRequired(Color c, Square prevKingSq, Square kingSq) {
            assert(c != Color::kNone);

            assert(prevKingSq != Square::kNone);
            assert(kingSq != Square::kNone);

            const bool prevFlipped = shouldFlip(prevKingSq);
            const bool flipped = shouldFlip(kingSq);

            if (prevFlipped != flipped) {
                return true;
            }

            if (c == Color::kBlack) {
                prevKingSq = flipSquareRank(prevKingSq);
                kingSq = flipSquareRank(kingSq);
            }

            return kBuckets[static_cast<i32>(prevKingSq)] != kBuckets[static_cast<i32>(kingSq)];
        }
    };

    template <MirroredKingSide kSide>
    using SingleBucketMirrored [[maybe_unused]] = KingBucketsMirrored<
        kSide,
        // clang-format off
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
        // clang-format on
        >;

    template <MirroredKingSide kSide>
    using HalfKaMirrored [[maybe_unused]] = KingBucketsMirrored<
        kSide,
        // clang-format off
         0,  1,  2,  3,
         4,  5,  6,  7,
         8,  9, 10, 11,
        12, 13, 14, 15,
        16, 17, 18, 19,
        20, 21, 22, 23,
        24, 25, 26, 27,
        28, 29, 30, 31
        // clang-format on
        >;

    //TODO verify that buckets work for merged kings
    template <MirroredKingSide kSide, u32... kBucketIndices>
    struct [[maybe_unused]] KingBucketsMergedMirrored : public KingBucketsMirrored<kSide, kBucketIndices...> {
        static constexpr u32 kInputSize = 704;
        static constexpr bool kMergedKings = true;
    };

    template <MirroredKingSide kSide>
    using HalfKaV2Mirrored [[maybe_unused]] = KingBucketsMergedMirrored<
        kSide,
        // clang-format off
         0,  1,  2,  3,
         4,  5,  6,  7,
         8,  9, 10, 11,
        12, 13, 14, 15,
        16, 17, 18, 19,
        20, 21, 22, 23,
        24, 25, 26, 27,
        28, 29, 30, 31
        // clang-format on
        >;
} // namespace stormphrax::eval::nnue::features
