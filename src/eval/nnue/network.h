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

        [[nodiscard]] inline auto featureTransformer() const -> const auto& {
            return m_featureTransformer;
        }

        inline auto propagate(
            const BitboardSet& bbs,
            std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::OutputCount> stmInputs,
            std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::OutputCount> nstmInputs
        ) const {
            util::simd::Array<typename Arch::OutputType, Arch::OutputCount> outputs;

            const auto bucket = OutputBucketing::getBucket(bbs);
            m_arch.propagate(bucket, stmInputs, nstmInputs, outputs);

            return outputs;
        }

        inline auto readFrom(IParamStream& stream) -> bool {
            if (!m_featureTransformer.readFrom(stream) || !m_arch.readFrom(stream))
                return false;

            if constexpr (Arch::RequiresFtPermute)
                Arch::template permuteFt<typename Ft::WeightType, typename Ft::OutputType>(
                    m_featureTransformer.weights,
                    m_featureTransformer.biases
                );

            return true;
        }

        inline auto writeTo(IParamStream& stream) const -> bool {
            return m_featureTransformer.writeTo(stream) && m_arch.writeTo(stream);
        }

    private:
        FeatureTransformer m_featureTransformer{};
        Arch m_arch{};
    };
} // namespace stormphrax::eval::nnue
