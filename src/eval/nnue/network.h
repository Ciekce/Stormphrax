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

#include <span>

#include "../../position/boards.h"
#include "../../util/aligned_array.h"

namespace stormphrax::eval::nnue {
    template <typename Ft, typename OutputBucketing, typename Arch>
    class PerspectiveNetwork {
    public:
        using FeatureTransformer = Ft;

        [[nodiscard]] inline const FeatureTransformer& featureTransformer() const {
            return m_featureTransformer;
        }

        inline util::simd::Array<typename Arch::OutputType, Arch::kOutputCount> propagate(
            const BitboardSet& bbs,
            std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::kOutputCount> stmPsqInputs,
            std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::kOutputCount> nstmPsqInputs,
            std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::kOutputCount> stmThreatInputs,
            std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::kOutputCount> nstmThreatInputs
        ) const {
            util::simd::Array<typename Arch::OutputType, Arch::kOutputCount> outputs;

            const auto bucket = OutputBucketing::getBucket(bbs);
            m_arch.propagate(bucket, stmPsqInputs, nstmPsqInputs, stmThreatInputs, nstmThreatInputs, outputs);

            return outputs;
        }

        inline bool readFrom(IParamStream& stream) {
            if (!m_featureTransformer.readFrom(stream) || !m_arch.readFrom(stream)) {
                return false;
            }

            if constexpr (Arch::kRequiresFtPermute) {
                Arch::template permuteFt<
                    typename Ft::PsqWeightType,
                    typename Ft::ThreatWeightType,
                    typename Ft::OutputType>(
                    m_featureTransformer.psqWeights,
                    m_featureTransformer.threatWeights,
                    m_featureTransformer.biases
                );
            }

            return true;
        }

        inline bool writeTo(IParamStream& stream) const {
            return m_featureTransformer.writeTo(stream) && m_arch.writeTo(stream);
        }

    private:
        FeatureTransformer m_featureTransformer{};
        Arch m_arch{};
    };
} // namespace stormphrax::eval::nnue
