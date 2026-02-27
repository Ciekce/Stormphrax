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

#include <algorithm>
#include <array>
#include <istream>
#include <ostream>
#include <span>

#include "../../core.h"
#include "../../position/boards.h"
#include "../../util/multi_array.h"
#include "../../util/simd.h"
#include "features/psq.h"
#include "features/threats.h"
#include "loader.h"

namespace stormphrax::eval::nnue {
    template <typename Ft>
    class Accumulator {
    private:
        using Type = typename Ft::OutputType;

        static constexpr auto kInputCount = Ft::kPsqInputCount;
        static constexpr auto kWeightCount = Ft::kPsqWeightCount;
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

        inline void clear(Color c) {
            m_outputs[c.idx()].fill(0);
        }

        inline void initBoth(const Ft& featureTransformer) {
            std::ranges::copy(featureTransformer.biases, m_outputs[0].begin());
            std::ranges::copy(featureTransformer.biases, m_outputs[1].begin());
        }

        inline void subAddFrom(const Accumulator& src, const Ft& featureTransformer, Color c, u32 sub, u32 add) {
            assert(sub < kInputCount);
            assert(add < kInputCount);

            subAdd(src.forColor(c), forColor(c), featureTransformer.psqWeights, sub * kOutputCount, add * kOutputCount);
        }

        inline void subSubAddFrom(
            const Accumulator& src,
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
                featureTransformer.psqWeights,
                sub0 * kOutputCount,
                sub1 * kOutputCount,
                add * kOutputCount
            );
        }

        inline void subSubAddAddFrom(
            const Accumulator& src,
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
                featureTransformer.psqWeights,
                sub0 * kOutputCount,
                sub1 * kOutputCount,
                add0 * kOutputCount,
                add1 * kOutputCount
            );
        }

        inline void activateFeature(const Ft& featureTransformer, Color c, u32 feature) {
            assert(feature < kInputCount);
            add(forColor(c), featureTransformer.psqWeights, feature * kOutputCount);
        }

        inline void deactivateFeature(const Ft& featureTransformer, Color c, u32 feature) {
            assert(feature < kInputCount);
            sub(forColor(c), featureTransformer.psqWeights, feature * kOutputCount);
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
                featureTransformer.psqWeights,
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
                featureTransformer.psqWeights,
                feature0 * kOutputCount,
                feature1 * kOutputCount,
                feature2 * kOutputCount,
                feature3 * kOutputCount
            );
        }

        inline void copyFrom(Color c, const Accumulator& other) {
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

    template <typename Type, typename ThreatType, u32 kOutputs, typename FeatureSet>
    struct FeatureTransformer {
        using PsqWeightType = Type;
        using ThreatWeightType = ThreatType;
        using OutputType = Type;

        using InputFeatureSet = FeatureSet;

        using Accumulator = Accumulator<FeatureTransformer>;
        using RefreshTable = RefreshTable<FeatureTransformer, FeatureSet::kRefreshTableSize>;

        static constexpr auto kPsqInputCount = InputFeatureSet::kBucketCount * FeatureSet::kInputSize;
        static constexpr auto kOutputCount = kOutputs;

        static constexpr auto kPsqWeightCount = kPsqInputCount * kOutputCount;
        static constexpr auto kThreatWeightCount = FeatureSet::kThreatFeatures * kOutputCount;
        static constexpr auto kBiasCount = kOutputCount;

        static_assert(kPsqInputCount > 0);
        static_assert(kOutputCount > 0);

        SP_NETWORK_PARAMS(PsqWeightType, kPsqWeightCount, psqWeights);
        SP_NETWORK_PARAMS(ThreatWeightType, kThreatWeightCount, threatWeights);
        SP_NETWORK_PARAMS(OutputType, kBiasCount, biases);

        inline bool loadFrom(NetworkLoader& loader) {
            return loader.load(psqWeights) && loader.load(threatWeights) && loader.load(biases);
        }

        [[nodiscard]] static inline usize byteSize() {
            return sizeof(PsqWeightType) * kPsqWeightCount       //
                 + sizeof(ThreatWeightType) * kThreatWeightCount //
                 + sizeof(OutputType) * kBiasCount;
        }
    };
} // namespace stormphrax::eval::nnue
