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

#include <array>
#include <cassert>
#include <istream>
#include <ostream>
#include <span>

#include "../../../util/multi_array.h"
#include "../../../util/simd.h"
#include "../activation.h"
#include "../io.h"
#include "../output.h"

namespace stormphrax::eval::nnue::arch {
    // implements an (inputs->L1)x2->1xN network, with configurable activation
    template <u32 kL1Size, u32 kFtQ, u32 kL1Q, typename Activation, output::OutputBucketing OutputBucketing, i32 kScale>
    struct SingleLayer {
        static constexpr u32 kArchId = 1;

        using OutputType = i32;
        static constexpr u32 kOutputCount = 1;

        static constexpr bool kPairwise = false;
        static constexpr bool kRequiresFtPermute = false;

    private:
        static constexpr auto kOutputBucketCount = OutputBucketing::kBucketCount;

        SP_SIMD_ALIGNAS std::array<i16, kOutputBucketCount * kL1Size * 2> l1Weights{};
        SP_SIMD_ALIGNAS std::array<i16, kOutputBucketCount> l1Biases{};

    public:
        inline void propagate(
            u32 bucket,
            std::span<const i16, kL1Size> stmInputs,
            std::span<const i16, kL1Size> nstmInputs,
            std::span<OutputType, kOutputCount> outputs
        ) const {
            using namespace util::simd;

            static constexpr i32 Q = kFtQ * kL1Q;

            assert(isAligned(stmInputs.data()));
            assert(isAligned(nstmInputs.data()));
            assert(isAligned(outputs.data()));

            const auto weightOffset = bucket * kL1Size * 2;
            const auto biasOffset = bucket;

            auto sum = zero<i32>();

            // stm perspective
            for (u32 inputIdx = 0; inputIdx < kL1Size; inputIdx += kChunkSize<i16>) {
                const auto inputs = load<i16>(&stmInputs[inputIdx]);
                const auto weights = load<i16>(&l1Weights[weightOffset + inputIdx]);

                sum = Activation::template activateDotAccumulate<i16, kFtQ>(sum, inputs, weights);
            }

            // nstm perspective
            for (u32 inputIdx = 0; inputIdx < kL1Size; inputIdx += kChunkSize<i16>) {
                const auto inputs = load<i16>(&nstmInputs[inputIdx]);
                const auto weights = load<i16>(&l1Weights[kL1Size + weightOffset + inputIdx]);

                sum = Activation::template activateDotAccumulate<i16, kFtQ>(sum, inputs, weights);
            }

            const auto output = hsum<i32>(sum);

            const auto bias = static_cast<i32>(l1Biases[biasOffset]);
            const auto out = bias + Activation::template output<i32, kFtQ>(output);

            outputs[0] = out * kScale / Q;
        }

        inline bool readFrom(IParamStream& stream) {
            return stream.read(l1Weights) && stream.read(l1Biases);
        }

        inline bool writeTo(IParamStream& stream) const {
            return stream.write(l1Weights) && stream.write(l1Biases);
        }
    };
} // namespace stormphrax::eval::nnue::arch
