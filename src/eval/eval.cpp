/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

#include "eval.h"

#include <memory>
#include <fstream>

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

INCBIN(defaultNet, SP_NETWORK_FILE);

namespace stormphrax::eval
{
	namespace
	{
		inline auto archName(u8 arch)
		{
			static constexpr auto NetworkArchNames = std::array {
				"\"basic\"", "\"perspective\""
			};

			if (arch < NetworkArchNames.size())
				return NetworkArchNames[arch];

			return "<unknown>";
		}

		inline auto activationFuncName(u8 func)
		{
			static constexpr auto ActivationFunctionNames = std::array {
				"\"clipped ReLU\"", "\"squared clipped ReLU\""
			};

			if (func < ActivationFunctionNames.size())
				return ActivationFunctionNames[func];

			return "<unknown>";
		}

		constexpr u16 ExpectedHeaderVersion = 1;

		struct __attribute__((packed)) NetworkHeader
		{
			std::array<char, 4> magic{};
			u16 version{};
			[[maybe_unused]] u16 flags{};
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

		std::unique_ptr<Network> s_externalNetwork{};

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

			if (header.arch != ArchId)
			{
				std::cerr << "wrong network architecture " << archName(header.arch)
					<< " (expected: " << archName(ArchId) << ")" << std::endl;
				return false;
			}

			if (header.activation != Activation::Id)
			{
				std::cerr << "wrong network activation function " << activationFuncName(header.activation)
					<< " (expected: " << activationFuncName(Activation::Id) << ")" << std::endl;
				return false;
			}

			if (header.hiddenSize != Layer1Size)
			{
				std::cerr << "wrong number of hidden neurons " << header.hiddenSize
					<< " (expected: " << Layer1Size << ")" << std::endl;
				return false;
			}

			if (header.inputBuckets != 1)
			{
				std::cerr << "wrong number of input buckets " << header.inputBuckets
					<< " (expected: 1)" << std::endl;
				return false;
			}

			if (header.outputBuckets != 1)
			{
				std::cerr << "wrong number of output buckets " << header.outputBuckets
					<< " (expected: 1)" << std::endl;
				return false;
			}

			return true;
		}

		// *** this is NOT equal to sizeof(Network) due to padding at the end ***
		constexpr auto NetworkSize
			= sizeof(Network::featureWeights)
			+ sizeof(Network::featureBiases)
			+ sizeof(Network::outputWeights)
			+ sizeof(Network::outputBias);
	}

	const Network *g_currNet;

	auto loadDefaultNetwork() -> void
	{
		assert(validate(*reinterpret_cast<const NetworkHeader *>(g_defaultNetData)));

		s_externalNetwork.reset();
		g_currNet = reinterpret_cast<const Network *>(g_defaultNetData + sizeof(NetworkHeader));
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

		{
			auto newNet = std::make_unique<Network>();

			// paranoia
			assert((reinterpret_cast<std::uintptr_t>(newNet.get()) % SP_NETWORK_ALIGNMENT) == 0);

			stream.read(reinterpret_cast<char *>(newNet.get()), NetworkSize);

			if (!stream)
			{
				std::cerr << "failed to read network parameters" << std::endl;
				return;
			}

			std::swap(s_externalNetwork, newNet);
		}

		g_currNet = s_externalNetwork.get();

		const std::string_view netName{header.name.data(), header.nameLen};
		std::cout << "info string loaded network " << netName << std::endl;
	}

	auto defaultNetworkName() -> std::string_view
	{
		const auto &header = *reinterpret_cast<const NetworkHeader *>(g_defaultNetData);
		return {header.name.data(), header.nameLen};
	}
}
