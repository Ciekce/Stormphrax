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

#include "../../../types.h"

#include <algorithm>

#include "../../../core.h"
#include "../../../util/static_vector.h"

namespace stormphrax::eval::nnue::features::psq {
    struct PsqFeaturesBase {
        static constexpr bool kThreatInputs = false;
        static constexpr u32 kThreatFeatures = 0;

        struct Updates {
            using PieceSquare = std::pair<Piece, Square>;

            // [black, white]
            std::array<bool, 2> refresh{};

            StaticVector<PieceSquare, 2> sub{};
            StaticVector<PieceSquare, 2> add{};

            inline void setRefresh(Color c) {
                refresh[c.idx()] = true;
            }

            [[nodiscard]] inline bool requiresRefresh(Color c) const {
                return refresh[c.idx()];
            }

            inline void pushSubAdd(Piece piece, Square src, Square dst) {
                sub.push({piece, src});
                add.push({piece, dst});
            }

            inline void pushSub(Piece piece, Square sq) {
                sub.push({piece, sq});
            }

            inline void pushAdd(Piece piece, Square sq) {
                add.push({piece, sq});
            }
        };
    };

    struct [[maybe_unused]] SingleBucket : PsqFeaturesBase {
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
    struct [[maybe_unused]] KingBuckets : PsqFeaturesBase {
        static_assert(sizeof...(kBucketIndices) == Squares::kCount);

    private:
        static constexpr std::array kBuckets = {kBucketIndices...};

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
            if (c == Colors::kBlack) {
                kingSq = kingSq.flipRank();
            }
            return kBuckets[kingSq.idx()];
        }

        static constexpr u32 getRefreshTableEntry(Color c, Square kingSq) {
            return getBucket(c, kingSq);
        }

        static constexpr bool refreshRequired(Color c, Square prevKingSq, Square kingSq) {
            assert(c != Colors::kNone);

            assert(prevKingSq != Squares::kNone);
            assert(kingSq != Squares::kNone);

            if (c == Colors::kBlack) {
                prevKingSq = prevKingSq.flipRank();
                kingSq = kingSq.flipRank();
            }

            return kBuckets[prevKingSq.idx()] != kBuckets[kingSq.idx()];
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
    struct [[maybe_unused]] KingBucketsMirrored : PsqFeaturesBase {
        static_assert(sizeof...(kBucketIndices) == Squares::kCount / 2);

    private:
        static constexpr auto kBuckets = [] {
            constexpr std::array kHalfBuckets = {kBucketIndices...};

            std::array<u32, Squares::kCount> dst{};

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
                return kingSq.file() > 3;
            } else {
                return kingSq.file() <= 3;
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
            return flipped ? sq.flipFile() : sq;
        }

        static constexpr u32 getBucket(Color c, Square kingSq) {
            if (c == Colors::kBlack) {
                kingSq = kingSq.flipRank();
            }
            return kBuckets[kingSq.idx()];
        }

        static constexpr u32 getRefreshTableEntry(Color c, Square kingSq) {
            if (c == Colors::kBlack) {
                kingSq = kingSq.flipRank();
            }
            const bool flipped = shouldFlip(kingSq);
            return kBuckets[kingSq.idx()] * 2 + flipped;
        }

        static constexpr bool refreshRequired(Color c, Square prevKingSq, Square kingSq) {
            assert(c != Colors::kNone);

            assert(prevKingSq != Squares::kNone);
            assert(kingSq != Squares::kNone);

            const bool prevFlipped = shouldFlip(prevKingSq);
            const bool flipped = shouldFlip(kingSq);

            if (prevFlipped != flipped) {
                return true;
            }

            if (c == Colors::kBlack) {
                prevKingSq = prevKingSq.flipRank();
                kingSq = kingSq.flipRank();
            }

            return kBuckets[prevKingSq.idx()] != kBuckets[kingSq.idx()];
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
    struct [[maybe_unused]] KingBucketsMergedMirrored : KingBucketsMirrored<kSide, kBucketIndices...> {
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

    template <typename FeatureSet>
    [[nodiscard]] constexpr u32 featureIndex(Color c, Piece piece, Square sq, Square king) {
        assert(c != Colors::kNone);
        assert(piece != Pieces::kNone);
        assert(sq != Squares::kNone);
        assert(king != Squares::kNone);

        constexpr u32 kColorStride = Squares::kCount * PieceTypes::kCount;
        constexpr u32 kPieceStride = Squares::kCount;

        const u32 type = piece.type().raw();

        const auto color = [piece, c]() -> u32 {
            if (FeatureSet::kMergedKings && piece.type() == PieceTypes::kKing) {
                return 0;
            }
            return piece.color() == c ? 0 : 1;
        }();

        if (c == Colors::kBlack) {
            sq = sq.flipRank();
        }

        sq = FeatureSet::transformFeatureSquare(sq, king);

        const auto bucketOffset = FeatureSet::getBucket(c, king) * FeatureSet::kInputSize;
        return bucketOffset + color * kColorStride + type * kPieceStride + sq.raw();
    }
} // namespace stormphrax::eval::nnue::features::psq
