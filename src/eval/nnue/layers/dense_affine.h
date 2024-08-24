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

		inline auto forward(u32 bucket,
			std::span<const typename Base::InputType, Base::InputCount> inputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( inputs.data()));
			assert(isAligned(outputs.data()));

			static constexpr auto ChunkSize = util::simd::ChunkSize<typename Base::InputType>;

			const auto bucketWeightOffset = bucket * Base::WeightCount;
			const auto   bucketBiasOffset = bucket * Base::  BiasCount;

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

		inline auto forward(u32 bucket,
			std::span<const typename Base::InputType, PerspectiveInputCount>  stmInputs,
			std::span<const typename Base::InputType, PerspectiveInputCount> nstmInputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( stmInputs.data()));
			assert(isAligned(nstmInputs.data()));
			assert(isAligned(   outputs.data()));

			static constexpr auto ChunkSize = util::simd::ChunkSize<typename Base::InputType>;

			const auto bucketWeightOffset = bucket * Base::WeightCount;
			const auto   bucketBiasOffset = bucket * Base::  BiasCount;

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

		inline auto forward(u32 bucket,
			std::span<const typename Base::InputType, PerspectiveInputCount>  stmInputs,
			std::span<const typename Base::InputType, PerspectiveInputCount> nstmInputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( stmInputs.data()));
			assert(isAligned(nstmInputs.data()));
			assert(isAligned(   outputs.data()));

			static constexpr auto ChunkSize = util::simd::ChunkSize<typename Base::InputType>;
			static constexpr auto PairCount = PerspectiveInputCount / 2;

			const auto bucketWeightOffset = bucket * Base::WeightCount;
			const auto   bucketBiasOffset = bucket * Base::  BiasCount;

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

	template <u32 L1Size>
	struct FtOutClippedReLU
	{
		using  InputType = i16;
		using OutputType = u8;

		static constexpr u32 PerspectiveInputCount = L1Size;
		static constexpr u32 OutputCount = L1Size;

		inline auto forward(u32 bucket,
			std::span<const InputType, PerspectiveInputCount>  stmInputs,
			std::span<const InputType, PerspectiveInputCount> nstmInputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( stmInputs.data()));
			assert(isAligned(nstmInputs.data()));
			assert(isAligned(   outputs.data()));

			static constexpr auto PairCount = PerspectiveInputCount / 2;

			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);

			static constexpr auto Shift = 6;

			const auto Zero = zero<i16>();
			const auto One = set1<i16>(L1Q);

			const auto activatePerspective = [&](
				std::span<const InputType, PerspectiveInputCount> inputs, u32 outputOffset)
			{
				for (u32 inputIdx = 0; inputIdx < PairCount; inputIdx += ChunkSize<i16> * 4)
				{
					auto i1_0 = load<i16>(&inputs[inputIdx + ChunkSize<i16> * 0]);
					auto i1_1 = load<i16>(&inputs[inputIdx + ChunkSize<i16> * 1]);
					auto i1_2 = load<i16>(&inputs[inputIdx + ChunkSize<i16> * 2]);
					auto i1_3 = load<i16>(&inputs[inputIdx + ChunkSize<i16> * 3]);

					auto i2_0 = load<i16>(&inputs[inputIdx + PairCount + ChunkSize<i16> * 0]);
					auto i2_1 = load<i16>(&inputs[inputIdx + PairCount + ChunkSize<i16> * 1]);
					auto i2_2 = load<i16>(&inputs[inputIdx + PairCount + ChunkSize<i16> * 2]);
					auto i2_3 = load<i16>(&inputs[inputIdx + PairCount + ChunkSize<i16> * 3]);

					i1_0 = min<i16>(i1_0, One);
					i1_1 = min<i16>(i1_1, One);
					i1_2 = min<i16>(i1_2, One);
					i1_3 = min<i16>(i1_3, One);

					i2_0 = min<i16>(i2_0, One);
					i2_1 = min<i16>(i2_1, One);
					i2_2 = min<i16>(i2_2, One);
					i2_3 = min<i16>(i2_3, One);

					i1_0 = max<i16>(i1_0, Zero);
					i1_1 = max<i16>(i1_1, Zero);
					i1_2 = max<i16>(i1_2, Zero);
					i1_3 = max<i16>(i1_3, Zero);

					i1_0 = shiftLeft<i16>(i1_0, Shift);
					i1_1 = shiftLeft<i16>(i1_1, Shift);
					i1_2 = shiftLeft<i16>(i1_2, Shift);
					i1_3 = shiftLeft<i16>(i1_3, Shift);

					const auto p_0 = mulHi<i16>(i1_0, i2_0);
					const auto p_1 = mulHi<i16>(i1_1, i2_1);
					const auto p_2 = mulHi<i16>(i1_2, i2_2);
					const auto p_3 = mulHi<i16>(i1_3, i2_3);

					auto packed_0 = packUnsigned<i16>(p_0, p_1);
					auto packed_1 = packUnsigned<i16>(p_2, p_3);

#if SP_HAS_AVX512
					packed_0 = _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), packed_0);
					packed_1 = _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), packed_1);
#elif SP_HAS_AVX2
					packed_0 = _mm256_permute4x64_epi64(packed_0, _MM_SHUFFLE(3, 1, 2, 0));
					packed_1 = _mm256_permute4x64_epi64(packed_1, _MM_SHUFFLE(3, 1, 2, 0));
#endif

					store<u8>(&outputs[outputOffset + inputIdx + ChunkSize<i8> * 0], packed_0);
					store<u8>(&outputs[outputOffset + inputIdx + ChunkSize<i8> * 1], packed_1);
				}
			};

			// stm perspective
			activatePerspective( stmInputs, 0);
			activatePerspective(nstmInputs, PairCount);
		}

		inline auto readFrom([[maybe_unused]] IParamStream &stream) -> bool
		{
			return true;
		}

		inline auto writeTo([[maybe_unused]] IParamStream &stream) const -> bool
		{
			return true;
		}
	};

	template <u32 L1Size, u32 L2Size, u32 L1Q, u32 L2Q, output::OutputBucketing OutputBucketing>
	struct DenseAffineL1SqrReLU
	{
	private:
		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		SP_SIMD_ALIGNAS std::array<i8,  OutputBucketCount * L1Size * L2Size> weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *          L2Size> biases{};

	public:
		using  InputType = u8;
		using OutputType = f32;

		static constexpr u32 InputCount = L1Size;
		static constexpr u32 OutputCount = L2Size;

		inline auto forward(u32 bucket,
			std::span<const InputType, InputCount> inputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( inputs.data()));
			assert(isAligned(outputs.data()));

			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);

			static constexpr auto FtShift = 6;

			const auto Rqf = static_cast<f32>(1 << (16 - FtShift)) / static_cast<f32>(L1Q * L1Q * L2Q);
			const auto Rq = set1<f32>(Rqf);

			const auto Zero = zero<f32>();

			const auto weightOffset = bucket * L2Size * L1Size;
			const auto biasOffset   = bucket * L2Size;

			// SAFETY: u8 (unsigned char) can safely be aliased to any type
			const auto *ftOutI32s = reinterpret_cast<const i32 *>(inputs.data());

			SP_SIMD_ALIGNAS util::MultiArray<Vector<i32>, L2Size / ChunkSize<i32>, 4> l1Intermediate{};

			for (u32 inputIdx = 0; inputIdx < L1Size; inputIdx += I8ChunkSizeI32 * 4)
			{
				const auto weightsStart = weightOffset + inputIdx * L2Size;

				const auto i_0 = set1<i32>(ftOutI32s[inputIdx / I8ChunkSizeI32 + 0]);
				const auto i_1 = set1<i32>(ftOutI32s[inputIdx / I8ChunkSizeI32 + 1]);
				const auto i_2 = set1<i32>(ftOutI32s[inputIdx / I8ChunkSizeI32 + 2]);
				const auto i_3 = set1<i32>(ftOutI32s[inputIdx / I8ChunkSizeI32 + 3]);

				for (u32 outputIdx = 0; outputIdx < L2Size; outputIdx += ChunkSize<i32>)
				{
					auto &intermediate = l1Intermediate[outputIdx / ChunkSize<i32>];

					const auto w_0 = load<i8>(&weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 0)]);
					const auto w_1 = load<i8>(&weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 1)]);
					const auto w_2 = load<i8>(&weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 2)]);
					const auto w_3 = load<i8>(&weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 3)]);

					intermediate[0] = dpbusd<i32>(intermediate[0], i_0, w_0);
					intermediate[1] = dpbusd<i32>(intermediate[1], i_1, w_1);
					intermediate[2] = dpbusd<i32>(intermediate[2], i_2, w_2);
					intermediate[3] = dpbusd<i32>(intermediate[3], i_3, w_3);
				}
			}

			for (u32 idx = 0; idx < L2Size; idx += ChunkSize<i32>)
			{
				const auto &intermediate = l1Intermediate[idx / ChunkSize<i32>];

				const auto halfSums_0 = add<i32>(intermediate[0], intermediate[1]);
				const auto halfSums_1 = add<i32>(intermediate[2], intermediate[3]);

				const auto sums = add<i32>(halfSums_0, halfSums_1);

				const auto b = load<f32>(&biases[biasOffset + idx]);

				auto out = cast<i32, f32>(sums);

				out = fma<f32>(out, Rq, b);

				out = max<f32>(out, Zero);
				out = mul<f32>(out, out);

				store<f32>(&outputs[idx], out);
			}
		}

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

	template <u32 InputSize, u32 OutputSize, output::OutputBucketing OutputBucketing>
	struct DenseAffineNoActivation
	{
	private:
		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * InputSize * OutputSize> weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *             OutputSize> biases{};

	public:
		using  InputType = f32;
		using OutputType = f32;

		static constexpr u32 InputCount = InputSize;
		static constexpr u32 OutputCount = OutputSize;

		inline auto forward(u32 bucket,
			std::span<const InputType, InputCount> inputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( inputs.data()));
			assert(isAligned(outputs.data()));

			const auto weightOffset = bucket * OutputSize * InputSize;
			const auto biasOffset   = bucket * OutputSize;

			std::memcpy(outputs.data(), &biases[biasOffset], outputs.size_bytes());

			// avx512
			if constexpr (ChunkSize<f32> * 4 > OutputSize)
			{
				for (u32 inputIdx = 0; inputIdx < InputSize; ++inputIdx)
				{
					const auto weightsStart = weightOffset + inputIdx * OutputSize;

					const auto i = set1<f32>(inputs[inputIdx]);

					for (u32 outputIdx = 0; outputIdx < OutputSize; outputIdx += ChunkSize<f32> * 2)
					{
						const auto w_0 = load<f32>(&weights[weightsStart + outputIdx + ChunkSize<f32> * 0]);
						const auto w_1 = load<f32>(&weights[weightsStart + outputIdx + ChunkSize<f32> * 1]);

						auto out_0 = load<f32>(&outputs[outputIdx + ChunkSize<f32> * 0]);
						auto out_1 = load<f32>(&outputs[outputIdx + ChunkSize<f32> * 1]);

						out_0 = fma<f32>(i, w_0, out_0);
						out_1 = fma<f32>(i, w_1, out_1);

						store<f32>(&outputs[outputIdx + ChunkSize<f32> * 0], out_0);
						store<f32>(&outputs[outputIdx + ChunkSize<f32> * 1], out_1);
					}
				}
			}
			else
			{
				for (u32 inputIdx = 0; inputIdx < InputSize; ++inputIdx)
				{
					const auto weightsStart = weightOffset + inputIdx * OutputSize;

					const auto i = set1<f32>(inputs[inputIdx]);

					for (u32 outputIdx = 0; outputIdx < OutputSize; outputIdx += ChunkSize<f32> * 4)
					{
						const auto w_0 = load<f32>(&weights[weightsStart + outputIdx + ChunkSize<f32> * 0]);
						const auto w_1 = load<f32>(&weights[weightsStart + outputIdx + ChunkSize<f32> * 1]);
						const auto w_2 = load<f32>(&weights[weightsStart + outputIdx + ChunkSize<f32> * 2]);
						const auto w_3 = load<f32>(&weights[weightsStart + outputIdx + ChunkSize<f32> * 3]);

						auto out_0 = load<f32>(&outputs[outputIdx + ChunkSize<f32> * 0]);
						auto out_1 = load<f32>(&outputs[outputIdx + ChunkSize<f32> * 1]);
						auto out_2 = load<f32>(&outputs[outputIdx + ChunkSize<f32> * 2]);
						auto out_3 = load<f32>(&outputs[outputIdx + ChunkSize<f32> * 3]);

						out_0 = fma<f32>(i, w_0, out_0);
						out_1 = fma<f32>(i, w_1, out_1);
						out_2 = fma<f32>(i, w_2, out_2);
						out_3 = fma<f32>(i, w_3, out_3);

						store<f32>(&outputs[outputIdx + ChunkSize<f32> * 0], out_0);
						store<f32>(&outputs[outputIdx + ChunkSize<f32> * 1], out_1);
						store<f32>(&outputs[outputIdx + ChunkSize<f32> * 2], out_2);
						store<f32>(&outputs[outputIdx + ChunkSize<f32> * 3], out_3);
					}
				}
			}
		}

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

	template <u32 L3Size, u32 Scale, output::OutputBucketing OutputBucketing>
	struct SqrReLUDenseAffine
	{
	private:
		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * L3Size> weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount>          biases{};

	public:
		using  InputType = f32;
		using OutputType = i32;

		static constexpr u32 InputCount = L3Size;
		static constexpr u32 OutputCount = 1;

		inline auto forward(u32 bucket,
			std::span<const InputType, InputCount> inputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( inputs.data()));
			assert(isAligned(outputs.data()));

			static constexpr auto Scalef = static_cast<f32>(Scale);

			const auto Zero = zero<f32>();

			const auto l3WeightOffset = bucket * L3Size;
			const auto l3BiasOffset   = bucket;

			Vector<f32> s;

			// avx512
			if constexpr (ChunkSize<f32> * 4 > L3Size)
			{
				auto l3Out_0 = zero<f32>();
				auto l3Out_1 = zero<f32>();

				for (u32 inputIdx = 0; inputIdx < L3Size; inputIdx += ChunkSize<f32> * 2)
				{
					const auto weightIdx = l3WeightOffset + inputIdx;

					auto i_0 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 0]);
					auto i_1 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 1]);

					const auto w_0 = load<f32>(&weights[weightIdx + ChunkSize<f32> * 0]);
					const auto w_1 = load<f32>(&weights[weightIdx + ChunkSize<f32> * 1]);

					i_0 = max<f32>(i_0, Zero);
					i_1 = max<f32>(i_1, Zero);

					i_0 = mul<f32>(i_0, i_0);
					i_1 = mul<f32>(i_1, i_1);

					l3Out_0 = fma<f32>(i_0, w_0, l3Out_0);
					l3Out_1 = fma<f32>(i_1, w_1, l3Out_1);
				}

				s = add<f32>(l3Out_0, l3Out_1);
			}
			else
			{
				auto l3Out_0 = zero<f32>();
				auto l3Out_1 = zero<f32>();
				auto l3Out_2 = zero<f32>();
				auto l3Out_3 = zero<f32>();

				for (u32 inputIdx = 0; inputIdx < L3Size; inputIdx += ChunkSize<f32> * 4)
				{
					const auto weightIdx = l3WeightOffset + inputIdx;

					auto i_0 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 0]);
					auto i_1 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 1]);
					auto i_2 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 2]);
					auto i_3 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 3]);

					const auto w_0 = load<f32>(&weights[weightIdx + ChunkSize<f32> * 0]);
					const auto w_1 = load<f32>(&weights[weightIdx + ChunkSize<f32> * 1]);
					const auto w_2 = load<f32>(&weights[weightIdx + ChunkSize<f32> * 2]);
					const auto w_3 = load<f32>(&weights[weightIdx + ChunkSize<f32> * 3]);

					i_0 = max<f32>(i_0, Zero);
					i_1 = max<f32>(i_1, Zero);
					i_2 = max<f32>(i_2, Zero);
					i_3 = max<f32>(i_3, Zero);

					i_0 = mul<f32>(i_0, i_0);
					i_1 = mul<f32>(i_1, i_1);
					i_2 = mul<f32>(i_2, i_2);
					i_3 = mul<f32>(i_3, i_3);

					l3Out_0 = fma<f32>(i_0, w_0, l3Out_0);
					l3Out_1 = fma<f32>(i_1, w_1, l3Out_1);
					l3Out_2 = fma<f32>(i_2, w_2, l3Out_2);
					l3Out_3 = fma<f32>(i_3, w_3, l3Out_3);
				}

				const auto s0 = add<f32>(l3Out_0, l3Out_1);
				const auto s1 = add<f32>(l3Out_2, l3Out_3);

				s = add<f32>(s0, s1);
			}

			const auto out = biases[l3BiasOffset] + hsum<f32>(s);
			outputs[0] = static_cast<i32>(out * Scalef);
		}

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
}
