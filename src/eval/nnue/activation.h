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
#include <cmath>
#include <concepts>
#include <exception>
#include <type_traits>

#include "../../util/simd.h"

namespace stormphrax::eval::nnue::activation {
    struct [[maybe_unused]] Identity {
        static constexpr u8 kId = 3;

        template <typename T, T _unused>
        SP_ALWAYS_INLINE_NDEBUG static inline util::simd::PromotedVector<T> activateDotAccumulate(
            util::simd::PromotedVector<T> sum,
            util::simd::Vector<T> inputs,
            util::simd::Vector<T> weights
        ) {
            using namespace util::simd;

            return mulAddAdjAcc<T>(sum, inputs, weights);
        }

        template <typename T, T _unused>
        SP_ALWAYS_INLINE_NDEBUG static inline util::simd::PromotedVector<T> activateDotAccumulate(
            util::simd::PromotedVector<T> sum,
            util::simd::Vector<T> inputs1,
            util::simd::Vector<T> inputs2,
            util::simd::Vector<T> weights
        ) {
            using namespace util::simd;

            const auto products = mulLo<T>(inputs1, weights);
            return mulAddAdjAcc<T>(sum, products, inputs2);
        }

        template <typename OutputType>
        SP_ALWAYS_INLINE_NDEBUG static inline OutputType output(OutputType value) {
            return value;
        }
    };

    struct [[maybe_unused]] ReLU {
        static constexpr u8 kId = 2;

        template <std::integral T, T _unused>
        SP_ALWAYS_INLINE_NDEBUG static inline util::simd::PromotedVector<T> activateDotAccumulate(
            util::simd::PromotedVector<T> sum,
            util::simd::Vector<T> inputs,
            util::simd::Vector<T> weights
        ) {
            using namespace util::simd;

            const auto activated = max<T>(inputs, zero<T>());
            return mulAddAdjAcc<T>(sum, activated, weights);
        }

        template <std::integral T, T _unused>
        SP_ALWAYS_INLINE_NDEBUG static inline util::simd::PromotedVector<T> activateDotAccumulate(
            util::simd::PromotedVector<T> sum,
            util::simd::Vector<T> inputs1,
            util::simd::Vector<T> inputs2,
            util::simd::Vector<T> weights
        ) {
            using namespace util::simd;

            const auto activated1 = max<T>(inputs1, zero<T>());
            const auto activated2 = max<T>(inputs2, zero<T>());

            const auto products = mulLo<T>(activated1, weights);
            return mulAddAdjAcc<T>(sum, products, activated2);
        }

        template <typename T>
        SP_ALWAYS_INLINE_NDEBUG static inline T output(T value) {
            return value;
        }
    };

    struct [[maybe_unused]] ClippedReLU {
        static constexpr u8 kId = 0;

        template <std::integral T, T kMax>
        SP_ALWAYS_INLINE_NDEBUG static inline util::simd::PromotedVector<T> activateDotAccumulate(
            util::simd::PromotedVector<T> sum,
            util::simd::Vector<T> inputs,
            util::simd::Vector<T> weights
        ) {
            using namespace util::simd;

            static const auto max = set1(kMax);

            const auto clipped = clamp<T>(inputs, zero<T>(), max);
            return mulAddAdjAcc<T>(sum, clipped, weights);
        }

        template <std::integral T, T kMax>
        SP_ALWAYS_INLINE_NDEBUG static inline util::simd::PromotedVector<T> activateDotAccumulate(
            util::simd::PromotedVector<T> sum,
            util::simd::Vector<T> inputs1,
            util::simd::Vector<T> inputs2,
            util::simd::Vector<T> weights
        ) {
            using namespace util::simd;

            static const auto max = set1(kMax);

            const auto clipped1 = clamp<T>(inputs1, zero<T>(), max);
            const auto clipped2 = clamp<T>(inputs2, zero<T>(), max);

            const auto products = mulLo<T>(clipped1, weights);
            return mulAddAdjAcc<T>(sum, clipped2, products);
        }

        template <typename T>
        SP_ALWAYS_INLINE_NDEBUG static inline T output(T value) {
            return value;
        }
    };

    struct [[maybe_unused]] SquaredClippedReLU {
        static constexpr u8 kId = 1;

        template <std::integral T, T kMax>
        SP_ALWAYS_INLINE_NDEBUG static inline util::simd::PromotedVector<T> activateDotAccumulate(
            util::simd::PromotedVector<T> sum,
            util::simd::Vector<T> inputs,
            util::simd::Vector<T> weights
        ) {
            using namespace util::simd;

            static const auto max = set1(kMax);

            const auto clipped = clamp<T>(inputs, zero<T>(), max);
            const auto crelu = mulLo<T>(clipped, weights);
            return mulAddAdjAcc<T>(sum, crelu, clipped);
        }

        template <typename T, T kMax>
        SP_ALWAYS_INLINE_NDEBUG static inline T output(T value) {
            return value / kMax;
        }
    };
} // namespace stormphrax::eval::nnue::activation
