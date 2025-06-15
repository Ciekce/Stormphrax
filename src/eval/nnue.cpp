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

#include "nnue.h"

#include <fstream>
#include <string_view>

#include "../util/memstream.h"
#include "nnue/io_impl.h"

#ifdef _MSC_VER
    #define SP_MSVC
    #pragma push_macro("_MSC_VER")
    #undef _MSC_VER
#endif

#define INCBIN_PREFIX g_
#include "../3rdparty/incbin.h"

#ifdef SP_MSVC
    #pragma pop_macro("_MSC_VER")
    #undef SP_MSVC
#endif

namespace {
    INCBIN(std::byte, defaultNet, SP_NETWORK_FILE);
}

namespace stormphrax::eval {
    namespace {
        SP_ENUM_FLAGS(u16, NetworkFlags){
            kNone = 0x0000,
            kZstdCompressed = 0x0001,
            kHorizontallyMirrored = 0x0002,
            kMergedKings = 0x0004,
            kPairwiseMul = 0x0008,
        };

        constexpr u16 kExpectedHeaderVersion = 1;

        struct __attribute__((packed)) NetworkHeader {
            std::array<char, 4> magic{};
            u16 version{};
            NetworkFlags flags{};
            [[maybe_unused]] u8 padding{};
            u8 arch{};
            u8 activation{};
            u16 hiddenSize{};
            u8 inputBuckets{};
            u8 outputBuckets{};
            u8 nameLen{};
            std::array<char, 48> name{};
        };

        static_assert(sizeof(NetworkHeader) == 64);

        inline std::string_view archName(u8 arch) {
            static constexpr auto kNetworkArchNames = std::array{
                "basic",
                "perspective",
                "perspective_multilayer",
                "perspective_multilayer_dual_act",
            };

            if (arch < kNetworkArchNames.size()) {
                return kNetworkArchNames[arch];
            }

            return "<unknown>";
        }

        inline std::string_view activationFuncName(u8 func) {
            static constexpr auto kActivationFunctionNames = std::array{"crelu", "screlu", "relu"};

            if (func < kActivationFunctionNames.size()) {
                return kActivationFunctionNames[func];
            }

            return "<unknown>";
        }

        //TODO better error messages
        bool validate(const NetworkHeader& header) {
            if (header.magic != std::array{'C', 'B', 'N', 'F'}) {
                println("invalid magic bytes in network header");
                return false;
            }

            if (header.version != kExpectedHeaderVersion) {
                eprintln(
                    "unsupported network format version {} (expected: {})",
                    header.version,
                    kExpectedHeaderVersion
                );
                return false;
            }

            if (header.arch != LayeredArch::kArchId) {
                eprintln(
                    "wrong network architecture {} (expected: {})",
                    archName(header.arch),
                    archName(LayeredArch::kArchId)
                );
                return false;
            }

            if (testFlags(header.flags, NetworkFlags::kHorizontallyMirrored) != InputFeatureSet::kIsMirrored) {
                if constexpr (InputFeatureSet::kIsMirrored) {
                    eprintln("unmirrored network, expected horizontally mirrored");
                } else {
                    eprintln("horizontally mirrored network, expected unmirrored");
                }

                return false;
            }

            if (testFlags(header.flags, NetworkFlags::kMergedKings) != InputFeatureSet::kMergedKings) {
                if constexpr (InputFeatureSet::kMergedKings) {
                    eprintln("network does not have merged king planes, expected merged");
                } else {
                    eprintln("network has merged king planes, expected unmerged");
                }

                return false;
            }

            if (testFlags(header.flags, NetworkFlags::kPairwiseMul) != LayeredArch::kPairwise) {
                if constexpr (LayeredArch::kPairwise) {
                    eprintln("network L1 does not require pairwise multiplication, expected paired");
                } else {
                    eprintln("network L1 requires pairwise multiplication, expected unpaired");
                }

                return false;
            }

            if (header.activation != L1Activation::kId) {
                eprintln(
                    "wrong l1 activation function {} (expected: {})",
                    activationFuncName(header.activation),
                    activationFuncName(L1Activation::kId)
                );
                return false;
            }

            if (header.hiddenSize != kL1Size) {
                eprintln("wrong number of hidden neurons {} (expected: {})", header.hiddenSize, kL1Size);
                return false;
            }

            if (header.inputBuckets != InputFeatureSet::kBucketCount) {
                eprintln(
                    "wrong number of input buckets {} (expected: {})",
                    header.inputBuckets,
                    InputFeatureSet::kBucketCount
                );
                return false;
            }

            if (header.outputBuckets != OutputBucketing::kBucketCount) {
                eprintln(
                    "wrong number of output buckets {} (expected: {})",
                    header.outputBuckets,
                    OutputBucketing::kBucketCount
                );
                return false;
            }

            return true;
        }

        bool loadNetworkFrom(Network& network, std::istream& stream, const NetworkHeader& header) {
            bool success;

            if (testFlags(header.flags, NetworkFlags::kZstdCompressed)) {
                nnue::ZstdParamStream paramStream{stream};
                success = network.readFrom(paramStream);
            } else {
                nnue::PaddedParamStream<64> paramStream{stream};
                success = network.readFrom(paramStream);
            }

            return success;
        }

        Network s_network{};
    } // namespace

    const Network& g_network = s_network;

    void loadDefaultNetwork() {
        if (g_defaultNetSize < sizeof(NetworkHeader)) {
            eprintln("Missing default network?");
            return;
        }

        const auto& header = *reinterpret_cast<const NetworkHeader*>(g_defaultNetData);

        if (!validate(header)) {
            eprintln("Failed to validate default network header");
            return;
        }

        const auto* begin = g_defaultNetData + sizeof(NetworkHeader);
        const auto* end = g_defaultNetData + g_defaultNetSize;

        util::MemoryIstream stream{{begin, end}};

        if (!loadNetworkFrom(s_network, stream, header)) {
            eprintln("Failed to load default network");
            return;
        }
    }

    void loadNetwork(const std::string& name) {
        std::ifstream stream{name, std::ios::binary};

        if (!stream) {
            eprintln("failed to open network file \"{}\"", name);
            return;
        }

        NetworkHeader header{};
        stream.read(reinterpret_cast<char*>(&header), sizeof(NetworkHeader));

        if (!stream) {
            eprintln("failed to read network file header");
            return;
        }

        if (!validate(header)) {
            return;
        }

        if (!loadNetworkFrom(s_network, stream, header)) {
            eprintln("failed to read network parameters");
            return;
        }

        const std::string_view netName{header.name.data(), header.nameLen};
        println("info string loaded network {}", netName);
    }

    std::string_view defaultNetworkName() {
        const auto& header = *reinterpret_cast<const NetworkHeader*>(g_defaultNetData);
        return {header.name.data(), header.nameLen};
    }
} // namespace stormphrax::eval
