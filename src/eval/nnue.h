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
#include "nnue/accumulator.h"
#include "nnue/network.h"
#include "nnue/layers.h"
#include "nnue/activation.h"

namespace stormphrax::eval
{
	using FeatureTransformer = nnue::FeatureTransformer<
		i16, InputSize, Layer1Size, InputFeatureSet
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
			m_accumulatorStack.resize(256);
		}

		inline auto push(ExtendedMove move)
		{
			assert(m_curr == &m_accumulatorStack[m_idx]);

			m_curr->move() = move;

			m_curr = &m_accumulatorStack[++m_idx];
			m_curr->setDirty();
		}

		inline auto pop()
		{
			assert(m_idx > 0);
			m_curr = &m_accumulatorStack[--m_idx];
		}

		inline auto reset(const PositionBoards &boards, Square blackKing, Square whiteKing)
		{
			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			m_idx = 0;
			m_curr = &m_accumulatorStack[m_idx];

			resetAccumulator(*m_curr, boards, blackKing, whiteKing);
		}

		[[nodiscard]] inline auto evaluate(const PositionBoards &boards, Square blackKing, Square whiteKing, Color stm)
		{
			assert(m_curr == &m_accumulatorStack[m_idx]);
			assert(stm != Color::None);

			for (const auto c : { Color::Black, Color::White })
			{
				if (m_curr->dirty(c))
				{
					const auto king = c == Color::Black ? blackKing : whiteKing;
					if (canUpdate(c))
						updateAccumulators(c, king);
					else resetAccumulator(*m_curr, c, boards, king);
				}
			}

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
			resetAccumulator(accumulator, boards, blackKing, whiteKing);

			return evaluate(accumulator, boards, stm);
		}

	private:
		using Accumulator = nnue::Accumulator<FeatureTransformer>;

		std::vector<Accumulator> m_accumulatorStack{};
		i32 m_idx{};

		Accumulator *m_curr{};

		static inline auto resetAccumulator(Accumulator &accumulator,
			Color c, const PositionBoards &boards, Square king) -> void
		{
			assert(c != Color::None);
			assert(king != Square::None);

			accumulator.init(c, g_network.featureTransformer());

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

			accumulator.dirty(c) = false;
		}

		static inline auto resetAccumulator(Accumulator &accumulator,
			const PositionBoards &boards, Square blackKing, Square whiteKing) -> void
		{
			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);

			for (const auto c : { Color::Black, Color::White })
			{
				const auto king = c == Color::Black ? blackKing : whiteKing;
				resetAccumulator(accumulator, c, boards, king);
			}
		}

		inline auto updateAccumulator(Color c, i32 idx, Square king)
		{
			assert(c != Color::None);
			assert(idx > 0 && idx <= m_idx);
			assert(king != Square::None);

			const auto &ft = g_network.featureTransformer();

			const auto &prev = m_accumulatorStack[idx - 1];
			const auto &move = prev.move();

			assert(move.valid());

			auto &accumulator = m_accumulatorStack[idx];
			accumulator.copyFrom(prev);

			const auto srcFeature = featureIndex(c, move.moving, move.src, king);

			if (move.type == MoveType::Castling)
			{
				const auto rank = squareRank(move.src);

				const auto kingDstFeature = featureIndex(c, move.moving, move.kingDst, king);
				if (srcFeature != kingDstFeature) // frc
					accumulator.moveFeature(ft, c, srcFeature, kingDstFeature);

				const auto rook = copyPieceColor(move.moving, PieceType::Rook);
				const auto rookDst = squareFile(move.src) < squareFile(move.dst)
					? toSquare(rank, 5)
					: toSquare(rank, 3);

				const auto rookSrcFeature = featureIndex(c, rook, move.dst, king);
				const auto rookDstFeature = featureIndex(c, rook, rookDst, king);

				if (rookSrcFeature != rookDstFeature) // frc
					accumulator.moveFeature(ft, c, rookSrcFeature, rookDstFeature);
			}
			else
			{
				auto dstPiece = move.moving;
				auto captureSquare = move.dst;

				if (move.type == MoveType::Promotion)
					dstPiece = copyPieceColor(move.moving, move.promo);
				else if (move.type == MoveType::EnPassant)
					captureSquare = relativeSquare(oppColor(pieceColor(move.moving)),
						toSquare(2, squareFile(move.dst)));

				const auto dstFeature = featureIndex(c, dstPiece, move.dst, king);
				accumulator.moveFeature(ft, c, srcFeature, dstFeature);

				if (move.captured != Piece::None)
				{
					const auto capturedFeature = featureIndex(c, move.captured, captureSquare, king);
					accumulator.deactivateFeature(ft, c, capturedFeature);
				}
			}

			accumulator.dirty(c) = false;
		}

		inline auto updateAccumulators(Color c, Square king) -> void
		{
			assert(m_curr == &m_accumulatorStack[m_idx]);
			assert(c != Color::None);

			// Scan backwards to find a clean accumulator we can update from
			// There must be one available before we hit a king bucket change
			auto idx = m_idx;
			for (; idx >= 1 && m_accumulatorStack[idx - 1].dirty(c); --idx) {}

			// root is always clean
			assert(idx >= 1);

			// Update all accumulators up to the current one
			for (; idx <= m_idx; ++idx)
			{
				updateAccumulator(c, idx, king);
			}
		}

		[[nodiscard]] inline auto canUpdate(Color c) const -> bool
		{
			assert(m_curr == &m_accumulatorStack[m_idx]);
			assert(c != Color::None);

			// Scan backwards to find a clean accumulator we can update from,
			// or a friendly king move that requires a refresh of this perspective
			for (auto i = m_idx - 1; i >= 0; --i)
			{
				const auto &curr = m_accumulatorStack[i];

				const auto piece = curr.move().moving;
				const auto src = relativeSquare(c, curr.move().src);
				const auto dst = relativeSquare(c, curr.move().kingDst);

				// found a move that requires a refresh
				if (pieceColor(piece) == c && InputFeatureSet::refreshRequired(c, pieceType(piece), src, dst))
					return false;
				// found a clean accumulator!
				if (!curr.dirty(c))
					return true;
			}

			// root is always clean
			assert(false);
			__builtin_unreachable();
		}

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

		[[nodiscard]] static inline auto featureIndex(Color c, Piece piece, Square sq, Square king) -> u32
		{
			assert(c != Color::None);
			assert(piece != Piece::None);
			assert(sq != Square::None);
			assert(king != Square::None);

			constexpr u32 ColorStride = 64 * 6;
			constexpr u32 PieceStride = 64;

			const auto type = static_cast<u32>(pieceType(piece));

			const u32 color = pieceColor(piece) == c ? 0 : 1;

			if (c == Color::Black)
				sq = flipSquare(sq);

			const auto bucketOffset = InputFeatureSet::getBucket(c, king) * InputSize;
			return bucketOffset + color * ColorStride + type * PieceStride + static_cast<u32>(sq);
		}
	};
}
