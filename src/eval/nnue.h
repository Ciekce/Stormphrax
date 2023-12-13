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

#include <vector>

#include "arch.h"
#include "nnue/input.h"
#include "nnue/network.h"
#include "nnue/layers.h"
#include "nnue/activation.h"

namespace stormphrax::eval
{
	using FeatureTransformer = nnue::FeatureTransformer<
		i16, 768, Layer1Size,
		*std::ranges::max_element(InputBuckets) + 1
	>;

	using Network = nnue::PerspectiveNetwork<
		FeatureTransformer,
		nnue::DensePerspectiveAffineLayer<
			i16, i16,
			L1Activation,
			Layer1Size, 1,
			OutputBucketing
		>
	>;

	extern const Network &g_network;

	auto loadDefaultNetwork() -> void;
	auto loadNetwork(const std::string &name) -> void;

	[[nodiscard]] auto defaultNetworkName() -> std::string_view;

	class NnueState
	{
	public:
		NnueState()
		{
			m_accumulatorStack.reserve(256);
		}

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

			const auto srcFeature = featureIndex(c, piece, src, king);
			const auto dstFeature = featureIndex(c, piece, dst, king);

			m_curr->moveFeature(g_network.featureTransformer(), c, srcFeature, dstFeature);
		}

		inline auto activateFeatureSingle(Color c, Piece piece, Square sq, Square king)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			assert(sq != king);

			const auto feature = featureIndex(c, piece, sq, king);
			m_curr->activateFeature(g_network.featureTransformer(), c, feature);
		}

		inline auto deactivateFeatureSingle(Color c, Piece piece, Square sq, Square king)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			const auto feature = featureIndex(c, piece, sq, king);
			m_curr->deactivateFeature(g_network.featureTransformer(), c, feature);
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

			moveFeatureSingle(Color::Black, piece, src, dst, blackKing);
			moveFeatureSingle(Color::White, piece, src, dst, whiteKing);
		}

		inline auto activateFeature(Piece piece, Square sq, Square blackKing, Square whiteKing)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(piece != Piece::None);
			assert(sq != Square::None);

			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			activateFeatureSingle(Color::Black, piece, sq, blackKing);
			activateFeatureSingle(Color::White, piece, sq, whiteKing);
		}

		inline auto deactivateFeature(Piece piece, Square sq, Square blackKing, Square whiteKing)
		{
			assert(m_curr == &m_accumulatorStack.back());

			assert(piece != Piece::None);
			assert(sq != Square::None);

			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			deactivateFeatureSingle(Color::Black, piece, sq, blackKing);
			deactivateFeatureSingle(Color::White, piece, sq, whiteKing);
		}

		[[nodiscard]] inline auto evaluate(const PositionBoards &boards, Color stm) const
		{
			assert(m_curr == &m_accumulatorStack.back());
			assert(stm != Color::None);

			return evaluate(*m_curr, boards, stm);
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

			return evaluate(accumulator, boards, stm);
		}

	private:
		using Accumulator = nnue::Accumulator<FeatureTransformer>;

		std::vector<Accumulator> m_accumulatorStack{};
		Accumulator *m_curr{};

		[[nodiscard]] static inline auto evaluate(const Accumulator &accumulator,
			const PositionBoards &boards, Color stm) -> i32
		{
			assert(stm != Color::None);

			constexpr i32 Q = L1Q * OutputQ;

			const auto output = stm == Color::Black
				? g_network.propagate(boards, accumulator.black(), accumulator.white())
				: g_network.propagate(boards, accumulator.white(), accumulator.black());
			return output * Scale / Q;
		}

		static inline auto refreshAccumulator(Accumulator &accumulator, Color c,
			const PositionBoards &boards, Square king) -> void
		{
			assert(c != Color::None);
			assert(king != Square::None);

			accumulator.init(g_network.featureTransformer(), c);

			// loop through each coloured piece, and activate the features
			// corresponding to that piece on each of the squares it occurs on
			for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				auto board = boards.forPiece(piece);
				while (!board.empty())
				{
					const auto sq = board.popLowestSquare();

					const auto feature = featureIndex(c, piece, sq, king);
					accumulator.activateFeature(g_network.featureTransformer(), c, feature);
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
	};

	constexpr auto refreshRequired(Color c, Square prevKingSq, Square kingSq)
	{
		assert(c != Color::None);

		assert(prevKingSq != Square::None);
		assert(kingSq != Square::None);
		assert(prevKingSq != kingSq);

		if constexpr (FeatureTransformer::InputBucketCount == 1)
			return false;
		else if constexpr (FeatureTransformer::InputBucketCount == 64)
			return true;

		if (c == Color::Black)
		{
			prevKingSq = flipSquare(prevKingSq);
			kingSq = flipSquare(kingSq);
		}

		return InputBuckets[static_cast<i32>(prevKingSq)] != InputBuckets[static_cast<i32>(kingSq)];
	}
}
