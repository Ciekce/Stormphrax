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
#include <array>
#include <istream>
#include <ostream>
#include <span>

#include "../../core.h"
#include "../../position/boards.h"
#include "../../util/multi_array.h"
#include "../../util/simd.h"
#include "features.h"
#include "io.h"

namespace stormphrax::eval::nnue {
    template <typename Ft>
    class Accumulator {
    private:
        using Type = typename Ft::OutputType;

        static constexpr auto kInputCount = Ft::kInputCount;
        static constexpr auto kWeightCount = Ft::kWeightCount;
        static constexpr auto kOutputCount = Ft::kOutputCount;

    public:
        [[nodiscard]] inline std::span<const Type, kOutputCount> black() const {
            return m_outputs[0];
        }

        [[nodiscard]] inline std::span<const Type, kOutputCount> white() const {
            return m_outputs[1];
        }

        [[nodiscard]] inline std::span<const Type, kOutputCount> forColor(Color c) const {
            assert(c != Colors::kNone);
            return m_outputs[c.idx()];
        }

        [[nodiscard]] inline std::span<Type, kOutputCount> black() {
            return m_outputs[0];
        }

        [[nodiscard]] inline std::span<Type, kOutputCount> white() {
            return m_outputs[1];
        }

        [[nodiscard]] inline std::span<Type, kOutputCount> forColor(Color c) {
            assert(c != Colors::kNone);
            return m_outputs[c.idx()];
        }

        inline void initBoth(const Ft& featureTransformer) {
            std::ranges::copy(featureTransformer.biases, m_outputs[0].begin());
            std::ranges::copy(featureTransformer.biases, m_outputs[1].begin());
        }

        inline void subAddFrom(const Accumulator<Ft>& src, const Ft& featureTransformer, Color c, u32 sub, u32 add) {
            assert(sub < kInputCount);
            assert(add < kInputCount);

            subAdd(src.forColor(c), forColor(c), featureTransformer.weights, sub * kOutputCount, add * kOutputCount);
        }

        inline void subSubAddFrom(
            const Accumulator<Ft>& src,
            const Ft& featureTransformer,
            Color c,
            u32 sub0,
            u32 sub1,
            u32 add
        ) {
            assert(sub0 < kInputCount);
            assert(sub1 < kInputCount);
            assert(add < kInputCount);

            subSubAdd(
                src.forColor(c),
                forColor(c),
                featureTransformer.weights,
                sub0 * kOutputCount,
                sub1 * kOutputCount,
                add * kOutputCount
            );
        }

        inline void subSubAddAddFrom(
            const Accumulator<Ft>& src,
            const Ft& featureTransformer,
            Color c,
            u32 sub0,
            u32 sub1,
            u32 add0,
            u32 add1
        ) {
            assert(sub0 < kInputCount);
            assert(sub1 < kInputCount);
            assert(add0 < kInputCount);
            assert(add1 < kInputCount);

            subSubAddAdd(
                src.forColor(c),
                forColor(c),
                featureTransformer.weights,
                sub0 * kOutputCount,
                sub1 * kOutputCount,
                add0 * kOutputCount,
                add1 * kOutputCount
            );
        }

        inline void activateFeature(const Ft& featureTransformer, Color c, u32 feature) {
            assert(feature < kInputCount);
            add(forColor(c), featureTransformer.weights, feature * kOutputCount);
        }

        inline void deactivateFeature(const Ft& featureTransformer, Color c, u32 feature) {
            assert(feature < kInputCount);
            sub(forColor(c), featureTransformer.weights, feature * kOutputCount);
        }

        inline void activateFourFeatures(
            const Ft& featureTransformer,
            Color c,
            u32 feature0,
            u32 feature1,
            u32 feature2,
            u32 feature3
        ) {
            assert(feature0 < kInputCount);
            assert(feature1 < kInputCount);
            assert(feature2 < kInputCount);
            assert(feature3 < kInputCount);
            addAddAddAdd(
                forColor(c),
                featureTransformer.weights,
                feature0 * kOutputCount,
                feature1 * kOutputCount,
                feature2 * kOutputCount,
                feature3 * kOutputCount
            );
        }

        inline void deactivateFourFeatures(
            const Ft& featureTransformer,
            Color c,
            u32 feature0,
            u32 feature1,
            u32 feature2,
            u32 feature3
        ) {
            assert(feature0 < kInputCount);
            assert(feature1 < kInputCount);
            assert(feature2 < kInputCount);
            assert(feature3 < kInputCount);
            subSubSubSub(
                forColor(c),
                featureTransformer.weights,
                feature0 * kOutputCount,
                feature1 * kOutputCount,
                feature2 * kOutputCount,
                feature3 * kOutputCount
            );
        }

        inline void copyFrom(Color c, const Accumulator<Ft>& other) {
            const auto idx = c.idx();
            std::ranges::copy(other.m_outputs[idx], m_outputs[idx].begin());
        }

    private:
        SP_SIMD_ALIGNAS util::MultiArray<Type, 2, kOutputCount> m_outputs;

        static inline void subAdd(
            std::span<const Type, kOutputCount> src,
            std::span<Type, kOutputCount> dst,
            std::span<const Type, kWeightCount> delta,
            u32 subOffset,
            u32 addOffset
        ) {
            assert(subOffset + kOutputCount <= delta.size());
            assert(addOffset + kOutputCount <= delta.size());

            for (u32 i = 0; i < kOutputCount; ++i) {
                dst[i] = src[i] + delta[addOffset + i] - delta[subOffset + i];
            }
        }

        static inline void subSubAdd(
            std::span<const Type, kOutputCount> src,
            std::span<Type, kOutputCount> dst,
            std::span<const Type, kWeightCount> delta,
            u32 subOffset0,
            u32 subOffset1,
            u32 addOffset
        ) {
            assert(subOffset0 + kOutputCount <= delta.size());
            assert(subOffset1 + kOutputCount <= delta.size());
            assert(addOffset + kOutputCount <= delta.size());

            for (u32 i = 0; i < kOutputCount; ++i) {
                dst[i] = src[i] + delta[addOffset + i] - delta[subOffset0 + i] - delta[subOffset1 + i];
            }
        }

        static inline void subSubAddAdd(
            std::span<const Type, kOutputCount> src,
            std::span<Type, kOutputCount> dst,
            std::span<const Type, kWeightCount> delta,
            u32 subOffset0,
            u32 subOffset1,
            u32 addOffset0,
            u32 addOffset1
        ) {
            assert(subOffset0 + kOutputCount <= delta.size());
            assert(subOffset1 + kOutputCount <= delta.size());
            assert(addOffset0 + kOutputCount <= delta.size());
            assert(addOffset1 + kOutputCount <= delta.size());

            for (u32 i = 0; i < kOutputCount; ++i) {
                dst[i] = src[i] + delta[addOffset0 + i] - delta[subOffset0 + i] + delta[addOffset1 + i]
                       - delta[subOffset1 + i];
            }
        }

        static __attribute__((always_inline)) inline void addAddAddAdd(
            std::span<Type, kOutputCount> accumulator,
            std::span<const Type, kWeightCount> delta,
            u32 addOffset0,
            u32 addOffset1,
            u32 addOffset2,
            u32 addOffset3
        ) {
            assert(addOffset0 + kOutputCount <= delta.size());
            assert(addOffset1 + kOutputCount <= delta.size());
            assert(addOffset2 + kOutputCount <= delta.size());
            assert(addOffset3 + kOutputCount <= delta.size());

            for (u32 i = 0; i < kOutputCount; ++i) {
                accumulator[i] +=
                    delta[addOffset0 + i] + delta[addOffset1 + i] + delta[addOffset2 + i] + delta[addOffset3 + i];
            }
        }

        static __attribute__((always_inline)) inline void subSubSubSub(
            std::span<Type, kOutputCount> accumulator,
            std::span<const Type, kWeightCount> delta,
            u32 subOffset0,
            u32 subOffset1,
            u32 subOffset2,
            u32 subOffset3
        ) {
            assert(subOffset0 + kOutputCount <= delta.size());
            assert(subOffset1 + kOutputCount <= delta.size());
            assert(subOffset2 + kOutputCount <= delta.size());
            assert(subOffset3 + kOutputCount <= delta.size());

            for (u32 i = 0; i < kOutputCount; ++i) {
                accumulator[i] -=
                    delta[subOffset0 + i] + delta[subOffset1 + i] + delta[subOffset2 + i] + delta[subOffset3 + i];
            }
        }

        static inline void add(
            std::span<Type, kOutputCount> accumulator,
            std::span<const Type, kWeightCount> delta,
            u32 offset
        ) {
            assert(offset + kOutputCount <= delta.size());

            for (u32 i = 0; i < kOutputCount; ++i) {
                accumulator[i] += delta[offset + i];
            }
        }

        static inline void sub(
            std::span<Type, kOutputCount> accumulator,
            std::span<const Type, kWeightCount> delta,
            u32 offset
        ) {
            assert(offset + kOutputCount <= delta.size());

            for (u32 i = 0; i < kOutputCount; ++i) {
                accumulator[i] -= delta[offset + i];
            }
        }
    };

    template <typename Acc>
    struct RefreshTableEntry {
        Acc accumulator{};
        std::array<BitboardSet, 2> bbs{};

        [[nodiscard]] BitboardSet& colorBbs(Color c) {
            return bbs[c.idx()];
        }
    };

    template <typename Ft, u32 kSize>
    struct RefreshTable {
        std::array<RefreshTableEntry<Accumulator<Ft>>, kSize> table{};

        inline void init(const Ft& featureTransformer) {
            for (auto& entry : table) {
                entry.accumulator.initBoth(featureTransformer);
                entry.bbs.fill(BitboardSet{});
            }
        }
    };

    template <typename Type, u32 kOutputs, typename FeatureSet = features::SingleBucket>
    struct FeatureTransformer {
        using WeightType = Type;
        using OutputType = Type;

        using InputFeatureSet = FeatureSet;

        using Accumulator = Accumulator<FeatureTransformer<Type, kOutputs, FeatureSet>>;
        using RefreshTable =
            RefreshTable<FeatureTransformer<Type, kOutputs, FeatureSet>, FeatureSet::kRefreshTableSize>;

        static constexpr auto kInputCount = InputFeatureSet::kBucketCount * FeatureSet::kInputSize;
        static constexpr auto kOutputCount = kOutputs;

        static constexpr auto kWeightCount = kInputCount * kOutputCount;
        static constexpr auto kBiasCount = kOutputCount;

        static_assert(kInputCount > 0);
        static_assert(kOutputCount > 0);

        SP_SIMD_ALIGNAS std::array<WeightType, kWeightCount> weights;
        SP_SIMD_ALIGNAS std::array<OutputType, kBiasCount> biases;

        inline bool readFrom(IParamStream& stream) {
            return stream.read(weights) && stream.read(biases);
        }

        inline bool writeTo(IParamStream& stream) const {
            return stream.write(weights) && stream.write(biases);
        }
    };
} // namespace stormphrax::eval::nnue
