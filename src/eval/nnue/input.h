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
    public:
        [[nodiscard]] inline const auto& black() const {
            return m_outputs[0];
        }

        [[nodiscard]] inline const auto& white() const {
            return m_outputs[1];
        }

        [[nodiscard]] inline const auto& forColor(Color c) const {
            assert(c != Color::None);
            return m_outputs[static_cast<i32>(c)];
        }

        [[nodiscard]] inline auto& black() {
            return m_outputs[0];
        }

        [[nodiscard]] inline auto& white() {
            return m_outputs[1];
        }

        [[nodiscard]] inline auto& forColor(Color c) {
            assert(c != Color::None);
            return m_outputs[static_cast<i32>(c)];
        }

        inline void initBoth(const Ft& featureTransformer) {
            std::ranges::copy(featureTransformer.biases, m_outputs[0].begin());
            std::ranges::copy(featureTransformer.biases, m_outputs[1].begin());
        }

        inline void subAddFrom(const Accumulator<Ft>& src, const Ft& featureTransformer, Color c, u32 sub, u32 add) {
            assert(sub < InputCount);
            assert(add < InputCount);

            subAdd(src.forColor(c), forColor(c), featureTransformer.weights, sub * OutputCount, add * OutputCount);
        }

        inline void subSubAddFrom(
            const Accumulator<Ft>& src,
            const Ft& featureTransformer,
            Color c,
            u32 sub0,
            u32 sub1,
            u32 add
        ) {
            assert(sub0 < InputCount);
            assert(sub1 < InputCount);
            assert(add < InputCount);

            subSubAdd(
                src.forColor(c),
                forColor(c),
                featureTransformer.weights,
                sub0 * OutputCount,
                sub1 * OutputCount,
                add * OutputCount
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
            assert(sub0 < InputCount);
            assert(sub1 < InputCount);
            assert(add0 < InputCount);
            assert(add1 < InputCount);

            subSubAddAdd(
                src.forColor(c),
                forColor(c),
                featureTransformer.weights,
                sub0 * OutputCount,
                sub1 * OutputCount,
                add0 * OutputCount,
                add1 * OutputCount
            );
        }

        inline void activateFeature(const Ft& featureTransformer, Color c, u32 feature) {
            assert(feature < InputCount);
            add(forColor(c), featureTransformer.weights, feature * OutputCount);
        }

        inline void deactivateFeature(const Ft& featureTransformer, Color c, u32 feature) {
            assert(feature < InputCount);
            sub(forColor(c), featureTransformer.weights, feature * OutputCount);
        }

        inline void activateFourFeatures(
            const Ft& featureTransformer,
            Color c,
            u32 feature0,
            u32 feature1,
            u32 feature2,
            u32 feature3
        ) {
            assert(feature0 < InputCount);
            assert(feature1 < InputCount);
            assert(feature2 < InputCount);
            assert(feature3 < InputCount);
            addAddAddAdd(
                forColor(c),
                featureTransformer.weights,
                feature0 * OutputCount,
                feature1 * OutputCount,
                feature2 * OutputCount,
                feature3 * OutputCount
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
            assert(feature0 < InputCount);
            assert(feature1 < InputCount);
            assert(feature2 < InputCount);
            assert(feature3 < InputCount);
            subSubSubSub(
                forColor(c),
                featureTransformer.weights,
                feature0 * OutputCount,
                feature1 * OutputCount,
                feature2 * OutputCount,
                feature3 * OutputCount
            );
        }

        inline void copyFrom(Color c, const Accumulator<Ft>& other) {
            const auto idx = static_cast<i32>(c);
            std::ranges::copy(other.m_outputs[idx], m_outputs[idx].begin());
        }

    private:
        using Type = typename Ft::OutputType;

        static constexpr auto InputCount = Ft::InputCount;
        static constexpr auto WeightCount = Ft::WeightCount;
        static constexpr auto OutputCount = Ft::OutputCount;

        SP_SIMD_ALIGNAS util::MultiArray<Type, 2, OutputCount> m_outputs;

        static inline void subAdd(
            std::span<const Type, OutputCount> src,
            std::span<Type, OutputCount> dst,
            std::span<const Type, WeightCount> delta,
            u32 subOffset,
            u32 addOffset
        ) {
            assert(subOffset + OutputCount <= delta.size());
            assert(addOffset + OutputCount <= delta.size());

            for (u32 i = 0; i < OutputCount; ++i) {
                dst[i] = src[i] + delta[addOffset + i] - delta[subOffset + i];
            }
        }

        static inline void subSubAdd(
            std::span<const Type, OutputCount> src,
            std::span<Type, OutputCount> dst,
            std::span<const Type, WeightCount> delta,
            u32 subOffset0,
            u32 subOffset1,
            u32 addOffset
        ) {
            assert(subOffset0 + OutputCount <= delta.size());
            assert(subOffset1 + OutputCount <= delta.size());
            assert(addOffset + OutputCount <= delta.size());

            for (u32 i = 0; i < OutputCount; ++i) {
                dst[i] = src[i] + delta[addOffset + i] - delta[subOffset0 + i] - delta[subOffset1 + i];
            }
        }

        static inline void subSubAddAdd(
            std::span<const Type, OutputCount> src,
            std::span<Type, OutputCount> dst,
            std::span<const Type, WeightCount> delta,
            u32 subOffset0,
            u32 subOffset1,
            u32 addOffset0,
            u32 addOffset1
        ) {
            assert(subOffset0 + OutputCount <= delta.size());
            assert(subOffset1 + OutputCount <= delta.size());
            assert(addOffset0 + OutputCount <= delta.size());
            assert(addOffset1 + OutputCount <= delta.size());

            for (u32 i = 0; i < OutputCount; ++i) {
                dst[i] = src[i] + delta[addOffset0 + i] - delta[subOffset0 + i] + delta[addOffset1 + i]
                       - delta[subOffset1 + i];
            }
        }

        static __attribute__((always_inline)) inline void addAddAddAdd(
            std::span<Type, OutputCount> accumulator,
            std::span<const Type, WeightCount> delta,
            u32 addOffset0,
            u32 addOffset1,
            u32 addOffset2,
            u32 addOffset3
        ) {
            assert(addOffset0 + OutputCount <= delta.size());
            assert(addOffset1 + OutputCount <= delta.size());
            assert(addOffset2 + OutputCount <= delta.size());
            assert(addOffset3 + OutputCount <= delta.size());

            for (u32 i = 0; i < OutputCount; ++i) {
                accumulator[i] +=
                    delta[addOffset0 + i] + delta[addOffset1 + i] + delta[addOffset2 + i] + delta[addOffset3 + i];
            }
        }

        static __attribute__((always_inline)) inline void subSubSubSub(
            std::span<Type, OutputCount> accumulator,
            std::span<const Type, WeightCount> delta,
            u32 subOffset0,
            u32 subOffset1,
            u32 subOffset2,
            u32 subOffset3
        ) {
            assert(subOffset0 + OutputCount <= delta.size());
            assert(subOffset1 + OutputCount <= delta.size());
            assert(subOffset2 + OutputCount <= delta.size());
            assert(subOffset3 + OutputCount <= delta.size());

            for (u32 i = 0; i < OutputCount; ++i) {
                accumulator[i] -=
                    delta[subOffset0 + i] + delta[subOffset1 + i] + delta[subOffset2 + i] + delta[subOffset3 + i];
            }
        }

        static inline void add(
            std::span<Type, OutputCount> accumulator,
            std::span<const Type, WeightCount> delta,
            u32 offset
        ) {
            assert(offset + OutputCount <= delta.size());

            for (u32 i = 0; i < OutputCount; ++i) {
                accumulator[i] += delta[offset + i];
            }
        }

        static inline void sub(
            std::span<Type, OutputCount> accumulator,
            std::span<const Type, WeightCount> delta,
            u32 offset
        ) {
            assert(offset + OutputCount <= delta.size());

            for (u32 i = 0; i < OutputCount; ++i) {
                accumulator[i] -= delta[offset + i];
            }
        }
    };

    template <typename Acc>
    struct RefreshTableEntry {
        Acc accumulator{};
        std::array<BitboardSet, 2> bbs{};

        [[nodiscard]] auto& colorBbs(Color c) {
            return bbs[static_cast<i32>(c)];
        }
    };

    template <typename Ft, u32 Size>
    struct RefreshTable {
        std::array<RefreshTableEntry<Accumulator<Ft>>, Size> table{};

        inline void init(const Ft& featureTransformer) {
            for (auto& entry : table) {
                entry.accumulator.initBoth(featureTransformer);
                entry.bbs.fill(BitboardSet{});
            }
        }
    };

    template <typename Type, u32 Outputs, typename FeatureSet = features::SingleBucket>
    struct FeatureTransformer {
        using WeightType = Type;
        using OutputType = Type;

        using InputFeatureSet = FeatureSet;

        using Accumulator = Accumulator<FeatureTransformer<Type, Outputs, FeatureSet>>;
        using RefreshTable = RefreshTable<FeatureTransformer<Type, Outputs, FeatureSet>, FeatureSet::RefreshTableSize>;

        static constexpr auto InputCount = InputFeatureSet::BucketCount * FeatureSet::InputSize;
        static constexpr auto OutputCount = Outputs;

        static constexpr auto WeightCount = InputCount * OutputCount;
        static constexpr auto BiasCount = OutputCount;

        static_assert(InputCount > 0);
        static_assert(OutputCount > 0);

        SP_SIMD_ALIGNAS std::array<WeightType, WeightCount> weights;
        SP_SIMD_ALIGNAS std::array<OutputType, BiasCount> biases;

        inline bool readFrom(IParamStream& stream) {
            return stream.read(weights) && stream.read(biases);
        }

        inline bool writeTo(IParamStream& stream) const {
            return stream.write(weights) && stream.write(biases);
        }
    };
} // namespace stormphrax::eval::nnue
