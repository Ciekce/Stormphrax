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
	// current arch: (768x4->768)x2->1, ClippedReLU

	// perspective
	const auto ArchId = 1;

	using Activation = activation::SquaredClippedReLU<255>;

	constexpr u32 InputSize = 768;
	constexpr u32 Layer1Size = 768;

	// for larger hidden layers, operations are done in blocks of
	// 256 values - this allows GCC to unroll loops without dying
	// cheers @jhonnold for the tip
	static_assert(Layer1Size < 512 || (Layer1Size % 256) == 0);

	constexpr i32 Scale = 400;

	constexpr i32 Q = 255 * 64;

	// visually flipped upside down, a1 = 0
	constexpr auto InputBuckets = std::array {
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0
	};

	constexpr auto InputBucketCount = *std::max_element(InputBuckets.begin(), InputBuckets.end()) + 1;

	constexpr auto refreshRequired(Color c, Square prevKingSq, Square kingSq)
	{
		assert(c != Color::None);

		assert(prevKingSq != Square::None);
		assert(kingSq != Square::None);
		assert(prevKingSq != kingSq);

		if constexpr (InputBucketCount == 1)
			return false;
		else if constexpr (InputBucketCount == 64)
			return true;

		if (c == Color::Black)
		{
			prevKingSq = flipSquare(prevKingSq);
			kingSq = flipSquare(kingSq);
		}

		return InputBuckets[static_cast<i32>(prevKingSq)] != InputBuckets[static_cast<i32>(kingSq)];
	}

	struct Network
	{
		SP_SIMD_ALIGNAS std::array<i16, InputSize * InputBucketCount * Layer1Size> featureWeights;
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
		SP_SIMD_ALIGNAS std::array<std::array<i16, Layer1Size>, 2> accumulators;

		[[nodiscard]] inline auto forColor(Color c) -> auto &
		{
			assert(c != Color::None);
			return accumulators[static_cast<i32>(c)];
		}

		[[nodiscard]] inline auto black() const -> const auto & { return accumulators[0]; }
		[[nodiscard]] inline auto white() const -> const auto & { return accumulators[1]; }

		[[nodiscard]] inline auto black() -> auto & { return accumulators[0]; }
		[[nodiscard]] inline auto white() -> auto & { return accumulators[1]; }

		inline auto init(Color c, std::span<const i16, Layer1Size> biases)
		{
			assert(c != Color::None);
			std::copy(biases.begin(), biases.end(), forColor(c).begin());
		}

		inline auto initBoth(std::span<const i16, Layer1Size> biases)
		{
			std::copy(biases.begin(), biases.end(), black().begin());
			std::copy(biases.begin(), biases.end(), white().begin());
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

		inline auto reset(const PositionBoards &boards, Square blackKing, Square whiteKing)
		{
			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			m_accumulatorStack.clear();
			m_curr = &m_accumulatorStack.emplace_back();

			refreshAccumulator(*m_curr, Color::Black, boards, blackKing);
			refreshAccumulator(*m_curr, Color::White, boards, whiteKing);
		}

		inline auto refresh(Color c, const PositionBoards &boards, Square king)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(c != Color::None);
			assert(king != Square::None);

			refreshAccumulator(*m_curr, c, boards, king);
		}

		inline auto moveFeatureSingle(Color c, Piece piece, Square src, Square dst, Square king)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(c != Color::None);

			assert(piece != Piece::None);

			assert(src != Square::None);
			assert(dst != Square::None);
			assert(src != dst);

			assert(src != king);

			assert(king != Square::None);

			moveFeatureSingle(*m_curr, c, piece, src, dst, king);
		}

		inline auto activateFeatureSingle(Color c, Piece piece, Square sq, Square king)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			assert(sq != king);

			activateFeatureSingle(*m_curr, c, piece, sq, king);
		}

		inline auto deactivateFeatureSingle(Color c, Piece piece, Square sq, Square king)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			assert(sq != king);

			deactivateFeatureSingle(*m_curr, c, piece, sq, king);
		}

		inline auto moveFeature(Piece piece, Square src, Square dst, Square blackKing, Square whiteKing)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(piece != Piece::None);

			assert(src != Square::None);
			assert(dst != Square::None);
			assert(src != dst);

			assert(src != blackKing);
			assert(src != whiteKing);

			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			moveFeatureSingle(*m_curr, Color::Black, piece, src, dst, blackKing);
			moveFeatureSingle(*m_curr, Color::White, piece, src, dst, whiteKing);
		}

		inline auto activateFeature(Piece piece, Square sq, Square blackKing, Square whiteKing)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(piece != Piece::None);
			assert(sq != Square::None);

			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			activateFeatureSingle(*m_curr, Color::Black, piece, sq, blackKing);
			activateFeatureSingle(*m_curr, Color::White, piece, sq, whiteKing);
		}

		inline auto deactivateFeature(Piece piece, Square sq, Square blackKing, Square whiteKing)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(piece != Piece::None);
			assert(sq != Square::None);

			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			deactivateFeatureSingle(*m_curr, Color::Black, piece, sq, blackKing);
			deactivateFeatureSingle(*m_curr, Color::White, piece, sq, whiteKing);
		}

		[[nodiscard]] inline auto evaluate(Color stm) const
		{
			assert(m_curr == &m_accumulatorStack.back());
			assert(stm != Color::None);

			return evaluate(*m_curr, stm);
		}

		[[nodiscard]] static inline auto evaluateOnce(const PositionBoards &boards,
			Square blackKing, Square whiteKing, Color stm)
		{
			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			assert(stm != Color::None);

			Accumulator accumulator{};

			refreshAccumulator(accumulator, Color::Black, boards, blackKing);
			refreshAccumulator(accumulator, Color::White, boards, whiteKing);

			return evaluate(accumulator, stm);
		}

	private:
		std::vector<Accumulator> m_accumulatorStack{};
		Accumulator *m_curr{};

		static inline auto refreshAccumulator(Accumulator &accumulator, Color c,
			const PositionBoards &boards, Square king) -> void
		{
			assert(c != Color::None);
			assert(king != Square::None);

			accumulator.init(c, g_currNet->featureBiases);

			// loop through each coloured piece, and activate the features
			// corresponding to that piece on each of the squares it occurs on
			for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				auto board = boards.forPiece(piece);
				while (!board.empty())
				{
					const auto sq = board.popLowestSquare();
					activateFeatureSingle(accumulator, c, piece, sq, king);
				}
			}
		}

		static inline auto moveFeatureSingle(Accumulator &accumulator, Color c,
			Piece piece, Square src, Square dst, Square king) -> void
		{
			assert(c != Color::None);

			assert(piece != Piece::None);

			assert(src != Square::None);
			assert(dst != Square::None);
			assert(src != dst);

			assert(src != king);

			assert(king != Square::None);

			const auto srcIdx = featureIndex(c, piece, src, king);
			const auto dstIdx = featureIndex(c, piece, dst, king);

			subtractAndAddToAll(accumulator.forColor(c),
				g_currNet->featureWeights, srcIdx * Layer1Size, dstIdx * Layer1Size);
		}

		static inline auto activateFeatureSingle(Accumulator &accumulator,
			Color c, Piece piece, Square sq, Square king) -> void
		{
			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			const auto idx = featureIndex(c, piece, sq, king);

			addToAll(accumulator.forColor(c), g_currNet->featureWeights, idx * Layer1Size);
		}

		static inline auto deactivateFeatureSingle(Accumulator &accumulator,
			Color c, Piece piece, Square sq, Square king) -> void
		{
			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			const auto idx = featureIndex(c, piece, sq, king);

			subtractFromAll(accumulator.forColor(c), g_currNet->featureWeights, idx * Layer1Size);
		}

		[[nodiscard]] static inline auto evaluate(const Accumulator &accumulator, Color stm) -> i32
		{
			assert(stm != Color::None);

			const auto output = stm == Color::Black
				? forward(accumulator.black(), accumulator.white(), g_currNet->outputWeights)
				: forward(accumulator.white(), accumulator.black(), g_currNet->outputWeights);
			return (output + g_currNet->outputBias) * Scale / Q;
		}

		static inline auto subtractAndAddToAll(std::array<i16, Layer1Size> &input,
			std::span<const i16> delta, u32 subOffset, u32 addOffset) -> void
		{
			assert(subOffset + Layer1Size <= delta.size());
			assert(addOffset + Layer1Size <= delta.size());

			if constexpr(Layer1Size >= 512)
			{
				for (usize i = 0; i < Layer1Size; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						input[idx] += delta[addOffset + idx] - delta[subOffset + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < Layer1Size; ++i)
				{
					input[i] += delta[addOffset + i] - delta[subOffset + i];
				}
			}
		}

		static inline auto addToAll(std::array<i16, Layer1Size> &input,
			std::span<const i16> delta, u32 offset) -> void
		{
			assert(offset + Layer1Size <= delta.size());

			if constexpr(Layer1Size >= 512)
			{
				for (usize i = 0; i < Layer1Size; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						input[idx] += delta[offset + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < Layer1Size; ++i)
				{
					input[i] += delta[offset + i];
				}
			}
		}

		static inline auto subtractFromAll(std::array<i16, Layer1Size> &input,
			std::span<const i16> delta, u32 offset) -> void
		{
			assert(offset + Layer1Size <= delta.size());

			if constexpr(Layer1Size >= 512)
			{
				for (usize i = 0; i < Layer1Size; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						input[idx] -= delta[offset + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < Layer1Size; ++i)
				{
					input[i] -= delta[offset + i];
				}
			}
		}

		[[nodiscard]] static inline auto featureIndex(Color c, Piece piece, Square sq, Square king) -> u32
		{
			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			constexpr u32 ColorStride = 64 * 6;
			constexpr u32 PieceStride = 64;

			const auto type = static_cast<u32>(pieceType(piece));

			u32 color = pieceColor(piece) == Color::White ? 0 : 1;

			if (c == Color::Black)
			{
				sq = flipSquare(sq);
				king = flipSquare(king);
				color ^= 1;
			}

			const auto bucketOffset = InputBuckets[static_cast<i32>(king)] * InputSize;
			return bucketOffset + color * ColorStride + type * PieceStride + static_cast<u32>(sq);
		}

		[[nodiscard]] static inline auto forward(
			std::span<const i16, Layer1Size> us,
			std::span<const i16, Layer1Size> them,
			std::span<const i16, Layer1Size * 2> weights
		) -> i32
		{
			assert(us.data() != them.data());

			i32 sum = 0;

			if constexpr(Layer1Size >= 512)
			{
				for (usize i = 0; i < Layer1Size; i += 256)
				{
					for (usize j = 0; j < 256; ++j)
					{
						const auto idx = i + j;

						const auto activated = Activation::activate(us[idx]);
						sum += activated * weights[idx];
					}
				}

				for (usize i = 0; i < Layer1Size; i += 256)
				{
					for (usize j = 0; j < 256; ++j)
					{
						const auto idx = i + j;

						const auto activated = Activation::activate(them[idx]);
						sum += activated * weights[Layer1Size + idx];
					}
				}
			}
			else
			{
				for (usize i = 0; i < Layer1Size; ++i)
				{
					const auto activated = Activation::activate(us[i]);
					sum += activated * weights[i];
				}

				for (usize i = 0; i < Layer1Size; ++i)
				{
					const auto activated = Activation::activate(them[i]);
					sum += activated * weights[Layer1Size + i];
				}
			}

			return sum / Activation::NormalizationK;
		}
	};

	auto loadDefaultNetwork() -> void;
	auto loadNetwork(const std::string &name) -> void;

	[[nodiscard]] auto defaultNetworkName() -> std::string_view;
}
