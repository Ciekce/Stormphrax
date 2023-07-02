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

#include "../core.h"
#include "../position/boards.h"

namespace stormphrax::eval
{
	// current arch: (768->64)x2->1, ClippedReLU

	namespace activation
	{
		template <Score Min, Score Max>
		struct [[maybe_unused]] ClippedReLU
		{
			static constexpr auto activate(i16 x)
			{
				return std::clamp(static_cast<i32>(x), Min, Max);
			}

			static constexpr i32 NormalizationK = 1;
		};

		template <Score Min, Score Max>
		struct [[maybe_unused]] SquaredClippedReLU
		{
			static constexpr auto activate(i16 x)
			{
				const auto clipped = std::clamp(static_cast<i32>(x), Min, Max);
				return clipped * clipped;
			}

			static constexpr i32 NormalizationK = Max;
		};
	}

	using Activation = activation::ClippedReLU<0, 255>;

	constexpr u32 InputSize = 768;
	constexpr u32 Layer1Size = 64;

	constexpr Score Scale = 400;

	constexpr Score Q = 255 * 64;

	struct alignas(64) Network
	{
		std::array<i16, InputSize * Layer1Size> featureWeights;
		std::array<i16, Layer1Size> featureBias;
		std::array<i16, Layer1Size * 2> outputWeights;
		i16 outputBias;
	};

	extern const Network &g_net;

	// Perspective network - separate accumulators for
	// each side to allow the network to learn tempo
	// (this is why there are two sets of output weights)
	template <u32 LayerSize>
	struct alignas(64) Accumulator
	{
		std::array<i16, LayerSize> black;
		std::array<i16, LayerSize> white;

		inline auto init(std::span<const i16, LayerSize> bias)
		{
			std::copy(bias.begin(), bias.end(), black.begin());
			std::copy(bias.begin(), bias.end(), white.begin());
		}
	};

	class NnueState
	{
	public:
		explicit NnueState(const Network &net = g_net)
			: m_net{net}
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

			m_curr->init(m_net.featureBias);

			// loop through each coloured piece, and activate the features
			// corresponding to that piece on each of the squares it occurs on
			for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				auto board = boards.forPiece(piece);
				while (!board.empty())
				{
					const auto sq = board.popLowestSquare();
					activateFeature(piece, sq);
				}
			}
		}

		inline auto moveFeature(Piece piece, Square src, Square dst)
		{
			assert(m_curr == &m_accumulatorStack.back());

			const auto [blackSrc, whiteSrc] = featureIndices(piece, src);
			const auto [blackDst, whiteDst] = featureIndices(piece, dst);

			subtractAndAddToAll(m_curr->black, m_net.featureWeights, blackSrc * Layer1Size, blackDst * Layer1Size);
			subtractAndAddToAll(m_curr->white, m_net.featureWeights, whiteSrc * Layer1Size, whiteDst * Layer1Size);
		}

		inline auto activateFeature(Piece piece, Square sq) -> void
		{
			assert(m_curr == &m_accumulatorStack.back());

			const auto [blackIdx, whiteIdx] = featureIndices(piece, sq);

			addToAll(m_curr->black, m_net.featureWeights, blackIdx * Layer1Size);
			addToAll(m_curr->white, m_net.featureWeights, whiteIdx * Layer1Size);
		}

		inline auto deactivateFeature(Piece piece, Square sq) -> void
		{
			assert(m_curr == &m_accumulatorStack.back());

			const auto [blackIdx, whiteIdx] = featureIndices(piece, sq);

			subtractFromAll(m_curr->black, m_net.featureWeights, blackIdx * Layer1Size);
			subtractFromAll(m_curr->white, m_net.featureWeights, whiteIdx * Layer1Size);
		}

		[[nodiscard]] inline auto evaluate(Color stm) const
		{
			assert(m_curr == &m_accumulatorStack.back());

			const auto output = stm == Color::Black
				? activateAndFlatten(m_curr->black, m_curr->white, m_net.outputWeights)
				: activateAndFlatten(m_curr->white, m_curr->black, m_net.outputWeights);
			return (output + m_net.outputBias) * Scale / Q;
		}

	private:
		const Network &m_net;

		std::vector<Accumulator<Layer1Size>> m_accumulatorStack{};
		Accumulator<Layer1Size> *m_curr{};

		template <usize Size>
		static inline auto subtractAndAddToAll(std::array<i16, Size> &input,
			std::span<const i16> delta, u32 subOffset, u32 addOffset) -> void
		{
			for (u32 i = 0; i < Size; ++i)
			{
				input[i] += delta[addOffset + i] - delta[subOffset + i];
			}
		}

		template <usize Size>
		static inline auto addToAll(std::array<i16, Size> &input,
			std::span<const i16> delta, u32 offset) -> void
		{
			for (u32 i = 0; i < Size; ++i)
			{
				input[i] += delta[offset + i];
			}
		}

		template <usize Size>
		static inline auto subtractFromAll(std::array<i16, Size> &input,
			std::span<const i16> delta, u32 offset) -> void
		{
			for (u32 i = 0; i < Size; ++i)
			{
				input[i] -= delta[offset + i];
			}
		}

		static inline auto featureIndices(Piece piece, Square sq) -> std::pair<u32, u32>
		{
			constexpr u32 ColorStride = 64 * 6;
			constexpr u32 PieceStride = 64;

			const auto base = static_cast<u32>(basePiece(piece));
			const u32 color = pieceColor(piece) == Color::White ? 0 : 1;

			const auto blackIdx = !color * ColorStride + base * PieceStride + (static_cast<u32>(sq) ^ 0x38);
			const auto whiteIdx =  color * ColorStride + base * PieceStride +  static_cast<u32>(sq)        ;

			return {blackIdx, whiteIdx};
		}

		static inline auto activateAndFlatten(
			std::span<const i16, Layer1Size> us,
			std::span<const i16, Layer1Size> them,
			std::span<const i16, Layer1Size * 2> weights
		) -> Score
		{
			i32 sum = 0;

			for (u32 i = 0; i < Layer1Size; ++i)
			{
				sum += Activation::activate(  us[i]) * weights[             i];
				sum += Activation::activate(them[i]) * weights[Layer1Size + i];
			}

			return sum / Activation::NormalizationK;
		}
	};
}
