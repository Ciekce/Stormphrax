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
#include "../io.h"
#include "../output.h"
#include "util/sparse.h"

namespace stormphrax::eval::nnue::arch {
    // implements an (inputs->L1)x2->(L2->L3->1)xN network, with
    // airwise clipped ReLU on the FT, squared clipped ReLU
    // or dual CReLU or SCReLU on L1, and clipped ReLU on L2
    template <
        typename FeatureSet,
        u32 kL1Size,
        u32 kL2Size,
        u32 kL3Size,
        u32 kFtScaleBits,
        u32 kFtQBits,
        u32 kL1QBits,
        bool kDualActivation,
        output::OutputBucketing OutputBucketing,
        i32 kScale>
    struct PairwiseMultilayerCReLUSCReLUCReLU {
        static constexpr u32 kArchId = kDualActivation ? 3 : 2;

        static_assert(kL2Size % 16 == 0);
        static_assert(kL3Size % 16 == 0);

        using OutputType = i32;
        static constexpr u32 kOutputCount = 1;

        static constexpr bool kPairwise = true;
        static constexpr bool kRequiresFtPermute = util::simd::kPackNonSequential;

    private:
        static constexpr auto kI8ChunkSizeI32 = sizeof(i32) / sizeof(u8);

        static constexpr u32 kL2SizeFull = kL2Size * (1 + kDualActivation);

        static constexpr auto kOutputBucketCount = OutputBucketing::kBucketCount;

        SP_SIMD_ALIGNAS std::array<i8, kOutputBucketCount * kL1Size * kL2Size> l1Weights{};
        SP_SIMD_ALIGNAS std::array<i32, kOutputBucketCount * kL2Size> l1Biases{};

        SP_SIMD_ALIGNAS std::array<i32, kOutputBucketCount * kL2SizeFull * kL3Size> l2Weights{};
        SP_SIMD_ALIGNAS std::array<i32, kOutputBucketCount * kL3Size> l2Biases{};

        SP_SIMD_ALIGNAS std::array<i32, kOutputBucketCount * kL3Size> l3Weights{};
        SP_SIMD_ALIGNAS std::array<i32, kOutputBucketCount> l3Biases{};

        static constexpr i32 kQuantBits = 6;
        static constexpr i32 kQ = 1 << kQuantBits;

        inline void activateFt(
            std::span<const i16, kL1Size> stmPsqInputs,
            std::span<const i16, kL1Size> nstmPsqInputs,
            std::span<const i16, kL1Size> stmThreatInputs,
            std::span<const i16, kL1Size> nstmThreatInputs,
            std::span<u8, kL1Size> outputs,
            sparse::SparseContext<kL1Size>& sparseCtx
        ) const {
            using namespace util::simd;

            static constexpr auto kPairCount = kL1Size / 2;
            static_assert(kPairCount % (kChunkSize<i16> * 2) == 0);

            const auto zero_ = zero<i16>();
            const auto one = set1<i16>((1 << kFtQBits) - 1);

            const auto activatePerspective = [&](std::span<const i16, kL1Size> psqInputs,
                                                 std::span<const i16, kL1Size> threatInputs,
                                                 u32 outputOffset) {
                for (u32 inputIdx = 0; inputIdx < kPairCount; inputIdx += kChunkSize<i16> * 2) {
                    auto i1_0 = load<i16>(&psqInputs[inputIdx + kChunkSize<i16> * 0]);
                    auto i1_1 = load<i16>(&psqInputs[inputIdx + kChunkSize<i16> * 1]);

                    auto i2_0 = load<i16>(&psqInputs[inputIdx + kPairCount + kChunkSize<i16> * 0]);
                    auto i2_1 = load<i16>(&psqInputs[inputIdx + kPairCount + kChunkSize<i16> * 1]);

                    if constexpr (FeatureSet::kThreatInputs) {
                        i1_0 = add<i16>(i1_0, load<i16>(&threatInputs[inputIdx + kChunkSize<i16> * 0]));
                        i1_1 = add<i16>(i1_1, load<i16>(&threatInputs[inputIdx + kChunkSize<i16> * 1]));

                        i2_0 = add<i16>(i2_0, load<i16>(&threatInputs[inputIdx + kPairCount + kChunkSize<i16> * 0]));
                        i2_1 = add<i16>(i2_1, load<i16>(&threatInputs[inputIdx + kPairCount + kChunkSize<i16> * 1]));
                    }

                    i1_0 = min<i16>(i1_0, one);
                    i1_1 = min<i16>(i1_1, one);

                    i2_0 = min<i16>(i2_0, one);
                    i2_1 = min<i16>(i2_1, one);

                    i1_0 = max<i16>(i1_0, zero_);
                    i1_1 = max<i16>(i1_1, zero_);

                    const auto p_0 = shiftLeftMulHi<i16>(i1_0, i2_0, kFtScaleBits);
                    const auto p_1 = shiftLeftMulHi<i16>(i1_1, i2_1, kFtScaleBits);

                    const auto packed_0 = packUnsigned<i16>(p_0, p_1);

                    store<u8>(&outputs[outputOffset + inputIdx + kChunkSize<i8> * 0], packed_0);
                }
            };

            activatePerspective(stmPsqInputs, stmThreatInputs, 0);
            activatePerspective(nstmPsqInputs, nstmThreatInputs, kPairCount);

            for (u32 output = 0; output < kL1Size; output += kChunkSize<i8> * 2) {
                const auto a = load<u8>(&outputs[output + kChunkSize<i8> * 0]);
                const auto b = load<u8>(&outputs[output + kChunkSize<i8> * 1]);
                sparseCtx.update(a, b);
            }
        }

        inline void propagateL1(
            u32 bucket,
            std::span<const u8, kL1Size> inputs,
            std::span<i32, kL2SizeFull> outputs,
            const sparse::SparseContext<kL1Size>& sparseCtx
        ) const {
            using namespace util::simd;

            static constexpr i32 kShift = 16 + kQuantBits - kFtScaleBits - kFtQBits - kFtQBits - kL1QBits;

            const auto weightOffset = bucket * kL2Size * kL1Size;
            const auto biasOffset = bucket * kL2Size;

            // SAFETY: u8 (unsigned char) can safely be aliased to any type
            const auto* inI32s = reinterpret_cast<const i32*>(inputs.data());

            SP_SIMD_ALIGNAS util::MultiArray<Vector<i32>, kL2Size / kChunkSize<i32>, 4> intermediate{};

            const auto quadChunks = sparseCtx.count() - (sparseCtx.count() % 4);

            for (usize chunk = 0; chunk < quadChunks; chunk += 4) {
                const auto idx_0 = sparseCtx.chunk(chunk + 0);
                const auto idx_1 = sparseCtx.chunk(chunk + 1);
                const auto idx_2 = sparseCtx.chunk(chunk + 2);
                const auto idx_3 = sparseCtx.chunk(chunk + 3);

                const auto ws_0 = weightOffset + idx_0 * kI8ChunkSizeI32 * kL2Size;
                const auto ws_1 = weightOffset + idx_1 * kI8ChunkSizeI32 * kL2Size;
                const auto ws_2 = weightOffset + idx_2 * kI8ChunkSizeI32 * kL2Size;
                const auto ws_3 = weightOffset + idx_3 * kI8ChunkSizeI32 * kL2Size;

                const auto i_0 = set1<i32>(inI32s[idx_0]);
                const auto i_1 = set1<i32>(inI32s[idx_1]);
                const auto i_2 = set1<i32>(inI32s[idx_2]);
                const auto i_3 = set1<i32>(inI32s[idx_3]);

                for (u32 outputIdx = 0; outputIdx < kL2Size; outputIdx += kChunkSize<i32>) {
                    auto& v = intermediate[outputIdx / kChunkSize<i32>];

                    const auto w_0 = load<i8>(&l1Weights[ws_0 + kI8ChunkSizeI32 * outputIdx]);
                    const auto w_1 = load<i8>(&l1Weights[ws_1 + kI8ChunkSizeI32 * outputIdx]);
                    const auto w_2 = load<i8>(&l1Weights[ws_2 + kI8ChunkSizeI32 * outputIdx]);
                    const auto w_3 = load<i8>(&l1Weights[ws_3 + kI8ChunkSizeI32 * outputIdx]);

                    v[0] = dpbusd<i32>(v[0], i_0, w_0);
                    v[1] = dpbusd<i32>(v[1], i_1, w_1);
                    v[2] = dpbusd<i32>(v[2], i_2, w_2);
                    v[3] = dpbusd<i32>(v[3], i_3, w_3);
                }
            }

            for (usize chunk = quadChunks; chunk < sparseCtx.count(); ++chunk) {
                const auto idx = sparseCtx.chunk(chunk);

                const auto ws = weightOffset + idx * kI8ChunkSizeI32 * kL2Size;

                const auto i = set1<i32>(inI32s[idx]);

                for (u32 outputIdx = 0; outputIdx < kL2Size; outputIdx += kChunkSize<i32>) {
                    auto& v = intermediate[outputIdx / kChunkSize<i32>];
                    const auto w = load<i8>(&l1Weights[ws + kI8ChunkSizeI32 * outputIdx]);
                    v[0] = dpbusd<i32>(v[0], i, w);
                }
            }

            for (u32 idx = 0; idx < kL2Size; idx += kChunkSize<i32>) {
                const auto& v = intermediate[idx / kChunkSize<i32>];

                const auto halfSums_0 = add<i32>(v[0], v[1]);
                const auto halfSums_1 = add<i32>(v[2], v[3]);

                const auto sums = add<i32>(halfSums_0, halfSums_1);
                const auto biases = load<i32>(&l1Biases[biasOffset + idx]);

                auto out = shift<i32, kShift>(sums);

                out = add<i32>(out, biases);

                if constexpr (kDualActivation) {
                    // crelu side
                    auto out0 = clamp<i32>(out, zero<i32>(), set1<i32>(kQ));
                    out0 = shiftLeft<i32>(out0, kQuantBits);

                    // screlu side
                    // SF-style square-then-clip
                    auto out1 = mulLo<i32>(out, out);
                    out1 = min<i32>(out1, set1<i32>(kQ * kQ));

                    store<i32>(&outputs[idx], out0);
                    store<i32>(&outputs[idx + kL2Size], out1);
                } else {
                    out = clamp<i32>(out, zero<i32>(), set1<i32>(kQ));
                    out = mulLo<i32>(out, out);

                    store<i32>(&outputs[idx], out);
                }
            }
        }

        // Take activated L1 outputs and propagate L2
        // Does not activate outputs
        inline void propagateL2(
            u32 bucket,
            std::span<const i32, kL2SizeFull> inputs,
            std::span<i32, kL3Size> outputs
        ) const {
            using namespace util::simd;

            const auto weightOffset = bucket * kL3Size * kL2SizeFull;
            const auto biasOffset = bucket * kL3Size;

            std::memcpy(outputs.data(), &l2Biases[biasOffset], outputs.size_bytes());

            // avx512
            if constexpr (kChunkSize<i32> * 4 > kL3Size) {
                for (u32 inputIdx = 0; inputIdx < kL2SizeFull; ++inputIdx) {
                    const auto weightsStart = weightOffset + inputIdx * kL3Size;

                    const auto i = set1<i32>(inputs[inputIdx]);

                    for (u32 outputIdx = 0; outputIdx < kL3Size; outputIdx += kChunkSize<i32> * 2) {
                        const auto w_0 = load<i32>(&l2Weights[weightsStart + outputIdx + kChunkSize<i32> * 0]);
                        const auto w_1 = load<i32>(&l2Weights[weightsStart + outputIdx + kChunkSize<i32> * 1]);

                        auto out_0 = load<i32>(&outputs[outputIdx + kChunkSize<i32> * 0]);
                        auto out_1 = load<i32>(&outputs[outputIdx + kChunkSize<i32> * 1]);

                        const auto p_0 = mulLo<i32>(i, w_0);
                        const auto p_1 = mulLo<i32>(i, w_1);

                        out_0 = add<i32>(out_0, p_0);
                        out_1 = add<i32>(out_1, p_1);

                        store<i32>(&outputs[outputIdx + kChunkSize<i32> * 0], out_0);
                        store<i32>(&outputs[outputIdx + kChunkSize<i32> * 1], out_1);
                    }
                }
            } else {
                for (u32 inputIdx = 0; inputIdx < kL2SizeFull; ++inputIdx) {
                    const auto weightsStart = weightOffset + inputIdx * kL3Size;

                    const auto i = set1<i32>(inputs[inputIdx]);

                    for (u32 outputIdx = 0; outputIdx < kL3Size; outputIdx += kChunkSize<i32> * 4) {
                        const auto w_0 = load<i32>(&l2Weights[weightsStart + outputIdx + kChunkSize<i32> * 0]);
                        const auto w_1 = load<i32>(&l2Weights[weightsStart + outputIdx + kChunkSize<i32> * 1]);
                        const auto w_2 = load<i32>(&l2Weights[weightsStart + outputIdx + kChunkSize<i32> * 2]);
                        const auto w_3 = load<i32>(&l2Weights[weightsStart + outputIdx + kChunkSize<i32> * 3]);

                        auto out_0 = load<i32>(&outputs[outputIdx + kChunkSize<i32> * 0]);
                        auto out_1 = load<i32>(&outputs[outputIdx + kChunkSize<i32> * 1]);
                        auto out_2 = load<i32>(&outputs[outputIdx + kChunkSize<i32> * 2]);
                        auto out_3 = load<i32>(&outputs[outputIdx + kChunkSize<i32> * 3]);

                        const auto p_0 = mulLo<i32>(i, w_0);
                        const auto p_1 = mulLo<i32>(i, w_1);
                        const auto p_2 = mulLo<i32>(i, w_2);
                        const auto p_3 = mulLo<i32>(i, w_3);

                        out_0 = add<i32>(out_0, p_0);
                        out_1 = add<i32>(out_1, p_1);
                        out_2 = add<i32>(out_2, p_2);
                        out_3 = add<i32>(out_3, p_3);

                        store<i32>(&outputs[outputIdx + kChunkSize<i32> * 0], out_0);
                        store<i32>(&outputs[outputIdx + kChunkSize<i32> * 1], out_1);
                        store<i32>(&outputs[outputIdx + kChunkSize<i32> * 2], out_2);
                        store<i32>(&outputs[outputIdx + kChunkSize<i32> * 3], out_3);
                    }
                }
            }
        }

        inline void propagateL3(u32 bucket, std::span<const i32, kL3Size> inputs, std::span<i32, 1> outputs) const {
            using namespace util::simd;

            const auto weightOffset = bucket * kL3Size;
            const auto biasOffset = bucket;

            const auto one = set1<i32>(kQ * kQ * kQ);

            Vector<i32> s;

            // avx512
            if constexpr (kChunkSize<i32> * 4 > kL3Size) {
                auto out_0 = zero<i32>();
                auto out_1 = zero<i32>();

                for (u32 inputIdx = 0; inputIdx < kL3Size; inputIdx += kChunkSize<i32> * 2) {
                    const auto weightIdx = weightOffset + inputIdx;

                    auto i_0 = load<i32>(&inputs[inputIdx + kChunkSize<i32> * 0]);
                    auto i_1 = load<i32>(&inputs[inputIdx + kChunkSize<i32> * 1]);

                    const auto w_0 = load<i32>(&l3Weights[weightIdx + kChunkSize<i32> * 0]);
                    const auto w_1 = load<i32>(&l3Weights[weightIdx + kChunkSize<i32> * 1]);

                    i_0 = clamp<i32>(i_0, zero<i32>(), one);
                    i_1 = clamp<i32>(i_1, zero<i32>(), one);

                    i_0 = mulLo<i32>(i_0, w_0);
                    i_1 = mulLo<i32>(i_1, w_1);

                    out_0 = add<i32>(i_0, out_0);
                    out_1 = add<i32>(i_1, out_1);
                }

                s = add<i32>(out_0, out_1);
            } else {
                auto out_0 = zero<i32>();
                auto out_1 = zero<i32>();
                auto out_2 = zero<i32>();
                auto out_3 = zero<i32>();

                for (u32 inputIdx = 0; inputIdx < kL3Size; inputIdx += kChunkSize<i32> * 4) {
                    const auto weightIdx = weightOffset + inputIdx;

                    auto i_0 = load<i32>(&inputs[inputIdx + kChunkSize<i32> * 0]);
                    auto i_1 = load<i32>(&inputs[inputIdx + kChunkSize<i32> * 1]);
                    auto i_2 = load<i32>(&inputs[inputIdx + kChunkSize<i32> * 2]);
                    auto i_3 = load<i32>(&inputs[inputIdx + kChunkSize<i32> * 3]);

                    const auto w_0 = load<i32>(&l3Weights[weightIdx + kChunkSize<i32> * 0]);
                    const auto w_1 = load<i32>(&l3Weights[weightIdx + kChunkSize<i32> * 1]);
                    const auto w_2 = load<i32>(&l3Weights[weightIdx + kChunkSize<i32> * 2]);
                    const auto w_3 = load<i32>(&l3Weights[weightIdx + kChunkSize<i32> * 3]);

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

            outputs[0] = (l3Biases[biasOffset] + hsum<i32>(s)) / kQ;
        }

    public:
        inline void propagate(
            u32 bucket,
            std::span<const i16, kL1Size> stmPsqInputs,
            std::span<const i16, kL1Size> nstmPsqInputs,
            std::span<const i16, kL1Size> stmThreatInputs,
            std::span<const i16, kL1Size> nstmThreatInputs,
            std::span<OutputType, kOutputCount> outputs
        ) const {
            using namespace util::simd;

            assert(isAligned(stmInputs.data()));
            assert(isAligned(nstmInputs.data()));
            assert(isAligned(outputs.data()));

            sparse::SparseContext<kL1Size> sparseCtx;

            Array<u8, kL1Size> ftOut;
            Array<i32, kL2SizeFull> l1Out;
            Array<i32, kL3Size> l2Out;
            Array<i32, 1> l3Out;

            activateFt(stmPsqInputs, nstmPsqInputs, stmThreatInputs, nstmThreatInputs, ftOut, sparseCtx);
            propagateL1(bucket, ftOut, l1Out, sparseCtx);
            propagateL2(bucket, l1Out, l2Out);
            propagateL3(bucket, l2Out, l3Out);

#if SP_SPARSE_BENCH_L1_SIZE > 0
            sparse::trackActivations(ftOut);
#endif

            outputs[0] = l3Out[0] * kScale / (kQ * kQ * kQ);
        }

        inline bool readFrom(IParamStream& stream) {
            return stream.read(l1Weights) && stream.read(l1Biases) && stream.read(l2Weights) && stream.read(l2Biases)
                && stream.read(l3Weights) && stream.read(l3Biases);
        }

        inline bool writeTo(IParamStream& stream) const {
            return stream.write(l1Weights) && stream.write(l1Biases) && stream.write(l2Weights)
                && stream.write(l2Biases) && stream.write(l3Weights) && stream.write(l3Biases);
        }

        template <typename W, typename T, typename B>
        static inline void permuteFt(std::span<W> psqWeights, std::span<T> threatWeights, std::span<B> biases) {
            using namespace util::simd;

            if constexpr (!kPackNonSequential) {
                return;
            }

            static constexpr usize kPackSize = kPackOrdering.size();
            static constexpr usize kChunkSize = kPackSize * kPackGrouping;

            const auto permute = [&]<typename P, usize kN>(std::span<P, kN> values) {
                util::MultiArray<P, kPackSize, kPackGrouping> tmp;

                for (usize offset = 0; offset < values.size(); offset += kChunkSize) {
                    std::copy(&values[offset], &values[offset] + kChunkSize, &tmp[0][0]);

                    for (usize i = 0; i < kPackSize; ++i) {
                        std::ranges::copy(tmp[kPackOrdering[i]], &values[offset + i * kPackGrouping]);
                    }
                }
            };

            permute(psqWeights);
            permute(threatWeights);
            permute(biases);
        }
    };
} // namespace stormphrax::eval::nnue::arch
