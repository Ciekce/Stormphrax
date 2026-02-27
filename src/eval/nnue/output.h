/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include <concepts>
#include <type_traits>

#include "../../position/boards.h"
#include "../../util/bits.h"

namespace stormphrax::eval::nnue::output {
    template <typename T>
    concept OutputBucketing = requires {
        { T::kBucketCount } -> std::same_as<const u32&>;
        { T::getBucket(BitboardSet{}) } -> std::same_as<u32>;
    };

    struct [[maybe_unused]] Single {
        static constexpr u32 kBucketCount = 1;

        static constexpr u32 getBucket(const BitboardSet&) {
            return 0;
        }
    };

    template <u32 kCount>
    struct [[maybe_unused]] MaterialCount {
        static_assert(kCount > 0 && util::resetLsb(kCount) == 0);
        static_assert(kCount <= 32);

        static constexpr u32 kBucketCount = kCount;

        static inline u32 getBucket(const BitboardSet& bbs) {
            static constexpr auto kDiv = 32 / kCount;
            return (bbs.occupancy().popcount() - 2) / kDiv;
        }
    };

    struct [[maybe_unused]] Ocb {
        static constexpr u32 kBucketCount = 2;

        static inline u32 getBucket(const BitboardSet& bbs) {
            return (!bbs.blackBishops().empty() && !bbs.whiteBishops().empty()
                    && (bbs.blackBishops() & boards::kLightSquares).empty()
                           != (bbs.whiteBishops() & boards::kLightSquares).empty())
                     ? 1
                     : 0;
        }
    };

    template <OutputBucketing L, OutputBucketing R>
        requires(!std::is_same_v<L, Single> && !std::is_same_v<R, Single>)
    struct [[maybe_unused]] Combo {
        static constexpr u32 kBucketCount = L::kBucketCount * R::kBucketCount;

        static inline u32 getBucket(const BitboardSet& bbs) {
            return L::getBucket(bbs) * R::kBucketCount + R::getBucket(bbs);
        }
    };
} // namespace stormphrax::eval::nnue::output
