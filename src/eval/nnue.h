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

#pragma once

#include "../types.h"

#include <vector>

#include "arch.h"
#include "nnue/input.h"
#include "nnue/network.h"
#include "nnue/layers.h"
#include "nnue/activation.h"
#include "../util/static_vector.h"

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

	using Accumulator = FeatureTransformer::Accumulator;
	using RefreshTable = FeatureTransformer::RefreshTable;

	extern const Network &g_network;

	auto loadDefaultNetwork() -> void;
	auto loadNetwork(const std::string &name) -> void;

	[[nodiscard]] auto defaultNetworkName() -> std::string_view;

	struct NnueUpdates
	{
		using PieceSquare = std::pair<Piece, Square>;

		// [black, white]
		std::array<bool, 2> refresh{};

		StaticVector<PieceSquare, 2> sub{};
		StaticVector<PieceSquare, 2> add{};

		inline auto setRefresh(Color c)
		{
			refresh[static_cast<i32>(c)] = true;
		}

		inline auto pushSubAdd(Piece piece, Square src, Square dst)
		{
			sub.push({piece, src});
			add.push({piece, dst});
		}

		inline auto pushSub(Piece piece, Square square)
		{
			sub.push({piece, square});
		}

		inline auto pushAdd(Piece piece, Square square)
		{
			add.push({piece, square});
		}
	};

	class NnueState
	{
	public:
		NnueState()
		{
			m_accumulatorStack.resize(256);
		}

		inline auto reset(const BitboardSet &bbs, Square blackKing, Square whiteKing)
		{
			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			m_refreshTable.init(g_network.featureTransformer());

			m_curr = &m_accumulatorStack[0];

			for (const auto c : { Color::Black, Color::White })
			{
				const auto king = c == Color::Black ? blackKing : whiteKing;
				const auto bucket = InputFeatureSet::getBucket(c, king);

				auto &rtEntry = m_refreshTable.table[bucket];
				resetAccumulator(rtEntry.accumulator, c, bbs, king);

				m_curr->copyFrom(c, rtEntry.accumulator);
				rtEntry.colorBbs(c) = bbs;
			}
		}

		template <bool Push>
		inline auto update(const NnueUpdates &updates, const BitboardSet &bbs, Square blackKing, Square whiteKing)
		{
			assert(m_curr >= &m_accumulatorStack[0] && m_curr <= &m_accumulatorStack.back());
			assert(!updates.refresh[0] || !updates.refresh[1]);

			auto *next = Push ? m_curr + 1 : m_curr;

			assert(next <= &m_accumulatorStack.back());

			const auto subCount = updates.sub.size();
			const auto addCount = updates.add.size();

			for (const auto c : { Color::Black, Color::White })
			{
				const auto king = c == Color::Black ? blackKing : whiteKing;

				if (updates.refresh[static_cast<i32>(c)])
				{
					refreshAccumulator(*next, c, bbs, m_refreshTable, king);
					continue;
				}

				if (addCount == 1 && subCount == 1) // regular non-capture
				{
					const auto [subPiece, subSquare] = updates.sub[0];
					const auto [addPiece, addSquare] = updates.add[0];

					const auto sub = featureIndex(c, subPiece, subSquare, king);
					const auto add = featureIndex(c, addPiece, addSquare, king);

					next->subAddFrom(*m_curr, g_network.featureTransformer(), c, sub, add);
				}
				else if (addCount == 1 && subCount == 2) // any capture
				{
					const auto [subPiece0, subSquare0] = updates.sub[0];
					const auto [subPiece1, subSquare1] = updates.sub[1];
					const auto [addPiece , addSquare ] = updates.add[0];

					const auto sub0 = featureIndex(c, subPiece0, subSquare0, king);
					const auto sub1 = featureIndex(c, subPiece1, subSquare1, king);
					const auto add  = featureIndex(c, addPiece , addSquare , king);

					next->subSubAddFrom(*m_curr, g_network.featureTransformer(), c, sub0, sub1, add);
				}
				else if (addCount == 2 && subCount == 2) // castling
				{
					const auto [subPiece0, subSquare0] = updates.sub[0];
					const auto [subPiece1, subSquare1] = updates.sub[1];
					const auto [addPiece0, addSquare0] = updates.add[0];
					const auto [addPiece1, addSquare1] = updates.add[1];

					const auto sub0 = featureIndex(c, subPiece0, subSquare0, king);
					const auto sub1 = featureIndex(c, subPiece1, subSquare1, king);
					const auto add0 = featureIndex(c, addPiece0, addSquare0, king);
					const auto add1 = featureIndex(c, addPiece1, addSquare1, king);

					next->subSubAddAddFrom(*m_curr, g_network.featureTransformer(), c, sub0, sub1, add0, add1);
				}
				else assert(false && "Materialising a piece from nowhere?");
			}

			m_curr = next;
		}

		inline auto pop()
		{
			assert(m_curr > &m_accumulatorStack[0]);
			--m_curr;
		}

		[[nodiscard]] inline auto evaluate(const BitboardSet &bbs, Color stm) const
		{
			assert(m_curr >= &m_accumulatorStack[0] && m_curr <= &m_accumulatorStack.back());
			assert(stm != Color::None);

			return evaluate(*m_curr, bbs, stm);
		}

		[[nodiscard]] static inline auto evaluateOnce(const BitboardSet &bbs,
			Square blackKing, Square whiteKing, Color stm)
		{
			assert(blackKing != Square::None);
			assert(whiteKing != Square::None);
			assert(blackKing != whiteKing);

			assert(stm != Color::None);

			Accumulator accumulator{};

			accumulator.initBoth(g_network.featureTransformer());

			resetAccumulator(accumulator, Color::Black, bbs, blackKing);
			resetAccumulator(accumulator, Color::White, bbs, whiteKing);

			return evaluate(accumulator, bbs, stm);
		}

	private:
		std::vector<Accumulator> m_accumulatorStack{};
		Accumulator *m_curr{};

		RefreshTable m_refreshTable{};

		[[nodiscard]] static inline auto evaluate(const Accumulator &accumulator,
			const BitboardSet &bbs, Color stm) -> i32
		{
			assert(stm != Color::None);

			constexpr i32 Q = L1Q * OutputQ;

			const auto output = stm == Color::Black
				? g_network.propagate(bbs, accumulator.black(), accumulator.white())
				: g_network.propagate(bbs, accumulator.white(), accumulator.black());
			return output * Scale / Q;
		}

		static inline auto refreshAccumulator(Accumulator &accumulator, Color c,
			const BitboardSet &bbs, RefreshTable &refreshTable, Square king) -> void
		{
			const auto bucket = InputFeatureSet::getBucket(c, king);

			auto &rtEntry = refreshTable.table[bucket];
			auto &prevBoards = rtEntry.colorBbs(c);

			for (u32 pieceIdx = 0; pieceIdx < static_cast<u32>(Piece::None); ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				const auto prev = prevBoards.forPiece(piece);
				const auto curr =     bbs.forPiece(piece);

				auto   added = curr & ~prev;
				auto removed = prev & ~curr;

				while (added)
				{
					const auto sq = added.popLowestSquare();
					const auto feature = featureIndex(c, piece, sq, king);

					rtEntry.accumulator.activateFeature(g_network.featureTransformer(), c, feature);
				}

				while (removed)
				{
					const auto sq = removed.popLowestSquare();
					const auto feature = featureIndex(c, piece, sq, king);

					rtEntry.accumulator.deactivateFeature(g_network.featureTransformer(), c, feature);
				}
			}

			accumulator.copyFrom(c, rtEntry.accumulator);
			prevBoards = bbs;
		}

		static inline auto resetAccumulator(Accumulator &accumulator, Color c,
			const BitboardSet &bbs, Square king) -> void
		{
			assert(c != Color::None);
			assert(king != Square::None);

			// loop through each coloured piece, and activate the features
			// corresponding to that piece on each of the squares it occurs on
			for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				auto board = bbs.forPiece(piece);
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

			const u32 color = pieceColor(piece) == c ? 0 : 1;

			if (c == Color::Black)
				sq = flipSquare(sq);

			const auto bucketOffset = InputFeatureSet::getBucket(c, king) * InputSize;
			return bucketOffset + color * ColorStride + type * PieceStride + static_cast<u32>(sq);
		}
	};
}
