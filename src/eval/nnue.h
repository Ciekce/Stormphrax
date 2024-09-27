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
#include "nnue/layers/dense_affine.h"
#include "nnue/layers/scale.h"
#include "nnue/layers/dequantize.h"
#include "nnue/activation.h"
#include "../util/static_vector.h"

namespace stormphrax::eval
{
	using FeatureTransformer = nnue::FeatureTransformer<
		i16, L1Size, InputFeatureSet
	>;

	using Network = nnue::PerspectiveNetwork<
		FeatureTransformer,
		nnue::layers::DensePerspectiveAffine<PairwiseMul,
			i16, i16,
			L1Activation,
			L1Size, 1, L1Q,
			OutputBucketing
		>,
		nnue::layers::Scale<i32, 1, Scale>,
		nnue::layers::Dequantize<i32, i32, 1, L1Q * OutputQ>
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

		[[nodiscard]] inline auto requiresRefresh(Color c) const
		{
			return refresh[static_cast<i32>(c)];
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

	struct UpdateContext
	{
		NnueUpdates updates{};
		BitboardSet bbs{};
		KingPair kings{};
	};

	class NnueState
	{
	private:
		struct UpdatableAccumulator
		{
			Accumulator acc{};
			UpdateContext ctx{};
			std::array<bool, 2> dirty{};

			inline auto setDirty() -> void
			{
				dirty[0] = dirty[1] = true;
			}

			inline auto setUpdated(Color c) -> void
			{
				assert(c != Color::None);
				dirty[static_cast<i32>(c)] = false;
			}

			[[nodiscard]] inline auto isDirty(Color c) -> bool
			{
				assert(c != Color::None);
				return dirty[static_cast<i32>(c)];
			}
		};

	public:
		NnueState()
		{
			m_accumulatorStack.resize(256);
		}

		inline auto reset(const BitboardSet &bbs, KingPair kings)
		{
			assert(kings.isValid());

			m_refreshTable.init(g_network.featureTransformer());

			m_curr = &m_accumulatorStack[0];

			for (const auto c : { Color::Black, Color::White })
			{
				const auto king = kings.color(c);
				const auto entry = InputFeatureSet::getRefreshTableEntry(c, king);

				auto &rtEntry = m_refreshTable.table[entry];
				resetAccumulator(rtEntry.accumulator, c, bbs, king);

				m_curr->acc.copyFrom(c, rtEntry.accumulator);
				rtEntry.colorBbs(c) = bbs;
			}
		}

		template <bool ApplyImmediately>
		inline auto pushUpdates(const NnueUpdates &updates, const BitboardSet &bbs, KingPair kings)
		{
			if constexpr (ApplyImmediately)
			{
				m_curr->ctx = {updates, bbs, kings};
				updateBoth(m_curr->acc, *m_curr, m_curr->ctx);
			}
			else
			{
				++m_curr;

				m_curr->ctx = {updates, bbs, kings};
				m_curr->setDirty();
			}
		}

		inline auto updateCurrent(const BitboardSet &realAndTrue)
		{
			const auto bbs = m_curr->ctx.bbs;

			assert(bbs == realAndTrue);

			for (const auto c : { Color::Black, Color::White })
			{
				if (!m_curr->isDirty(c))
					continue;

				const auto king = m_curr->ctx.kings.color(c);

				if (m_curr->ctx.updates.requiresRefresh(c))
				{
					refreshAccumulator(*m_curr, c, bbs, m_refreshTable, king);
					continue;
				}

				const auto *prev = findUsableAccumulator(c);

				const auto prevBbs = prev->ctx.bbs;
				const auto prevKing = prev->ctx.kings.color(c);

				if (InputFeatureSet::refreshRequired(c, prevKing, king))
				{
					refreshAccumulator(*m_curr, c, bbs, m_refreshTable, king);
					continue;
				}

				const auto rtIdx = InputFeatureSet::getRefreshTableEntry(c, king);
				auto &rtEntry = m_refreshTable.table[rtIdx];

				const auto updateCost = calcUpdateCost(prevBbs, bbs);
				const auto refreshCost = calcUpdateCost(rtEntry.colorBbs(c), bbs);

				if (updateCost <= refreshCost)
					diffAccumulator(m_curr->acc, c, prevBbs, bbs, king);
				else refreshAccumulator(*m_curr, c, bbs, m_refreshTable, king);
			}
		}

		inline auto pop()
		{
			assert(m_curr > &m_accumulatorStack[0]);
			--m_curr;
		}

		[[nodiscard]] inline auto evaluate(const BitboardSet &bbs, KingPair kings, Color stm)
		{
			assert(m_curr >= &m_accumulatorStack[0] && m_curr <= &m_accumulatorStack.back());
			assert(stm != Color::None);

			ensureUpToDate(bbs, kings);

			return evaluate(m_curr->acc, bbs, stm);
		}

		[[nodiscard]] static inline auto evaluateOnce(const BitboardSet &bbs, KingPair kings, Color stm)
		{
			assert(kings.isValid());
			assert(stm != Color::None);

			Accumulator accumulator{};

			accumulator.initBoth(g_network.featureTransformer());

			resetAccumulator(accumulator, Color::Black, bbs, kings.black());
			resetAccumulator(accumulator, Color::White, bbs, kings.white());

			return evaluate(accumulator, bbs, stm);
		}

	private:
		std::vector<UpdatableAccumulator> m_accumulatorStack{};
		UpdatableAccumulator *m_curr{};

		RefreshTable m_refreshTable{};

		static inline auto update(const Accumulator &prev,
			UpdatableAccumulator &curr, const UpdateContext &ctx, Color c) -> void
		{
			assert(!ctx.updates.refresh[static_cast<i32>(c)]);

			const auto subCount = ctx.updates.sub.size();
			const auto addCount = ctx.updates.add.size();

			if (subCount == 0 && addCount == 0)
				return;

			const auto king = ctx.kings.color(c);

			if (addCount == 1 && subCount == 1) // regular non-capture
			{
				const auto [subPiece, subSquare] = ctx.updates.sub[0];
				const auto [addPiece, addSquare] = ctx.updates.add[0];

				const auto sub = featureIndex(c, subPiece, subSquare, king);
				const auto add = featureIndex(c, addPiece, addSquare, king);

				curr.acc.subAddFrom(prev, g_network.featureTransformer(), c, sub, add);
			}
			else if (addCount == 1 && subCount == 2) // any capture
			{
				const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
				const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
				const auto [addPiece , addSquare ] = ctx.updates.add[0];

				const auto sub0 = featureIndex(c, subPiece0, subSquare0, king);
				const auto sub1 = featureIndex(c, subPiece1, subSquare1, king);
				const auto add  = featureIndex(c, addPiece , addSquare , king);

				curr.acc.subSubAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add);
			}
			else if (addCount == 2 && subCount == 2) // castling
			{
				const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
				const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
				const auto [addPiece0, addSquare0] = ctx.updates.add[0];
				const auto [addPiece1, addSquare1] = ctx.updates.add[1];

				const auto sub0 = featureIndex(c, subPiece0, subSquare0, king);
				const auto sub1 = featureIndex(c, subPiece1, subSquare1, king);
				const auto add0 = featureIndex(c, addPiece0, addSquare0, king);
				const auto add1 = featureIndex(c, addPiece1, addSquare1, king);

				curr.acc.subSubAddAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add0, add1);
			}
			else assert(false && "Materialising a piece from nowhere?");

			curr.setUpdated(c);
		}

		static inline auto updateBoth(const Accumulator &prev,
			UpdatableAccumulator &curr, const UpdateContext &ctx) -> void
		{
			update(prev, curr, ctx, Color::Black);
			update(prev, curr, ctx, Color::White);
		}

		inline auto findUsableAccumulator(Color c) -> UpdatableAccumulator  *
		{
			// scan back to the last non-dirty accumulator, or an accumulator that requires a refresh.
			// root accumulator is always up-to-date
			auto *curr = m_curr - 1;
			for (; curr->isDirty(c)
					&& !curr->ctx.updates.requiresRefresh(c);
				--curr) {}

			assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresRefresh(c));

			return curr;
		}

		inline auto ensureUpToDate(const BitboardSet &bbs, KingPair kings) -> void
		{
			for (const auto c : { Color::Black, Color::White })
			{
				if (!m_curr->isDirty(c))
					continue;

				// if the current accumulator needs a refresh, just do it
				if (m_curr->ctx.updates.requiresRefresh(c))
				{
					refreshAccumulator(*m_curr, c, bbs, m_refreshTable, kings.color(c));
					continue;
				}

				auto *curr = findUsableAccumulator(c);

				// if the found accumulator requires a refresh, just give up and refresh the current one
				if (curr->ctx.updates.requiresRefresh(c))
					refreshAccumulator(*m_curr, c, bbs, m_refreshTable, kings.color(c));
				else // otherwise go forward and incrementally update all accumulators in between
				{
					do
					{
						const auto &prev = *curr;

						++curr;
						update(prev.acc, *curr, curr->ctx, c);
					}
					while (curr != m_curr);
				}
			}
		}

		[[nodiscard]] static inline auto evaluate(const Accumulator &accumulator,
			const BitboardSet &bbs, Color stm) -> i32
		{
			assert(stm != Color::None);

			return stm == Color::Black
				? g_network.propagate(bbs, accumulator.black(), accumulator.white())
				: g_network.propagate(bbs, accumulator.white(), accumulator.black());
		}

		static inline auto calcUpdateCost(const BitboardSet &srcBbs, const BitboardSet &dstBbs) -> u32
		{
			u32 cost = 0;

			for (u32 pieceIdx = 0; pieceIdx < static_cast<u32>(Piece::None); ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				const auto prev = srcBbs.forPiece(piece);
				const auto curr = dstBbs.forPiece(piece);

				cost += (curr ^ prev).popcount();
			}

			return cost;
		}

		static inline auto diffAccumulator(Accumulator &accumulator, Color c,
			const BitboardSet &srcBbs, const BitboardSet &dstBbs, Square king) -> void
		{
			for (u32 pieceIdx = 0; pieceIdx < static_cast<u32>(Piece::None); ++pieceIdx)
			{
				const auto piece = static_cast<Piece>(pieceIdx);

				const auto prev = srcBbs.forPiece(piece);
				const auto curr = dstBbs.forPiece(piece);

				auto   added = curr & ~prev;
				auto removed = prev & ~curr;

				while (added)
				{
					const auto sq = added.popLowestSquare();
					const auto feature = featureIndex(c, piece, sq, king);

					accumulator.activateFeature(g_network.featureTransformer(), c, feature);
				}

				while (removed)
				{
					const auto sq = removed.popLowestSquare();
					const auto feature = featureIndex(c, piece, sq, king);

					accumulator.deactivateFeature(g_network.featureTransformer(), c, feature);
				}
			}
		}

		static inline auto refreshAccumulator(UpdatableAccumulator &accumulator, Color c,
			const BitboardSet &bbs, RefreshTable &refreshTable, Square king) -> void
		{
			const auto tableIdx = InputFeatureSet::getRefreshTableEntry(c, king);

			auto &rtEntry = refreshTable.table[tableIdx];
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

			accumulator.acc.copyFrom(c, rtEntry.accumulator);
			prevBoards = bbs;

			accumulator.setUpdated(c);
		}

		static inline auto resetAccumulator(Accumulator &accumulator,
			Color c, const BitboardSet &bbs, Square king) -> void
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

		static inline auto resetAccumulator(UpdatableAccumulator &accumulator,
			Color c, const BitboardSet &bbs, Square king) -> void
		{
			resetAccumulator(accumulator.acc, c, bbs, king);
			accumulator.setUpdated(c);
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

			const auto color = [piece, c]() -> u32
			{
				if (InputFeatureSet::MergedKings && pieceType(piece) == PieceType::King)
					return 0;
				return pieceColor(piece) == c ? 0 : 1;
			}();

			if (c == Color::Black)
				sq = flipSquareRank(sq);

			sq = InputFeatureSet::transformFeatureSquare(sq, king);

			const auto bucketOffset = InputFeatureSet::getBucket(c, king) * InputFeatureSet::InputSize;
			return bucketOffset + color * ColorStride + type * PieceStride + static_cast<u32>(sq);
		}
	};
}
