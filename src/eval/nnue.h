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

#pragma once

#include "../types.h"

#include <array>
#include <vector>
#include <utility>
#include <span>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>

#include "activation.h"
#include "../core.h"
#include "../util/simd.h"
#include "../position/boards.h"

namespace stormphrax::eval
{
	// current arch: (768->384)x2->1, ClippedReLU

	// perspective
	const auto ArchId = 1;

	using Activation = activation::SquaredClippedReLU<255>;

	constexpr u32 InputSize = 768;
	constexpr u32 Layer1Size = 768;

	constexpr Score Scale = 400;

	constexpr Score Q = 255 * 64;

	struct Network
	{
		SP_SIMD_ALIGNAS std::array<i16, InputSize * Layer1Size> featureWeights;
		SP_SIMD_ALIGNAS std::array<i16, Layer1Size> featureBiases;
		SP_SIMD_ALIGNAS std::array<i16, Layer1Size * 2> outputWeights;
		i16 outputBias;
	};

	extern const Network *g_currNet;

	// Perspective network - separate accumulators for
	// each side to allow the network to learn tempo
	// (this is why there are two sets of output weights)
	struct Accumulator
	{
		SP_SIMD_ALIGNAS std::array<i16, Layer1Size> black;
		SP_SIMD_ALIGNAS std::array<i16, Layer1Size> white;

		inline auto init(std::span<const i16, Layer1Size> bias)
		{
			std::copy(bias.begin(), bias.end(), black.begin());
			std::copy(bias.begin(), bias.end(), white.begin());
		}
	};

	class NnueState
	{
	public:
		NnueState()
		{
			m_accumulatorStack.reserve(256);
		}

		~NnueState() = default;

		inline auto push()
		{
			assert(m_curr == &m_accumulatorStack.back());

			m_accumulatorStack.push_back(*m_curr);
			m_curr = &m_accumulatorStack.back();
		}

		inline auto pop()
		{
			m_accumulatorStack.pop_back();
			m_curr = &m_accumulatorStack.back();
		}

		inline auto reset(const PositionBoards &boards)
		{
			m_accumulatorStack.clear();
			m_curr = &m_accumulatorStack.emplace_back();

			initAccumulator(*m_curr, boards);
		}

		inline auto moveFeature(Piece piece, Square src, Square dst)
		{
			assert(m_curr == &m_accumulatorStack.back());
			moveFeature(*m_curr, piece, src, dst);
		}

		inline auto activateFeature(Piece piece, Square sq)
		{
			assert(m_curr == &m_accumulatorStack.back());
			activateFeature(*m_curr, piece, sq);
		}

		inline auto deactivateFeature(Piece piece, Square sq)
		{
			assert(m_curr == &m_accumulatorStack.back());
			deactivateFeature(*m_curr, piece, sq);
		}

		[[nodiscard]] inline auto evaluate(Color stm) const
		{
			assert(m_curr == &m_accumulatorStack.back());
			return evaluate(*m_curr, stm);
		}

		[[nodiscard]] static inline auto evaluateOnce(const PositionBoards &boards, Color stm)
		{
			Accumulator accumulator{};
			initAccumulator(accumulator, boards);

			return evaluate(accumulator, stm);
		}

	private:
		std::vector<Accumulator> m_accumulatorStack{};
		Accumulator *m_curr{};

		static inline auto initAccumulator(Accumulator &accumulator, const PositionBoards &boards) -> void
		{
			accumulator.init(g_currNet->featureBiases);

			// loop through each coloured piece, and activate the features
			// corresponding to that piece on each of the squares it occurs on
			for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				auto board = boards.forPiece(piece);
				while (!board.empty())
				{
					const auto sq = board.popLowestSquare();
					activateFeature(accumulator, piece, sq);
				}
			}
		}

		static inline auto moveFeature(Accumulator &accumulator, Piece piece, Square src, Square dst) -> void
		{
			const auto [blackSrc, whiteSrc] = featureIndices(piece, src);
			const auto [blackDst, whiteDst] = featureIndices(piece, dst);

			subtractAndAddToAll(accumulator.black, g_currNet->featureWeights, blackSrc * Layer1Size, blackDst * Layer1Size);
			subtractAndAddToAll(accumulator.white, g_currNet->featureWeights, whiteSrc * Layer1Size, whiteDst * Layer1Size);
		}

		static inline auto activateFeature(Accumulator &accumulator, Piece piece, Square sq) -> void
		{
			const auto [blackIdx, whiteIdx] = featureIndices(piece, sq);

			addToAll(accumulator.black, g_currNet->featureWeights, blackIdx * Layer1Size);
			addToAll(accumulator.white, g_currNet->featureWeights, whiteIdx * Layer1Size);
		}

		static inline auto deactivateFeature(Accumulator &accumulator, Piece piece, Square sq) -> void
		{
			const auto [blackIdx, whiteIdx] = featureIndices(piece, sq);

			subtractFromAll(accumulator.black, g_currNet->featureWeights, blackIdx * Layer1Size);
			subtractFromAll(accumulator.white, g_currNet->featureWeights, whiteIdx * Layer1Size);
		}

		[[nodiscard]] static inline auto evaluate(const Accumulator &accumulator, Color stm) -> i32
		{
			const auto output = stm == Color::Black
				? forward(accumulator.black, accumulator.white, g_currNet->outputWeights)
				: forward(accumulator.white, accumulator.black, g_currNet->outputWeights);
			return (output + g_currNet->outputBias) * Scale / Q;
		}

		template <usize Size>
		static inline auto subtractAndAddToAll(std::array<i16, Size> &input,
			std::span<const i16> delta, u32 subOffset, u32 addOffset) -> void
		{
			assert(subOffset + Size <= delta.size());
			assert(addOffset + Size <= delta.size());

			for (u32 i = 0; i < Size; ++i)
			{
				input[i] += delta[addOffset + i] - delta[subOffset + i];
			}
		}

		template <usize Size>
		static inline auto addToAll(std::array<i16, Size> &input,
			std::span<const i16> delta, u32 offset) -> void
		{
			assert(offset + Size <= delta.size());

			for (u32 i = 0; i < Size; ++i)
			{
				input[i] += delta[offset + i];
			}
		}

		template <usize Size>
		static inline auto subtractFromAll(std::array<i16, Size> &input,
			std::span<const i16> delta, u32 offset) -> void
		{
			assert(offset + Size <= delta.size());

			for (u32 i = 0; i < Size; ++i)
			{
				input[i] -= delta[offset + i];
			}
		}

		[[nodiscard]] static inline auto featureIndices(Piece piece, Square sq) -> std::pair<u32, u32>
		{
			assert(piece != Piece::None);
			assert(sq != Square::None);

			constexpr u32 ColorStride = 64 * 6;
			constexpr u32 PieceStride = 64;

			const auto base = static_cast<u32>(basePiece(piece));
			const u32 color = pieceColor(piece) == Color::White ? 0 : 1;

			const auto blackIdx = !color * ColorStride + base * PieceStride + (static_cast<u32>(sq) ^ 0x38);
			const auto whiteIdx =  color * ColorStride + base * PieceStride +  static_cast<u32>(sq)        ;

			return {blackIdx, whiteIdx};
		}

		[[nodiscard]] static inline auto forward(
			std::span<const i16, Layer1Size> us,
			std::span<const i16, Layer1Size> them,
			std::span<const i16, Layer1Size * 2> weights
		) -> Score
		{
			constexpr auto Step = util::Simd::RegisterSize / sizeof(i16);

			auto sum = util::Simd::zero();

			// stm's accumulator
			for (usize i = 0; i < Layer1Size; i += Step)
			{
				// load the input of these hidden neurons
				auto v = util::Simd::load(&us[i]);
				// apply the activation function
				v = Activation::activate(v);

				// load the weights for these neurons
				const auto weight = util::Simd::load(&weights[i]);
				// multiply the inputs by the weights and sum each adjacent
				// pair of 16 bit integers into half as many 32 bit integers
				v = util::Simd::mulAddAdj16(v, weight);

				// add the sums to the totals
				sum = util::Simd::add32(sum, v);
			}

			// opponent's accumulator
			// same as above, but for the other accumulator
			for (usize i = 0; i < Layer1Size; i += Step)
			{
				auto v = util::Simd::load(&them[i]);
				v = Activation::activate(v);

				const auto weight = util::Simd::load(&weights[Layer1Size + i]);
				v = util::Simd::mulAddAdj16(v, weight);

				sum = util::Simd::add32(sum, v);
			}

			// sum all the values in the total and renormalise if screlu
			return util::Simd::sum32(sum) / Activation::NormalizationK;
		}
	};

	auto loadDefaultNetwork() -> void;
	auto loadNetwork(const std::string &name) -> void;

	[[nodiscard]] auto defaultNetworkName() -> std::string_view;
}
