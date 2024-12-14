/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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
#include <span>
#include <istream>
#include <ostream>
#include <cassert>

#include "../activation.h"
#include "../output.h"
#include "../../../util/simd.h"
#include "../io.h"
#include "../../../util/multi_array.h"

namespace stormphrax::eval::nnue::arch
{
	// implements an (inputs->L1)x2->1xN network, with configurable activation
	template <u32 L1Size, u32 FtQ, u32 L1Q, typename Activation,
	    output::OutputBucketing OutputBucketing, i32 Scale>
	struct SingleLayer
	{
		using OutputType = i32;
		static constexpr u32 OutputCount = 1;

		static constexpr bool Pairwise = false;

	private:
		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		SP_SIMD_ALIGNAS std::array<i16, OutputBucketCount * L1Size * 2> l1Weights{};
		SP_SIMD_ALIGNAS std::array<i16, OutputBucketCount> l1Biases{};

	public:
		inline auto propagate(u32 bucket,
			std::span<const i16, L1Size>  stmInputs,
			std::span<const i16, L1Size> nstmInputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			using namespace util::simd;

			static constexpr i32 Q = FtQ * L1Q;

			assert(isAligned( stmInputs.data()));
			assert(isAligned(nstmInputs.data()));
			assert(isAligned(   outputs.data()));

			const auto weightOffset = bucket * L1Size * 2;
			const auto   biasOffset = bucket;

			auto sum = zero<i32>();

			// stm perspective
			for (u32 inputIdx = 0; inputIdx < L1Size; inputIdx += ChunkSize<i16>)
			{
				const auto  inputs = load<i16>(&stmInputs[inputIdx]);
				const auto weights = load<i16>(&l1Weights[weightOffset + inputIdx]);

				sum = Activation::template activateDotAccumulate<i16, FtQ>(sum, inputs, weights);
			}

			// nstm perspective
			for (u32 inputIdx = 0; inputIdx < L1Size; inputIdx += ChunkSize<i16>)
			{
				const auto  inputs = load<i16>(&nstmInputs[inputIdx]);
				const auto weights = load<i16>(&l1Weights[L1Size + weightOffset + inputIdx]);

				sum = Activation::template activateDotAccumulate<i16, FtQ>(sum, inputs, weights);
			}

			const auto output = hsum<i32>(sum);

			const auto bias = static_cast<i32>(l1Biases[biasOffset]);
			const auto out = bias + Activation::template output<i32, FtQ>(output);

			outputs[0] = out * Scale / Q;
		}

		inline auto readFrom(IParamStream &stream) -> bool
		{
			return stream.read(l1Weights)
				&& stream.read(l1Biases);
		}

		inline auto writeTo(IParamStream &stream) const -> bool
		{
			return stream.write(l1Weights)
				&& stream.write(l1Biases);
		}
	};
}
