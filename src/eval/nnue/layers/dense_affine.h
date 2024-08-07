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

		SP_SIMD_ALIGNAS std::array<i8, OutputBucketCount * L1Size * L2Size> l1Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *          L2Size> l1Biases{};

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * L2Size * L3Size> l2Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *          L3Size> l2Biases{};

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * L3Size> l3Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount>          l3Biases{};

		template <typename T, usize N>
		using SimdArray = util::AlignedArray<util::simd::Alignment, T, N>;

		using vepi8 = __m256i;
		using vepi16 = __m256i;
		using vepi32 = __m256i;
		using vps32 = __m256;

	public:
		using  InputType = i16;
		using OutputType = i32;

		static constexpr u32 PerspectiveInputCount = L1Size;
		static constexpr u32 OutputCount = 1;

		// this can produce slightly different results to the equivalent
		// autovectorised code, because of FP order of operations differences.
		// in practice this does change bench
		inline auto forward(const BitboardSet &bbs,
			std::span<const InputType, PerspectiveInputCount>  stmInputs,
			std::span<const InputType, PerspectiveInputCount> nstmInputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			static constexpr auto Scalef = static_cast<f32>(Scale);

			static constexpr auto L1PairCount = PerspectiveInputCount / 2;

			static constexpr auto U8ChunkSize = sizeof(__m256i) / sizeof(u8);
			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);
			static constexpr auto I16ChunkSize = sizeof(__m256i) / sizeof(i16);
			static constexpr auto I32ChunkSize = sizeof(__m256i) / sizeof(i32);
			static constexpr auto F32ChunkSize = sizeof(__m256) / sizeof(f32);

			static constexpr auto FtShift = 10;

			const auto loadI = [](const void *ptr) -> __m256i
			{
				return _mm256_load_si256(reinterpret_cast<const __m256i *>(ptr));
			};

			const auto storeI = [](void *ptr, __m256i v)
			{
				_mm256_store_si256(reinterpret_cast<__m256i *>(ptr), v);
			};

			// emulates the VNNI instruction
			const auto dpbusd_epi32 = [](__m256i sum, __m256i u, __m256i s)
			{
				const auto p = _mm256_maddubs_epi16(u, s);
				const auto w = _mm256_madd_epi16(p, _mm256_set1_epi16(1));
				return _mm256_add_epi32(sum, w);
			};

			const auto FtZero = _mm256_setzero_si256();
			const auto FtOne = _mm256_set1_epi16(L1Q);

			const auto Rqf = static_cast<f32>(1 << FtShift) / static_cast<f32>(L1Q * L1Q * L2Q);
			const auto Rq = _mm256_set1_ps(Rqf);

			const auto Zero = _mm256_setzero_ps();
			const auto One = _mm256_set1_ps(1.0F);

			const auto outputBucket = OutputBucketing::getBucket(bbs);

			const auto l1WeightOffset = outputBucket * L1Size * L2Size;
			const auto l1BiasOffset   = outputBucket * L2Size;

			const auto l2WeightOffset = outputBucket * L2Size * L3Size;
			const auto l2BiasOffset   = outputBucket * L3Size;

			const auto l3WeightOffset = outputBucket * L3Size;
			const auto l3BiasOffset   = outputBucket;

			SimdArray<u8, L1Size> ftOut{};

			// stm perspective
			for (u32 inputIdx = 0; inputIdx < L1PairCount; inputIdx += I16ChunkSize * 4)
			{
				auto i1_0 = loadI(&stmInputs[inputIdx + I16ChunkSize * 0]);
				auto i1_1 = loadI(&stmInputs[inputIdx + I16ChunkSize * 1]);
				auto i1_2 = loadI(&stmInputs[inputIdx + I16ChunkSize * 2]);
				auto i1_3 = loadI(&stmInputs[inputIdx + I16ChunkSize * 3]);

				auto i2_0 = loadI(&stmInputs[inputIdx + L1PairCount + I16ChunkSize * 0]);
				auto i2_1 = loadI(&stmInputs[inputIdx + L1PairCount + I16ChunkSize * 1]);
				auto i2_2 = loadI(&stmInputs[inputIdx + L1PairCount + I16ChunkSize * 2]);
				auto i2_3 = loadI(&stmInputs[inputIdx + L1PairCount + I16ChunkSize * 3]);

				i1_0 = _mm256_min_epi16(i1_0, FtOne);
				i1_1 = _mm256_min_epi16(i1_1, FtOne);
				i1_2 = _mm256_min_epi16(i1_2, FtOne);
				i1_3 = _mm256_min_epi16(i1_3, FtOne);

				i2_0 = _mm256_min_epi16(i2_0, FtOne);
				i2_1 = _mm256_min_epi16(i2_1, FtOne);
				i2_2 = _mm256_min_epi16(i2_2, FtOne);
				i2_3 = _mm256_min_epi16(i2_3, FtOne);

				i1_0 = _mm256_max_epi16(i1_0, FtZero);
				i1_1 = _mm256_max_epi16(i1_1, FtZero);
				i1_2 = _mm256_max_epi16(i1_2, FtZero);
				i1_3 = _mm256_max_epi16(i1_3, FtZero);

				i1_0 = _mm256_slli_epi16(i1_0, 6);
				i1_1 = _mm256_slli_epi16(i1_1, 6);
				i1_2 = _mm256_slli_epi16(i1_2, 6);
				i1_3 = _mm256_slli_epi16(i1_3, 6);

				const auto p_0 = _mm256_mulhi_epi16(i1_0, i2_0);
				const auto p_1 = _mm256_mulhi_epi16(i1_1, i2_1);
				const auto p_2 = _mm256_mulhi_epi16(i1_2, i2_2);
				const auto p_3 = _mm256_mulhi_epi16(i1_3, i2_3);

				auto packed_0 = _mm256_packus_epi16(p_0, p_1);
				auto packed_1 = _mm256_packus_epi16(p_2, p_3);

				packed_0 = _mm256_permute4x64_epi64(packed_0, _MM_SHUFFLE(3, 1, 2, 0));
				packed_1 = _mm256_permute4x64_epi64(packed_1, _MM_SHUFFLE(3, 1, 2, 0));

				storeI(&ftOut[inputIdx + U8ChunkSize * 0], packed_0);
				storeI(&ftOut[inputIdx + U8ChunkSize * 1], packed_1);
			}

			// nstm perspective
			for (u32 inputIdx = 0; inputIdx < L1PairCount; inputIdx += I16ChunkSize * 4)
			{
				auto i1_0 = loadI(&nstmInputs[inputIdx + I16ChunkSize * 0]);
				auto i1_1 = loadI(&nstmInputs[inputIdx + I16ChunkSize * 1]);
				auto i1_2 = loadI(&nstmInputs[inputIdx + I16ChunkSize * 2]);
				auto i1_3 = loadI(&nstmInputs[inputIdx + I16ChunkSize * 3]);

				auto i2_0 = loadI(&nstmInputs[inputIdx + L1PairCount + I16ChunkSize * 0]);
				auto i2_1 = loadI(&nstmInputs[inputIdx + L1PairCount + I16ChunkSize * 1]);
				auto i2_2 = loadI(&nstmInputs[inputIdx + L1PairCount + I16ChunkSize * 2]);
				auto i2_3 = loadI(&nstmInputs[inputIdx + L1PairCount + I16ChunkSize * 3]);

				i1_0 = _mm256_min_epi16(i1_0, FtOne);
				i1_1 = _mm256_min_epi16(i1_1, FtOne);
				i1_2 = _mm256_min_epi16(i1_2, FtOne);
				i1_3 = _mm256_min_epi16(i1_3, FtOne);

				i2_0 = _mm256_min_epi16(i2_0, FtOne);
				i2_1 = _mm256_min_epi16(i2_1, FtOne);
				i2_2 = _mm256_min_epi16(i2_2, FtOne);
				i2_3 = _mm256_min_epi16(i2_3, FtOne);

				i1_0 = _mm256_max_epi16(i1_0, FtZero);
				i1_1 = _mm256_max_epi16(i1_1, FtZero);
				i1_2 = _mm256_max_epi16(i1_2, FtZero);
				i1_3 = _mm256_max_epi16(i1_3, FtZero);

				i1_0 = _mm256_slli_epi16(i1_0, 6);
				i1_1 = _mm256_slli_epi16(i1_1, 6);
				i1_2 = _mm256_slli_epi16(i1_2, 6);
				i1_3 = _mm256_slli_epi16(i1_3, 6);

				const auto p_0 = _mm256_mulhi_epi16(i1_0, i2_0);
				const auto p_1 = _mm256_mulhi_epi16(i1_1, i2_1);
				const auto p_2 = _mm256_mulhi_epi16(i1_2, i2_2);
				const auto p_3 = _mm256_mulhi_epi16(i1_3, i2_3);

				auto packed_0 = _mm256_packus_epi16(p_0, p_1);
				auto packed_1 = _mm256_packus_epi16(p_2, p_3);

				packed_0 = _mm256_permute4x64_epi64(packed_0, _MM_SHUFFLE(3, 1, 2, 0));
				packed_1 = _mm256_permute4x64_epi64(packed_1, _MM_SHUFFLE(3, 1, 2, 0));

				storeI(&ftOut[L1PairCount + inputIdx + U8ChunkSize * 0], packed_0);
				storeI(&ftOut[L1PairCount + inputIdx + U8ChunkSize * 1], packed_1);
			}

			SimdArray<f32, L2Size> l1Out{};

			// SAFETY: i8 (signed char) can safely be aliased to any type
			const auto *ftOutI32s = reinterpret_cast<const i32 *>(ftOut.data());

			auto l1Intermediate_0 = _mm256_setzero_si256();
			auto l1Intermediate_1 = _mm256_setzero_si256();
			auto l1Intermediate_2 = _mm256_setzero_si256();
			auto l1Intermediate_3 = _mm256_setzero_si256();

			for (u32 idx = 0; idx < L1Size; idx += I8ChunkSizeI32 * 4)
			{
				const auto weightsStart = l1WeightOffset + idx * L2Size;

				const auto i_0 = _mm256_set1_epi32(ftOutI32s[idx / I8ChunkSizeI32 + 0]);
				const auto i_1 = _mm256_set1_epi32(ftOutI32s[idx / I8ChunkSizeI32 + 1]);
				const auto i_2 = _mm256_set1_epi32(ftOutI32s[idx / I8ChunkSizeI32 + 2]);
				const auto i_3 = _mm256_set1_epi32(ftOutI32s[idx / I8ChunkSizeI32 + 3]);

				const auto w_0 = loadI(&l1Weights[weightsStart + I8ChunkSizeI32 * L2Size * 0]);
				const auto w_1 = loadI(&l1Weights[weightsStart + I8ChunkSizeI32 * L2Size * 1]);
				const auto w_2 = loadI(&l1Weights[weightsStart + I8ChunkSizeI32 * L2Size * 2]);
				const auto w_3 = loadI(&l1Weights[weightsStart + I8ChunkSizeI32 * L2Size * 3]);

				l1Intermediate_0 = dpbusd_epi32(l1Intermediate_0, i_0, w_0);
				l1Intermediate_1 = dpbusd_epi32(l1Intermediate_1, i_1, w_1);
				l1Intermediate_2 = dpbusd_epi32(l1Intermediate_2, i_2, w_2);
				l1Intermediate_3 = dpbusd_epi32(l1Intermediate_3, i_3, w_3);
			}

			const auto l1HalfSums_0 = _mm256_add_epi32(l1Intermediate_0, l1Intermediate_1);
			const auto l1HalfSums_1 = _mm256_add_epi32(l1Intermediate_2, l1Intermediate_3);

			const auto l1SumsI32 = _mm256_add_epi32(l1HalfSums_0, l1HalfSums_1);

			const auto l1b = _mm256_load_ps(&l1Biases[l1BiasOffset]);

			auto l1Sums = _mm256_cvtepi32_ps(l1SumsI32);

			l1Sums = _mm256_fmadd_ps(l1Sums, Rq, l1b);
			l1Sums = _mm256_min_ps(l1Sums, One);
			l1Sums = _mm256_max_ps(l1Sums, Zero);
			l1Sums = _mm256_mul_ps(l1Sums, l1Sums);

			_mm256_store_ps(l1Out.data(), l1Sums);

			SimdArray<f32, L3Size> l2Out{};
			std::memcpy(l2Out.data(), &l2Biases[l2BiasOffset], sizeof(l2Out));

			for (u32 inputIdx = 0; inputIdx < L2Size; inputIdx += 1)
			{
				const auto weightsStart = l2WeightOffset + inputIdx * L3Size;

				const auto i = _mm256_set1_ps(l1Out[inputIdx]);

				for (u32 outputIdx = 0; outputIdx < L3Size; outputIdx += F32ChunkSize * 4)
				{
					const auto w_0 = _mm256_load_ps(&l2Weights[weightsStart + outputIdx + F32ChunkSize * 0]);
					const auto w_1 = _mm256_load_ps(&l2Weights[weightsStart + outputIdx + F32ChunkSize * 1]);
					const auto w_2 = _mm256_load_ps(&l2Weights[weightsStart + outputIdx + F32ChunkSize * 2]);
					const auto w_3 = _mm256_load_ps(&l2Weights[weightsStart + outputIdx + F32ChunkSize * 3]);

					auto out_0 = _mm256_load_ps(&l2Out[outputIdx + F32ChunkSize * 0]);
					auto out_1 = _mm256_load_ps(&l2Out[outputIdx + F32ChunkSize * 1]);
					auto out_2 = _mm256_load_ps(&l2Out[outputIdx + F32ChunkSize * 2]);
					auto out_3 = _mm256_load_ps(&l2Out[outputIdx + F32ChunkSize * 3]);

					out_0 = _mm256_fmadd_ps(i, w_0, out_0);
					out_1 = _mm256_fmadd_ps(i, w_1, out_1);
					out_2 = _mm256_fmadd_ps(i, w_2, out_2);
					out_3 = _mm256_fmadd_ps(i, w_3, out_3);

					_mm256_store_ps(&l2Out[outputIdx + F32ChunkSize * 0], out_0);
					_mm256_store_ps(&l2Out[outputIdx + F32ChunkSize * 1], out_1);
					_mm256_store_ps(&l2Out[outputIdx + F32ChunkSize * 2], out_2);
					_mm256_store_ps(&l2Out[outputIdx + F32ChunkSize * 3], out_3);
				}
			}

			auto l3Out_0 = _mm256_setzero_ps();
			auto l3Out_1 = _mm256_setzero_ps();
			auto l3Out_2 = _mm256_setzero_ps();
			auto l3Out_3 = _mm256_setzero_ps();

			for (u32 inputIdx = 0; inputIdx < L3Size; inputIdx += F32ChunkSize * 4)
			{
				const auto weightIdx = l3WeightOffset + inputIdx;

				auto i_0 = _mm256_load_ps(&l2Out[inputIdx + F32ChunkSize * 0]);
				auto i_1 = _mm256_load_ps(&l2Out[inputIdx + F32ChunkSize * 1]);
				auto i_2 = _mm256_load_ps(&l2Out[inputIdx + F32ChunkSize * 2]);
				auto i_3 = _mm256_load_ps(&l2Out[inputIdx + F32ChunkSize * 3]);

				const auto w_0 = _mm256_load_ps(&l3Weights[weightIdx + F32ChunkSize * 0]);
				const auto w_1 = _mm256_load_ps(&l3Weights[weightIdx + F32ChunkSize * 1]);
				const auto w_2 = _mm256_load_ps(&l3Weights[weightIdx + F32ChunkSize * 2]);
				const auto w_3 = _mm256_load_ps(&l3Weights[weightIdx + F32ChunkSize * 3]);

				i_0 = _mm256_min_ps(i_0, One);
				i_1 = _mm256_min_ps(i_1, One);
				i_2 = _mm256_min_ps(i_2, One);
				i_3 = _mm256_min_ps(i_3, One);

				i_0 = _mm256_max_ps(i_0, Zero);
				i_1 = _mm256_max_ps(i_1, Zero);
				i_2 = _mm256_max_ps(i_2, Zero);
				i_3 = _mm256_max_ps(i_3, Zero);

				i_0 = _mm256_mul_ps(i_0, i_0);
				i_1 = _mm256_mul_ps(i_1, i_1);
				i_2 = _mm256_mul_ps(i_2, i_2);
				i_3 = _mm256_mul_ps(i_3, i_3);

				l3Out_0 = _mm256_fmadd_ps(i_0, w_0, l3Out_0);
				l3Out_1 = _mm256_fmadd_ps(i_1, w_1, l3Out_1);
				l3Out_2 = _mm256_fmadd_ps(i_2, w_2, l3Out_2);
				l3Out_3 = _mm256_fmadd_ps(i_3, w_3, l3Out_3);
			}

			const auto s0 = _mm256_add_ps(l3Out_0, l3Out_1);
			const auto s1 = _mm256_add_ps(l3Out_2, l3Out_3);

			const auto s = _mm256_add_ps(s0, s1);

			const auto high128 = _mm256_extractf128_ps(s, 1);
			const auto low128 = _mm256_castps256_ps128(s);
			const auto sum128 = _mm_add_ps(high128, low128);

			const auto high64 = _mm_movehl_ps(sum128, sum128);
			const auto sum64 = _mm_add_ps(sum128, high64);

			const auto high32 = _mm_shuffle_ps(sum64, sum64, _MM_SHUFFLE(0, 0, 0, 1));
			const auto sum32 = _mm_add_ss(sum64, high32);

			const auto l3Out = l3Biases[l3BiasOffset] + _mm_cvtss_f32(sum32);

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
