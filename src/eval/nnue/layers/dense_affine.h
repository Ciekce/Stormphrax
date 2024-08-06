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

#include <array>
#include <span>
#include <istream>
#include <ostream>
#include <cassert>
#include <type_traits>

#include "../activation.h"
#include "../output.h"
#include "../../../util/simd.h"
#include "../io.h"

namespace stormphrax::eval::nnue::layers
{
	template <typename Input, typename Param, typename Output,
		u32 Inputs, u32 InputWeights, u32 Outputs, output::OutputBucketing OutputBucketing>
	struct BaseAffine
	{
		using  InputType = Input;
		using  ParamType = Param;
		using OutputType = Output;

		static constexpr auto  InputCount =  Inputs;
		static constexpr auto OutputCount = Outputs;

		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		static constexpr auto WeightCount = InputWeights * Outputs;
		static constexpr auto   BiasCount = OutputCount;

		static_assert( InputCount > 0);
		static_assert(OutputCount > 0);

		static_assert(sizeof(InputType) * InputCount >= util::simd::Alignment
			&& (sizeof(InputType) * InputCount) % util::simd::Alignment == 0);

		static_assert(sizeof(ParamType) * WeightCount >= util::simd::Alignment
			&& (sizeof(ParamType) * WeightCount) % util::simd::Alignment == 0);

		SP_SIMD_ALIGNAS std::array<ParamType, OutputBucketCount * WeightCount> weights;
		SP_SIMD_ALIGNAS std::array<ParamType, OutputBucketCount *   BiasCount> biases;

		inline auto readFrom(IParamStream &stream) -> bool
		{
			return stream.read(weights)
				&& stream.read(biases);
		}

		inline auto writeTo(IParamStream &stream) const -> bool
		{
			return stream.write(weights)
				&& stream.write(biases);
		}
	};

	template <typename Input, typename Param, activation::Activation Activation,
	    u32 Inputs, u32 Outputs, output::OutputBucketing OutputBucketing = output::Single>
	struct DenseAffine
		: BaseAffine<Input, Param, typename Activation::OutputType, Inputs, Inputs, Outputs, OutputBucketing>
	{
		using Base = BaseAffine<
		    Input, Param, typename Activation::OutputType, Inputs, Inputs, Outputs, OutputBucketing
		>;

		inline auto forward(const BitboardSet &bbs,
			std::span<const typename Base::InputType, Base::InputCount> inputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( inputs.data()));
			assert(isAligned(outputs.data()));

			const auto outputBucket = OutputBucketing::getBucket(bbs);

			const auto bucketWeightOffset = outputBucket * Base::WeightCount;
			const auto   bucketBiasOffset = outputBucket * Base::  BiasCount;

			for (u32 outputIdx = 0; outputIdx < Base::OutputCount; ++outputIdx)
			{
				const auto weightOffset = bucketWeightOffset + outputIdx * Base::InputCount;

				typename Base::OutputType sum{0};

				for (u32 inputIdx = 0; inputIdx < Base::InputCount; inputIdx += ChunkSize)
				{
					const auto inputVec = util::simd::load<typename Base::InputType>(&inputs[inputIdx]);
					const auto weightVec = util::simd::load<typename Base::ParamType>(
						&Base::weights[weightOffset + inputIdx]
					);

					const auto products = Activation::activateAndDot(inputVec, weightVec);
					sum = add<typename Base::OutputType>(sum, products);
				}

				const auto bias = static_cast<typename Base::OutputType>(Base::biases[bucketBiasOffset + outputIdx]);
				outputs[outputIdx] = bias + Activation::output(sum);
			}
		}
	};

	template <typename Input, typename Param, typename Activation,
		u32 Inputs, u32 Outputs, output::OutputBucketing OutputBucketing>
	struct DensePerspectivePlainAffine
		: BaseAffine<Input, Param, typename Activation::OutputType, Inputs * 2, Inputs * 2, Outputs, OutputBucketing>
	{
		using Base = BaseAffine<
		    Input, Param, typename Activation::OutputType, Inputs * 2, Inputs * 2, Outputs, OutputBucketing
		>;

		static_assert(activation::Activation<Activation>);

		static constexpr auto PerspectiveInputCount = Inputs;

		inline auto forward(const BitboardSet &bbs,
			std::span<const typename Base::InputType, PerspectiveInputCount>  stmInputs,
			std::span<const typename Base::InputType, PerspectiveInputCount> nstmInputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( stmInputs.data()));
			assert(isAligned(nstmInputs.data()));
			assert(isAligned(   outputs.data()));

			const auto outputBucket = OutputBucketing::getBucket(bbs);

			const auto bucketWeightOffset = outputBucket * Base::WeightCount;
			const auto   bucketBiasOffset = outputBucket * Base::  BiasCount;

			for (u32 outputIdx = 0; outputIdx < Base::OutputCount; ++outputIdx)
			{
				const auto weightOffset = bucketWeightOffset + outputIdx * PerspectiveInputCount;

				auto sum = zero<typename Base::OutputType>();

				// stm perspective
				for (u32 inputIdx = 0; inputIdx < PerspectiveInputCount; inputIdx += ChunkSize)
				{
					const auto inputVec = load<typename Base::InputType>(&stmInputs[inputIdx]);
					const auto weightVec = load<typename Base::ParamType>(
						&Base::weights[weightOffset + inputIdx]
					);

					sum = Activation::activateDotAccumulate(sum, inputVec, weightVec);
				}

				// nstm perspective
				for (u32 inputIdx = 0; inputIdx < PerspectiveInputCount; inputIdx += ChunkSize)
				{
					const auto inputVec = load<typename Base::InputType>(&nstmInputs[inputIdx]);
					const auto weightVec = load<typename Base::ParamType>(
						&Base::weights[PerspectiveInputCount + weightOffset + inputIdx]
					);

					sum = Activation::activateDotAccumulate(sum, inputVec, weightVec);
				}

				const auto output = hsum<typename Base::OutputType>(sum);

				const auto bias = static_cast<typename Base::OutputType>(Base::biases[bucketBiasOffset + outputIdx]);
				outputs[outputIdx] = bias + Activation::output(output);
			}
		}
	};

	template <typename Input, typename Param, typename Activation,
		u32 Inputs, u32 Outputs, typename Activation::OutputType Q, output::OutputBucketing OutputBucketing>
	struct DensePerspectivePairwiseMulAffine
		: BaseAffine<Input, Param, typename Activation::OutputType, Inputs * 2, Inputs, Outputs, OutputBucketing>
	{
		using Base = BaseAffine<
		    Input, Param, typename Activation::OutputType, Inputs * 2, Inputs, Outputs, OutputBucketing
		>;

		static_assert(activation::PairwiseActivation<Activation>);

		static constexpr auto PerspectiveInputCount = Inputs;

		static_assert(PerspectiveInputCount % 2 == 0);

		inline auto forward(const BitboardSet &bbs,
			std::span<const typename Base::InputType, PerspectiveInputCount>  stmInputs,
			std::span<const typename Base::InputType, PerspectiveInputCount> nstmInputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( stmInputs.data()));
			assert(isAligned(nstmInputs.data()));
			assert(isAligned(   outputs.data()));

			static constexpr auto PairCount = PerspectiveInputCount / 2;

			const auto outputBucket = OutputBucketing::getBucket(bbs);

			const auto bucketWeightOffset = outputBucket * Base::WeightCount;
			const auto   bucketBiasOffset = outputBucket * Base::  BiasCount;

			for (u32 outputIdx = 0; outputIdx < Base::OutputCount; ++outputIdx)
			{
				const auto weightOffset = bucketWeightOffset + outputIdx * PairCount;

				auto sum = zero<typename Base::OutputType>();

				// stm perspective
				for (u32 inputIdx = 0; inputIdx < PairCount; inputIdx += ChunkSize)
				{
					const auto input1Vec = load<typename Base::InputType>(&stmInputs[inputIdx]);
					const auto input2Vec = load<typename Base::InputType>(&stmInputs[inputIdx + PairCount]);

					const auto weightVec = load<typename Base::ParamType>(
						&Base::weights[weightOffset + inputIdx]
					);

					sum = Activation::activateDotAccumulate(sum, input1Vec, input2Vec, weightVec);
				}

				// nstm perspective
				for (u32 inputIdx = 0; inputIdx < PairCount; inputIdx += ChunkSize)
				{
					const auto input1Vec = load<typename Base::InputType>(&nstmInputs[inputIdx]);
					const auto input2Vec = load<typename Base::InputType>(&nstmInputs[inputIdx + PairCount]);

					const auto weightVec = load<typename Base::ParamType>(
						&Base::weights[PairCount + weightOffset + inputIdx]
					);

					sum = Activation::activateDotAccumulate(sum, input1Vec, input2Vec, weightVec);
				}

				const auto output = hsum<typename Base::OutputType>(sum) / Q;

				const auto bias = static_cast<typename Base::OutputType>(Base::biases[bucketBiasOffset + outputIdx]);
				outputs[outputIdx] = bias + Activation::output(output);
			}
		}
	};

	template <bool PairwiseMul, typename Input, typename Param, typename Activation, u32 Inputs, u32 Outputs,
		typename Activation::OutputType Q, output::OutputBucketing OutputBucketing = output::Single>
	using DensePerspectiveAffine = std::conditional_t<PairwiseMul,
		DensePerspectivePairwiseMulAffine<Input, Param, Activation, Inputs, Outputs, Q, OutputBucketing>,
		DensePerspectivePlainAffine      <Input, Param, Activation, Inputs, Outputs,    OutputBucketing>
	>;

	template <u32 L1Size, u32 L2Size, u32 L3Size,
		u32 L1Q, u32 L2Q, u32 Scale, output::OutputBucketing OutputBucketing>
	struct MakeItWork
	{
	private:
		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		SP_SIMD_ALIGNAS std::array<i16, OutputBucketCount * L1Size * L2Size> l1Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *          L2Size> l1Biases{};

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * L2Size * L3Size> l2Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *          L3Size> l2Biases{};

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * L3Size> l3Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount>          l3Biases{};

	public:
		using  InputType = i16;
		using OutputType = i32;

		static constexpr u32 PerspectiveInputCount = L1Size;
		static constexpr u32 OutputCount = 1;

		inline auto forward(const BitboardSet &bbs,
			std::span<const InputType, PerspectiveInputCount>  stmInputs,
			std::span<const InputType, PerspectiveInputCount> nstmInputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			static constexpr auto Qf = static_cast<f32>(L1Q * L1Q * L2Q);
			static constexpr auto Scalef = static_cast<f32>(Scale);

			static constexpr auto L1PairCount = PerspectiveInputCount / 2;

			const auto outputBucket = OutputBucketing::getBucket(bbs);

			const auto l1WeightOffset = outputBucket * L1Size * L2Size;
			const auto l1BiasOffset   = outputBucket * L2Size;

			const auto l2WeightOffset = outputBucket * L2Size * L3Size;
			const auto l2BiasOffset   = outputBucket * L3Size;

			const auto l3WeightOffset = outputBucket * L3Size;
			const auto l3BiasOffset   = outputBucket;

			util::AlignedArray<util::simd::Alignment, f32, L2Size> l1Out{};
			std::memcpy(l1Out.data(), &l1Biases[l1BiasOffset], sizeof(l1Out));

			// stm perspective
			for (u32 inputIdx = 0; inputIdx < L1PairCount; ++inputIdx)
			{
				const auto weightsStart = l1WeightOffset + inputIdx * L2Size;

				auto i1 = stmInputs[inputIdx              ];
				auto i2 = stmInputs[inputIdx + L1PairCount];

				i1 = std::clamp<InputType>(i1, 0, L1Q);
				i2 = std::clamp<InputType>(i2, 0, L1Q);

				const auto i = static_cast<f32>(i1 * i2) / Qf;

				for (u32 outputIdx = 0; outputIdx < L2Size; ++outputIdx)
				{
					l1Out[outputIdx] += i * l1Weights[weightsStart + outputIdx];
				}
			}

			// nstm perspective
			for (u32 inputIdx = 0; inputIdx < L1PairCount; ++inputIdx)
			{
				const auto weightsStart = l1WeightOffset + (L1PairCount + inputIdx) * L2Size;

				auto i1 = nstmInputs[inputIdx              ];
				auto i2 = nstmInputs[inputIdx + L1PairCount];

				i1 = std::clamp<InputType>(i1, 0, L1Q);
				i2 = std::clamp<InputType>(i2, 0, L1Q);

				const auto i = static_cast<f32>(i1 * i2) / Qf;

				for (u32 outputIdx = 0; outputIdx < L2Size; ++outputIdx)
				{
					l1Out[outputIdx] += i * l1Weights[weightsStart + outputIdx];
				}
			}

			util::AlignedArray<util::simd::Alignment, f32, L3Size> l2Out{};
			std::memcpy(l2Out.data(), &l2Biases[l2BiasOffset], sizeof(l2Out));

			for (u32 inputIdx = 0; inputIdx < L2Size; ++inputIdx)
			{
				const auto weightsStart = l2WeightOffset + inputIdx * L3Size;

				auto i = static_cast<f32>(l1Out[inputIdx]);
				i = std::clamp(i, 0.0F, 1.0F);
				i *= i;

				for (u32 outputIdx = 0; outputIdx < L3Size; ++outputIdx)
				{
					l2Out[outputIdx] += i * l2Weights[weightsStart + outputIdx];
				}
			}

			auto l3Out = l3Biases[l3BiasOffset];

			for (u32 inputIdx = 0; inputIdx < L3Size; ++inputIdx)
			{
				const auto weightIdx = l3WeightOffset + inputIdx;

				auto i = static_cast<f32>(l2Out[inputIdx]);
				i = std::clamp(i, 0.0F, 1.0F);
				i *= i;

				l3Out += i * l3Weights[weightIdx];
			}

			outputs[0] = static_cast<i32>(l3Out * Scalef);
		}

		inline auto readFrom(IParamStream &stream) -> bool
		{
			return stream.read(l1Weights)
				&& stream.read(l1Biases)
				&& stream.read(l2Weights)
				&& stream.read(l2Biases)
				&& stream.read(l3Weights)
				&& stream.read(l3Biases);
		}

		inline auto writeTo(IParamStream &stream) const -> bool
		{
			return stream.write(l1Weights)
				&& stream.write(l1Biases)
				&& stream.write(l2Weights)
				&& stream.write(l2Biases)
				&& stream.write(l3Weights)
				&& stream.write(l3Biases);
		}
	};
}
