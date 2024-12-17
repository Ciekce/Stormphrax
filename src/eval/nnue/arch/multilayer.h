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

#include "../output.h"
#include "../../../util/simd.h"
#include "../io.h"
#include "../../../util/multi_array.h"

namespace stormphrax::eval::nnue::arch
{
	// implements an (inputs->L1)x2->(L2->L3->1)xN network, with
	// pairwise clipped ReLU on L1 and squared ReLU on later layers
	template <u32 L1Size, u32 L2Size, u32 L3Size, u32 FtScaleBits,
	    u32 FtQ, u32 L1Q, typename L2Activation, typename L3Activation,
		output::OutputBucketing OutputBucketing, i32 Scale>
	struct PairwiseMultilayerCReLU
	{
		static_assert(L1Size % (util::simd::ChunkSize<i16>) == 0);
		static_assert(L2Size % 16 == 0 || L2Size == 8);
		static_assert(L3Size % 16 == 0);

		using OutputType = i32;
		static constexpr u32 OutputCount = 1;

		static constexpr bool Pairwise = true;

	private:
		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		SP_SIMD_ALIGNAS std::array<i8,  OutputBucketCount * L1Size * L2Size> l1Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *          L2Size> l1Biases{};

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * L2Size * L3Size> l2Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount *          L3Size> l2Biases{};

		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount * L3Size> l3Weights{};
		SP_SIMD_ALIGNAS std::array<f32, OutputBucketCount>          l3Biases{};

		inline auto activateFt(std::span<const i16, L1Size> stmInputs,
			std::span<const i16, L1Size> nstmInputs,
			std::span<u8, L1Size> outputs) const
		{
			using namespace util::simd;

			static constexpr auto PairCount = L1Size / 2;

			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);

			const auto zero_ = zero<i16>();
			const auto one = set1<i16>(FtQ);

			const auto activatePerspective = [&](
				std::span<const i16, L1Size> inputs, u32 outputOffset)
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

					i1_0 = min<i16>(i1_0, one);
					i1_1 = min<i16>(i1_1, one);
					i1_2 = min<i16>(i1_2, one);
					i1_3 = min<i16>(i1_3, one);

					i2_0 = min<i16>(i2_0, one);
					i2_1 = min<i16>(i2_1, one);
					i2_2 = min<i16>(i2_2, one);
					i2_3 = min<i16>(i2_3, one);

					i1_0 = max<i16>(i1_0, zero_);
					i1_1 = max<i16>(i1_1, zero_);
					i1_2 = max<i16>(i1_2, zero_);
					i1_3 = max<i16>(i1_3, zero_);

					i1_0 = shiftLeft<i16>(i1_0, FtScaleBits);
					i1_1 = shiftLeft<i16>(i1_1, FtScaleBits);
					i1_2 = shiftLeft<i16>(i1_2, FtScaleBits);
					i1_3 = shiftLeft<i16>(i1_3, FtScaleBits);

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

			activatePerspective( stmInputs, 0);
			activatePerspective(nstmInputs, PairCount);
		}

		// Extremely dirty hack to allow 8-neuron L2 with AVX512
		// Hardcoded to squared ReLU
		//TODO: literally anything but this
#if SP_HAS_AVX512
	private:
		inline auto propagateL1Avx2
			(u32 bucket, std::span<const u8, L1Size> inputs, std::span<f32, L2Size> outputs) const
		{
			static_assert(L2Activation::Id == activation::SquaredReLU::Id);

			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);
			static constexpr auto ChunkSizeI32 = sizeof(__m256i) / sizeof(i32);

			const auto dpbusd = [](__m256i sum, __m256i u, __m256i i)
			{
#if SP_HAS_AVX512VNNI && __AVX512VL__
				return _mm256_dpbusd_epi32(sum, u, i);
#else
				const auto p = _mm256_maddubs_epi16(u, i);
				const auto w = _mm256_madd_epi16(p, _mm256_set1_epi16(1));
				return _mm256_add_epi32(sum, w);
#endif
			};

			const auto oneOverQuant = _mm256_set1_ps(
				static_cast<f32>(1 << (16 - FtScaleBits)) / static_cast<f32>(FtQ * FtQ * L1Q));

			const auto weightOffset = bucket * L2Size * L1Size;
			const auto biasOffset   = bucket * L2Size;

			// SAFETY: u8 (unsigned char) can safely be aliased to any type
			const auto *inI32s = reinterpret_cast<const i32 *>(inputs.data());

			SP_SIMD_ALIGNAS util::MultiArray<__m256i, L2Size / ChunkSizeI32, 4> intermediate{};

			for (u32 inputIdx = 0; inputIdx < L1Size; inputIdx += I8ChunkSizeI32 * 4)
			{
				const auto weightsStart = weightOffset + inputIdx * L2Size;

				const auto i_0 = _mm256_set1_epi32(inI32s[inputIdx / I8ChunkSizeI32 + 0]);
				const auto i_1 = _mm256_set1_epi32(inI32s[inputIdx / I8ChunkSizeI32 + 1]);
				const auto i_2 = _mm256_set1_epi32(inI32s[inputIdx / I8ChunkSizeI32 + 2]);
				const auto i_3 = _mm256_set1_epi32(inI32s[inputIdx / I8ChunkSizeI32 + 3]);

				for (u32 outputIdx = 0; outputIdx < L2Size; outputIdx += ChunkSizeI32)
				{
					auto &v = intermediate[outputIdx / ChunkSizeI32];

					const auto w_0 = _mm256_load_epi32
						(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 0)]);
					const auto w_1 = _mm256_load_epi32
						(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 1)]);
					const auto w_2 = _mm256_load_epi32
						(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 2)]);
					const auto w_3 = _mm256_load_epi32
						(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 3)]);

					v[0] = dpbusd(v[0], i_0, w_0);
					v[1] = dpbusd(v[1], i_1, w_1);
					v[2] = dpbusd(v[2], i_2, w_2);
					v[3] = dpbusd(v[3], i_3, w_3);
				}
			}

			for (u32 idx = 0; idx < L2Size; idx += ChunkSizeI32)
			{
				const auto &v = intermediate[idx / ChunkSizeI32];

				const auto halfSums_0 = _mm256_add_epi32(v[0], v[1]);
				const auto halfSums_1 = _mm256_add_epi32(v[2], v[3]);

				const auto sums = _mm256_add_epi32(halfSums_0, halfSums_1);

				const auto biases = _mm256_load_ps(&l1Biases[biasOffset + idx]);

				auto out = _mm256_cvtepi32_ps(sums);

				out = _mm256_fmadd_ps(out, oneOverQuant, biases);
				out = _mm256_max_ps(out, _mm256_setzero_ps());
				out = _mm256_mul_ps(out, out);

				_mm256_store_ps(&outputs[idx], out);
			}
		}

	public:
#endif

		// Take activated feature transformer outputs, propagate L1, dequantise and activate
		inline auto propagateL1(u32 bucket, std::span<const u8, L1Size> inputs, std::span<f32, L2Size> outputs) const
		{
#if SP_HAS_AVX512
			if constexpr (L2Size == 8)
			{
				propagateL1Avx2(bucket, inputs, outputs);
				return;
			}
#endif

			using namespace util::simd;

			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);

			const auto oneOverQuant = set1<f32>(
				static_cast<f32>(1 << (16 - FtScaleBits)) / static_cast<f32>(FtQ * FtQ * L1Q));

			const auto weightOffset = bucket * L2Size * L1Size;
			const auto biasOffset   = bucket * L2Size;

			// SAFETY: u8 (unsigned char) can safely be aliased to any type
			const auto *inI32s = reinterpret_cast<const i32 *>(inputs.data());

			SP_SIMD_ALIGNAS util::MultiArray<Vector<i32>, L2Size / ChunkSize<i32>, 4> intermediate{};

			for (u32 inputIdx = 0; inputIdx < L1Size; inputIdx += I8ChunkSizeI32 * 4)
			{
				const auto weightsStart = weightOffset + inputIdx * L2Size;

				const auto i_0 = set1<i32>(inI32s[inputIdx / I8ChunkSizeI32 + 0]);
				const auto i_1 = set1<i32>(inI32s[inputIdx / I8ChunkSizeI32 + 1]);
				const auto i_2 = set1<i32>(inI32s[inputIdx / I8ChunkSizeI32 + 2]);
				const auto i_3 = set1<i32>(inI32s[inputIdx / I8ChunkSizeI32 + 3]);

				for (u32 outputIdx = 0; outputIdx < L2Size; outputIdx += ChunkSize<i32>)
				{
					auto &v = intermediate[outputIdx / ChunkSize<i32>];

					const auto w_0 = load<i8>(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 0)]);
					const auto w_1 = load<i8>(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 1)]);
					const auto w_2 = load<i8>(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 2)]);
					const auto w_3 = load<i8>(&l1Weights[weightsStart + I8ChunkSizeI32 * (outputIdx + L2Size * 3)]);

					v[0] = dpbusd<i32>(v[0], i_0, w_0);
					v[1] = dpbusd<i32>(v[1], i_1, w_1);
					v[2] = dpbusd<i32>(v[2], i_2, w_2);
					v[3] = dpbusd<i32>(v[3], i_3, w_3);
				}
			}

			for (u32 idx = 0; idx < L2Size; idx += ChunkSize<i32>)
			{
				const auto &v = intermediate[idx / ChunkSize<i32>];

				const auto halfSums_0 = add<i32>(v[0], v[1]);
				const auto halfSums_1 = add<i32>(v[2], v[3]);

				const auto sums = add<i32>(halfSums_0, halfSums_1);

				const auto biases = load<f32>(&l1Biases[biasOffset + idx]);

				auto out = cast<i32, f32>(sums);

				out = fma<f32>(out, oneOverQuant, biases);
				out = L2Activation::template activate<f32>(out);

				store<f32>(&outputs[idx], out);
			}
		}

		// Take activated L1 outputs and propagate L2
		// Does not activate outputs
		inline auto propagateL2(u32 bucket, std::span<const f32, L2Size> inputs, std::span<f32, L3Size> outputs) const
		{
			using namespace util::simd;

			const auto weightOffset = bucket * L3Size * L2Size;
			const auto biasOffset   = bucket * L3Size;

			std::memcpy(outputs.data(), &l2Biases[biasOffset], outputs.size_bytes());

			// avx512
			if constexpr (ChunkSize<f32> * 4 > L3Size)
			{
				for (u32 inputIdx = 0; inputIdx < L2Size; ++inputIdx)
				{
					const auto weightsStart = weightOffset + inputIdx * L3Size;

					const auto i = set1<f32>(inputs[inputIdx]);

					for (u32 outputIdx = 0; outputIdx < L3Size; outputIdx += ChunkSize<f32> * 2)
					{
						const auto w_0 = load<f32>(&l2Weights[weightsStart + outputIdx + ChunkSize<f32> * 0]);
						const auto w_1 = load<f32>(&l2Weights[weightsStart + outputIdx + ChunkSize<f32> * 1]);

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
				for (u32 inputIdx = 0; inputIdx < L2Size; ++inputIdx)
				{
					const auto weightsStart = weightOffset + inputIdx * L3Size;

					const auto i = set1<f32>(inputs[inputIdx]);

					for (u32 outputIdx = 0; outputIdx < L3Size; outputIdx += ChunkSize<f32> * 4)
					{
						const auto w_0 = load<f32>(&l2Weights[weightsStart + outputIdx + ChunkSize<f32> * 0]);
						const auto w_1 = load<f32>(&l2Weights[weightsStart + outputIdx + ChunkSize<f32> * 1]);
						const auto w_2 = load<f32>(&l2Weights[weightsStart + outputIdx + ChunkSize<f32> * 2]);
						const auto w_3 = load<f32>(&l2Weights[weightsStart + outputIdx + ChunkSize<f32> * 3]);

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

		inline auto propagateL3(u32 bucket, std::span<const f32, L3Size> inputs, std::span<f32, 1> outputs) const
		{
			using namespace util::simd;

			const auto weightOffset = bucket * L3Size;
			const auto biasOffset   = bucket;

			Vector<f32> s;

			// avx512
			if constexpr (ChunkSize<f32> * 4 > L3Size)
			{
				auto out_0 = zero<f32>();
				auto out_1 = zero<f32>();

				for (u32 inputIdx = 0; inputIdx < L3Size; inputIdx += ChunkSize<f32> * 2)
				{
					const auto weightIdx = weightOffset + inputIdx;

					auto i_0 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 0]);
					auto i_1 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 1]);

					const auto w_0 = load<f32>(&l3Weights[weightIdx + ChunkSize<f32> * 0]);
					const auto w_1 = load<f32>(&l3Weights[weightIdx + ChunkSize<f32> * 1]);

					i_0 = L3Activation::template activate<f32>(i_0);
					i_1 = L3Activation::template activate<f32>(i_1);

					out_0 = fma<f32>(i_0, w_0, out_0);
					out_1 = fma<f32>(i_1, w_1, out_1);
				}

				s = add<f32>(out_0, out_1);
			}
			else
			{
				auto l3Out_0 = zero<f32>();
				auto l3Out_1 = zero<f32>();
				auto l3Out_2 = zero<f32>();
				auto l3Out_3 = zero<f32>();

				for (u32 inputIdx = 0; inputIdx < L3Size; inputIdx += ChunkSize<f32> * 4)
				{
					const auto weightIdx = weightOffset + inputIdx;

					auto i_0 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 0]);
					auto i_1 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 1]);
					auto i_2 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 2]);
					auto i_3 = load<f32>(&inputs[inputIdx + ChunkSize<f32> * 3]);

					const auto w_0 = load<f32>(&l3Weights[weightIdx + ChunkSize<f32> * 0]);
					const auto w_1 = load<f32>(&l3Weights[weightIdx + ChunkSize<f32> * 1]);
					const auto w_2 = load<f32>(&l3Weights[weightIdx + ChunkSize<f32> * 2]);
					const auto w_3 = load<f32>(&l3Weights[weightIdx + ChunkSize<f32> * 3]);

					i_0 = max<f32>(i_0, zero<f32>());
					i_1 = max<f32>(i_1, zero<f32>());
					i_2 = max<f32>(i_2, zero<f32>());
					i_3 = max<f32>(i_3, zero<f32>());

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

			outputs[0] = l3Biases[biasOffset] + hsum<f32>(s);
		}

	public:
		inline auto propagate(u32 bucket,
			std::span<const i16, L1Size>  stmInputs,
			std::span<const i16, L1Size> nstmInputs,
			std::span<OutputType, OutputCount> outputs) const
		{
			using namespace util::simd;

			assert(isAligned( stmInputs.data()));
			assert(isAligned(nstmInputs.data()));
			assert(isAligned(   outputs.data()));

			Array<u8, L1Size> activatedFt;
			Array<f32, L2Size> l1Out;
			Array<f32, L3Size> l2Out;
			Array<f32, 1> l3Out;

			activateFt(stmInputs, nstmInputs, activatedFt);
			propagateL1(bucket, activatedFt, l1Out);
			propagateL2(bucket, l1Out, l2Out);
			propagateL3(bucket, l2Out, l3Out);

			outputs[0] = static_cast<i32>(l3Out[0] * Scale);
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
