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
	// airwise clipped ReLU on the FT, dual activation on L1,
	// and clipped ReLU on L2
	template <u32 L1Size, u32 L2Size, u32 L3Size, u32 FtScaleBits,
	    u32 FtQBits, u32 L1QBits, output::OutputBucketing OutputBucketing, i32 Scale>
	struct PairwiseMultilayerCReLUDualActCReLU
	{
		static constexpr u32 ArchId = 3;

		static_assert(L2Size % 16 == 0);
		static_assert(L3Size % 16 == 0);

		using OutputType = i32;
		static constexpr u32 OutputCount = 1;

		static constexpr bool Pairwise = true;
		static constexpr bool RequiresFtPermute = util::simd::PackNonSequential;

	private:
		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		SP_SIMD_ALIGNAS std::array<i8,  OutputBucketCount * L1Size * L2Size> l1Weights{};
		SP_SIMD_ALIGNAS std::array<i32, OutputBucketCount *          L2Size> l1Biases{};

		SP_SIMD_ALIGNAS std::array<i32, OutputBucketCount * L2Size * 2 * L3Size> l2Weights{};
		SP_SIMD_ALIGNAS std::array<i32, OutputBucketCount *              L3Size> l2Biases{};

		SP_SIMD_ALIGNAS std::array<i32, OutputBucketCount * L3Size> l3Weights{};
		SP_SIMD_ALIGNAS std::array<i32, OutputBucketCount>          l3Biases{};

		static constexpr i32 QuantBits = 6;
		static constexpr i32 Q = 1 << QuantBits;

		inline auto activateFt(std::span<const i16, L1Size> stmInputs,
			std::span<const i16, L1Size> nstmInputs,
			std::span<u8, L1Size> outputs) const
		{
			using namespace util::simd;

			static constexpr auto PairCount = L1Size / 2;
			static_assert(PairCount % (util::simd::ChunkSize<i16> * 4) == 0);

			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);

			const auto zero_ = zero<i16>();
			const auto one = set1<i16>((1 << FtQBits) - 1);

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

					const auto p_0 = shiftLeftMulHi<i16>(i1_0, i2_0, FtScaleBits);
					const auto p_1 = shiftLeftMulHi<i16>(i1_1, i2_1, FtScaleBits);
					const auto p_2 = shiftLeftMulHi<i16>(i1_2, i2_2, FtScaleBits);
					const auto p_3 = shiftLeftMulHi<i16>(i1_3, i2_3, FtScaleBits);

					const auto packed_0 = packUnsigned<i16>(p_0, p_1);
					const auto packed_1 = packUnsigned<i16>(p_2, p_3);

					store<u8>(&outputs[outputOffset + inputIdx + ChunkSize<i8> * 0], packed_0);
					store<u8>(&outputs[outputOffset + inputIdx + ChunkSize<i8> * 1], packed_1);
				}
			};

			activatePerspective( stmInputs, 0);
			activatePerspective(nstmInputs, PairCount);
		}

		inline auto propagateL1(
			u32 bucket, std::span<const u8, L1Size> inputs, std::span<i32, L2Size * 2> outputs) const
		{
			using namespace util::simd;

			static constexpr auto I8ChunkSizeI32 = sizeof(i32) / sizeof(u8);

			static constexpr i32 Shift = 16 + QuantBits - FtScaleBits - FtQBits - FtQBits - L1QBits;

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
				const auto biases = load<i32>(&l1Biases[biasOffset + idx]);

				auto out = shift<i32, Shift>(sums);

				out = add<i32>(out, biases);

				// crelu side
				auto out0 = clamp<i32>(out, zero<i32>(), set1<i32>(Q));
				out0 = shiftLeft<i32>(out0, QuantBits);

				// screlu side
				// SF-style square-then-clip
				auto out1 = mulLo<i32>(out, out);
				out1 = min<i32>(out1, set1<i32>(Q * Q));

				store<i32>(&outputs[idx         ], out0);
				store<i32>(&outputs[idx + L2Size], out1);
			}
		}

		// Take activated L1 outputs and propagate L2
		// Does not activate outputs
		inline auto propagateL2(u32 bucket, std::span<const i32, L2Size * 2> inputs, std::span<i32, L3Size> outputs) const
		{
			using namespace util::simd;

			const auto weightOffset = bucket * L3Size * L2Size * 2;
			const auto biasOffset   = bucket * L3Size;

			std::memcpy(outputs.data(), &l2Biases[biasOffset], outputs.size_bytes());

			// avx512
			if constexpr (ChunkSize<i32> * 4 > L3Size)
			{
				for (u32 inputIdx = 0; inputIdx < L2Size * 2; ++inputIdx)
				{
					const auto weightsStart = weightOffset + inputIdx * L3Size;

					const auto i = set1<i32>(inputs[inputIdx]);

					for (u32 outputIdx = 0; outputIdx < L3Size; outputIdx += ChunkSize<i32> * 2)
					{
						const auto w_0 = load<i32>(&l2Weights[weightsStart + outputIdx + ChunkSize<i32> * 0]);
						const auto w_1 = load<i32>(&l2Weights[weightsStart + outputIdx + ChunkSize<i32> * 1]);

						auto out_0 = load<i32>(&outputs[outputIdx + ChunkSize<i32> * 0]);
						auto out_1 = load<i32>(&outputs[outputIdx + ChunkSize<i32> * 1]);

						const auto p_0 = mulLo<i32>(i, w_0);
						const auto p_1 = mulLo<i32>(i, w_1);

						out_0 = add<i32>(out_0, p_0);
						out_1 = add<i32>(out_1, p_1);

						store<i32>(&outputs[outputIdx + ChunkSize<i32> * 0], out_0);
						store<i32>(&outputs[outputIdx + ChunkSize<i32> * 1], out_1);
					}
				}
			}
			else
			{
				for (u32 inputIdx = 0; inputIdx < L2Size * 2; ++inputIdx)
				{
					const auto weightsStart = weightOffset + inputIdx * L3Size;

					const auto i = set1<i32>(inputs[inputIdx]);

					for (u32 outputIdx = 0; outputIdx < L3Size; outputIdx += ChunkSize<i32> * 4)
					{
						const auto w_0 = load<i32>(&l2Weights[weightsStart + outputIdx + ChunkSize<i32> * 0]);
						const auto w_1 = load<i32>(&l2Weights[weightsStart + outputIdx + ChunkSize<i32> * 1]);
						const auto w_2 = load<i32>(&l2Weights[weightsStart + outputIdx + ChunkSize<i32> * 2]);
						const auto w_3 = load<i32>(&l2Weights[weightsStart + outputIdx + ChunkSize<i32> * 3]);

						auto out_0 = load<i32>(&outputs[outputIdx + ChunkSize<i32> * 0]);
						auto out_1 = load<i32>(&outputs[outputIdx + ChunkSize<i32> * 1]);
						auto out_2 = load<i32>(&outputs[outputIdx + ChunkSize<i32> * 2]);
						auto out_3 = load<i32>(&outputs[outputIdx + ChunkSize<i32> * 3]);

						const auto p_0 = mulLo<i32>(i, w_0);
						const auto p_1 = mulLo<i32>(i, w_1);
						const auto p_2 = mulLo<i32>(i, w_2);
						const auto p_3 = mulLo<i32>(i, w_3);

						out_0 = add<i32>(out_0, p_0);
						out_1 = add<i32>(out_1, p_1);
						out_2 = add<i32>(out_2, p_2);
						out_3 = add<i32>(out_3, p_3);

						store<i32>(&outputs[outputIdx + ChunkSize<i32> * 0], out_0);
						store<i32>(&outputs[outputIdx + ChunkSize<i32> * 1], out_1);
						store<i32>(&outputs[outputIdx + ChunkSize<i32> * 2], out_2);
						store<i32>(&outputs[outputIdx + ChunkSize<i32> * 3], out_3);
					}
				}
			}
		}

		inline auto propagateL3(u32 bucket, std::span<const i32, L3Size> inputs, std::span<i32, 1> outputs) const
		{
			using namespace util::simd;

			const auto weightOffset = bucket * L3Size;
			const auto biasOffset   = bucket;

			const auto one = set1<i32>(Q * Q * Q);

			Vector<i32> s;

			// avx512
			if constexpr (ChunkSize<i32> * 4 > L3Size)
			{
				auto out_0 = zero<i32>();
				auto out_1 = zero<i32>();

				for (u32 inputIdx = 0; inputIdx < L3Size; inputIdx += ChunkSize<i32> * 2)
				{
					const auto weightIdx = weightOffset + inputIdx;

					auto i_0 = load<i32>(&inputs[inputIdx + ChunkSize<i32> * 0]);
					auto i_1 = load<i32>(&inputs[inputIdx + ChunkSize<i32> * 1]);

					const auto w_0 = load<i32>(&l3Weights[weightIdx + ChunkSize<i32> * 0]);
					const auto w_1 = load<i32>(&l3Weights[weightIdx + ChunkSize<i32> * 1]);

					i_0 = clamp<i32>(i_0, zero<i32>(), one);
					i_1 = clamp<i32>(i_1, zero<i32>(), one);

					i_0 = mulLo<i32>(i_0, w_0);
					i_1 = mulLo<i32>(i_1, w_1);

					out_0 = add<i32>(i_0, out_0);
					out_1 = add<i32>(i_1, out_1);
				}

				s = add<i32>(out_0, out_1);
			}
			else
			{
				auto out_0 = zero<i32>();
				auto out_1 = zero<i32>();
				auto out_2 = zero<i32>();
				auto out_3 = zero<i32>();

				for (u32 inputIdx = 0; inputIdx < L3Size; inputIdx += ChunkSize<i32> * 4)
				{
					const auto weightIdx = weightOffset + inputIdx;

					auto i_0 = load<i32>(&inputs[inputIdx + ChunkSize<i32> * 0]);
					auto i_1 = load<i32>(&inputs[inputIdx + ChunkSize<i32> * 1]);
					auto i_2 = load<i32>(&inputs[inputIdx + ChunkSize<i32> * 2]);
					auto i_3 = load<i32>(&inputs[inputIdx + ChunkSize<i32> * 3]);

					const auto w_0 = load<i32>(&l3Weights[weightIdx + ChunkSize<i32> * 0]);
					const auto w_1 = load<i32>(&l3Weights[weightIdx + ChunkSize<i32> * 1]);
					const auto w_2 = load<i32>(&l3Weights[weightIdx + ChunkSize<i32> * 2]);
					const auto w_3 = load<i32>(&l3Weights[weightIdx + ChunkSize<i32> * 3]);

					i_0 = clamp<i32>(i_0, zero<i32>(), one);
					i_1 = clamp<i32>(i_1, zero<i32>(), one);
					i_2 = clamp<i32>(i_2, zero<i32>(), one);
					i_3 = clamp<i32>(i_3, zero<i32>(), one);

					i_0 = mulLo<i32>(i_0, w_0);
					i_1 = mulLo<i32>(i_1, w_1);
					i_2 = mulLo<i32>(i_2, w_2);
					i_3 = mulLo<i32>(i_3, w_3);

					out_0 = add<i32>(i_0, out_0);
					out_1 = add<i32>(i_1, out_1);
					out_2 = add<i32>(i_2, out_2);
					out_3 = add<i32>(i_3, out_3);
				}

				const auto s0 = add<i32>(out_0, out_1);
				const auto s1 = add<i32>(out_2, out_3);

				s = add<i32>(s0, s1);
			}

			outputs[0] = (l3Biases[biasOffset] + hsum<i32>(s)) / Q;
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

			Array< u8, L1Size    > ftOut;
			Array<i32, L2Size * 2> l1Out;
			Array<i32, L3Size    > l2Out;
			Array<i32,          1> l3Out;

			activateFt(stmInputs, nstmInputs, ftOut);
			propagateL1(bucket, ftOut, l1Out);
			propagateL2(bucket, l1Out, l2Out);
			propagateL3(bucket, l2Out, l3Out);

			outputs[0] = l3Out[0] * Scale / (Q * Q * Q);
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

		template <typename W, typename B>
		static inline auto permuteFt(std::span<W> weights, std::span<B> biases)
		{
			using namespace util::simd;

			if constexpr (!PackNonSequential)
				return;

			static constexpr usize PackSize = PackOrdering.size();
			static constexpr usize ChunkSize = PackSize * PackGrouping;

			const auto permute = [&]<typename T, usize N>(std::span<T, N> values)
			{
				util::MultiArray<T, PackSize, PackGrouping> tmp;

				for (usize offset = 0; offset < values.size(); offset += ChunkSize)
				{
					std::copy(&values[offset], &values[offset] + ChunkSize, &tmp[0][0]);

					for (usize i = 0; i < PackSize; ++i)
					{
						std::ranges::copy(tmp[PackOrdering[i]], &values[offset + i * PackGrouping]);
					}
				}
			};

			permute(weights);
			permute(biases);
		}
	};
}
