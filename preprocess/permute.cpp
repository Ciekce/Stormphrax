/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "../src/types.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <type_traits>

#include "../src/eval/arch.h"
#include "../src/eval/header.h"
#include "../src/util/multi_array.h"

using namespace stormphrax;
using namespace stormphrax::eval;

struct ThreatInputFtWeights {
    util::MultiArray<i16, InputFeatureSet::kBucketCount * InputFeatureSet::kInputSize * kL1Size> psq;
    util::MultiArray<i8, InputFeatureSet::kThreatFeatures * kL1Size> threat;
};

struct NoThreatInputFtWeights {
    util::MultiArray<i16, InputFeatureSet::kBucketCount * InputFeatureSet::kInputSize * kL1Size> psq;
    static std::array<i8, 0> threat;
};

using FtWeights = std::conditional_t<InputFeatureSet::kThreatInputs, ThreatInputFtWeights, NoThreatInputFtWeights>;

constexpr auto kL2Weights = kL2Size * (1 + kDualActivation);

struct LoadedNetwork {
    FtWeights ftWeights;
    std::array<i16, kL1Size> ftBiases;
    std::array<i8, OutputBucketing::kBucketCount * kL1Size * kL2Size> l1Weights;
    std::array<i32, OutputBucketing::kBucketCount * kL2Size> l1Biases;
    std::array<i32, OutputBucketing::kBucketCount * kL2Weights * kL3Size> l2Weights;
    std::array<i32, OutputBucketing::kBucketCount * kL3Size> l2Biases;
    std::array<i32, OutputBucketing::kBucketCount * kL3Size> l3Weights;
    std::array<i32, OutputBucketing::kBucketCount> l3Biases;
};

i32 main(i32 argc, char* argv[]) {
    if (argc < 3) {
        eprintln("usage: {} <input net> <output net>", argv[0]);
        return 1;
    }

    std::ifstream in{argv[1], std::ios::binary};
    if (!in) {
        eprintln("Failed to open input file \"{}\"", argv[1]);
        return 1;
    }

    NetworkHeader header;
    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))) {
        eprintln("Failed to read network header");
        return 1;
    }

    if (header.magic != std::array{'C', 'B', 'N', 'F'}) {
        eprintln("Invalid header magic");
        return 1;
    }

    std::ofstream out{argv[2], std::ios::binary};
    if (!out) {
        eprintln("Failed to open output file \"{}\"", argv[2]);
        return 1;
    }

    if (!out.write(reinterpret_cast<const char*>(&header), sizeof(header))) {
        eprintln("Failed to write header");
        return 1;
    }

    if (testFlags(header.flags, NetworkFlags::kZstdCompressed)) {
        println("Compressed network, skipping permutation");
        std::copy(std::istreambuf_iterator{in}, std::istreambuf_iterator<char>{}, std::ostreambuf_iterator{out});
        if (!out) {
            eprintln("Failed to write network");
            return 1;
        }
        return 0;
    }

    if constexpr (!LayeredArch::kRequiresFtPermute) {
        println("No permutation required for current network arch");
        std::copy(std::istreambuf_iterator{in}, std::istreambuf_iterator<char>{}, std::ostreambuf_iterator{out});
        if (!out) {
            eprintln("Failed to write network");
            return 1;
        }
        return 0;
    }

    auto network = std::make_unique<LoadedNetwork>();

    if (!in.read(reinterpret_cast<char*>(network.get()), sizeof(LoadedNetwork))) {
        eprintln("Failed to read network");
        return 1;
    }

    println("Permuting network");

    LayeredArch::permuteParams<i16>(network->ftWeights.psq);
    LayeredArch::permuteParams<i16>(network->ftBiases);

    if constexpr (InputFeatureSet::kThreatInputs) {
        LayeredArch::permuteParams<i8>(network->ftWeights.threat);
    }

    if (!out.write(reinterpret_cast<const char*>(network.get()), sizeof(LoadedNetwork))) {
        eprintln("Failed to write network");
        return 1;
    }

    return 0;
}
