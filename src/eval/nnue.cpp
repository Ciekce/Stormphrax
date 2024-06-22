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

#include "nnue.h"

#include <fstream>

#include "../util/memstream.h"
#include "nnue/io.h"

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

namespace
{
	INCBIN(std::byte, defaultNet, SP_NETWORK_FILE);
}

namespace stormphrax::eval
{
	namespace
	{
		SP_ENUM_FLAGS(u16, NetworkFlags)
		{
			None = 0x0000,
			HorizontallyMirrored = 0x0002,
		};

		constexpr u16 ExpectedHeaderVersion = 1;

		struct __attribute__((packed)) NetworkHeader
		{
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

		inline auto archName(u8 arch)
		{
			static constexpr auto NetworkArchNames = std::array {
				"basic", "perspective"
			};

			if (arch < NetworkArchNames.size())
				return NetworkArchNames[arch];

			return "<unknown>";
		}

		inline auto activationFuncName(u8 func)
		{
			static constexpr auto ActivationFunctionNames = std::array {
				"crelu", "screlu", "relu"
			};

			if (func < ActivationFunctionNames.size())
				return ActivationFunctionNames[func];

			return "<unknown>";
		}

		//TODO better error messages
		auto validate(const NetworkHeader &header)
		{
			if (header.magic != std::array{'C', 'B', 'N', 'F'})
			{
				std::cerr << "invalid magic bytes in network header" << std::endl;
				return false;
			}

			if (header.version != ExpectedHeaderVersion)
			{
				std::cerr << "unsupported network format version " << header.version
					<< " (expected: " << ExpectedHeaderVersion << ")" << std::endl;
				return false;
			}

			if (header.arch != 1 /* perspective */)
			{
				std::cerr << "wrong network architecture " << archName(header.arch)
					<< " (expected: " << archName(1) << ")" << std::endl;
				return false;
			}

			if (testFlags(header.flags, NetworkFlags::HorizontallyMirrored) != InputFeatureSet::IsMirrored)
			{
				if constexpr (InputFeatureSet::IsMirrored)
					std::cerr << "unmirrored network, expected horizontally mirrored" << std::endl;
				else std::cerr << "horizontally mirrored network, expected unmirrored" << std::endl;

				return false;
			}

			if (header.activation != L1Activation::Id)
			{
				std::cerr << "wrong network l1 activation function (" << activationFuncName(header.activation)
					<< ", expected: " << activationFuncName(L1Activation::Id) << ")" << std::endl;
				return false;
			}

			if (header.hiddenSize != L1Size)
			{
				std::cerr << "wrong number of hidden neurons (" << header.hiddenSize
					<< ", expected: " << L1Size << ")" << std::endl;
				return false;
			}

			if (header.inputBuckets != InputFeatureSet::BucketCount)
			{
				std::cerr << "wrong number of input buckets (" << static_cast<u32>(header.inputBuckets)
					<< ", expected: " << InputFeatureSet::BucketCount << ")" << std::endl;
				return false;
			}

			if (header.outputBuckets != OutputBucketing::BucketCount)
			{
				std::cerr << "wrong number of output buckets (" << static_cast<u32>(header.outputBuckets)
					<< ", expected: " << OutputBucketing::BucketCount << ")" << std::endl;
				return false;
			}

			return true;
		}

		Network s_network{};
	}

	const Network &g_network = s_network;

	auto loadDefaultNetwork() -> void
	{
		const auto *begin = g_defaultNetData + sizeof(NetworkHeader);
		const auto *end = g_defaultNetData + g_defaultNetSize;

		util::MemoryIstream stream{{begin, end}};
		nnue::PaddedParamStream<64> paramStream{stream};

		s_network.readFrom(paramStream);
	}

	auto loadNetwork(const std::string &name) -> void
	{
		std::ifstream stream{name, std::ios::binary};

		if (!stream)
		{
			std::cerr << "failed to open network file \"" << name << "\"" << std::endl;
			return;
		}

		NetworkHeader header{};
		stream.read(reinterpret_cast<char *>(&header), sizeof(NetworkHeader));

		if (!stream)
		{
			std::cerr << "failed to read network file header" << std::endl;
			return;
		}

		if (!validate(header))
			return;

		nnue::PaddedParamStream<64> paramStream{stream};
		if (!s_network.readFrom(paramStream))
		{
			std::cerr << "failed to read network parameters" << std::endl;
			return;
		}

		const std::string_view netName{header.name.data(), header.nameLen};
		std::cout << "info string loaded network " << netName << std::endl;
	}

	auto defaultNetworkName() -> std::string_view
	{
		const auto &header = *reinterpret_cast<const NetworkHeader *>(g_defaultNetData);
		return {header.name.data(), header.nameLen};
	}
}
