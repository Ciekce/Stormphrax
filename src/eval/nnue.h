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

#pragma once

#include "../types.h"

#include <vector>

#include "../util/static_vector.h"
#include "arch.h"
#include "nnue/activation.h"
#include "nnue/arch/multilayer.h"
#include "nnue/input.h"
#include "nnue/network.h"

namespace stormphrax::eval {
    using FeatureTransformer = nnue::FeatureTransformer<i16, i8, kL1Size, InputFeatureSet>;
    using Network = nnue::PerspectiveNetwork<FeatureTransformer, OutputBucketing, LayeredArch>;

    using Accumulator = FeatureTransformer::Accumulator;
    using RefreshTable = FeatureTransformer::RefreshTable;

    using NnueUpdates = InputFeatureSet::Updates;

    extern const Network& g_network;

    void loadDefaultNetwork();
    void loadNetwork(std::string_view name);

    [[nodiscard]] std::string_view defaultNetworkName();

    struct UpdateContext {
        NnueUpdates updates{};
        BitboardSet bbs{};
        KingPair kings{};
    };

    void addThreatFeatures(std::span<i16, kL1Size> acc, Color c, const PositionBoards& boards, Square king);

    struct BoardObserver {
        UpdateContext& ctx;

        void prepareKingMove(Color color, Square src, Square dst);
        void pieceMoved(Piece piece, Square src, Square dst);
        void pieceCaptured(Piece piece, Square src, Square dst, Piece captured);
        void pawnPromoted(Piece pawn, Square src, Square dst, Piece promo);
        void pawnPromoteCaptured(Piece pawn, Square src, Square dst, Piece promo, Piece captured);
        void castled(Piece king, Square kingSrc, Square kingDst, Piece rook, Square rookSrc, Square rookDst);
        void enPassanted(Piece pawn, Square src, Square dst, Piece enemyPawn, Square captureSquare);
        void finalize(BitboardSet bbs, KingPair kings);
    };

    class NnueState {
    private:
        struct UpdatableAccumulator {
            Accumulator acc{};
            UpdateContext ctx{};
            std::array<bool, 2> dirty{};

            inline void setDirty() {
                dirty[0] = dirty[1] = true;
            }

            inline void setUpdated(Color c) {
                assert(c != Colors::kNone);
                dirty[c.idx()] = false;
            }

            [[nodiscard]] inline bool isDirty(Color c) {
                assert(c != Colors::kNone);
                return dirty[c.idx()];
            }
        };

    public:
        NnueState() {
            m_accumulatorStack.resize(256);
        }

        inline void reset(const BitboardSet& bbs, KingPair kings) {
            assert(kings.isValid());

            m_refreshTable.init(g_network.featureTransformer());

            m_curr = &m_accumulatorStack[0];

            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                const auto king = kings.color(c);
                const auto entry = InputFeatureSet::getRefreshTableEntry(c, king);

                auto& rtEntry = m_refreshTable.table[entry];
                resetAccumulator(rtEntry.accumulator, c, bbs, king);

                m_curr->acc.copyFrom(c, rtEntry.accumulator);
                rtEntry.colorBbs(c) = bbs;
            }
        }

        inline BoardObserver push() {
            ++m_curr;

            m_curr->ctx = {};
            m_curr->setDirty();

            return BoardObserver{m_curr->ctx};
        }

        inline void apply(const UpdateContext& ctx) {
            updateBoth(m_curr->acc, *m_curr, m_refreshTable, ctx);
        }

        inline void pop() {
            assert(m_curr > &m_accumulatorStack[0]);
            --m_curr;
        }

        [[nodiscard]] inline i32 evaluate(const PositionBoards& boards, KingPair kings, Color stm) {
            assert(m_curr >= &m_accumulatorStack[0] && m_curr <= &m_accumulatorStack.back());
            assert(stm != Colors::kNone);

            ensureUpToDate(boards.bbs(), kings);

            return evaluate(m_curr->acc, boards, kings, stm);
        }

        [[nodiscard]] static inline i32 evaluateOnce(const PositionBoards& boards, KingPair kings, Color stm) {
            assert(kings.isValid());
            assert(stm != Colors::kNone);

            Accumulator accumulator{};

            accumulator.initBoth(g_network.featureTransformer());

            resetAccumulator(accumulator, Colors::kBlack, boards.bbs(), kings.black());
            resetAccumulator(accumulator, Colors::kWhite, boards.bbs(), kings.white());

            return evaluate(accumulator, boards, kings, stm);
        }

    private:
        std::vector<UpdatableAccumulator> m_accumulatorStack{};
        UpdatableAccumulator* m_curr{};

        RefreshTable m_refreshTable{};

        static inline void update(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            RefreshTable& refreshTable,
            const UpdateContext& ctx,
            Color c
        ) {
            if (ctx.updates.requiresRefresh(c)) {
                refreshAccumulator(curr, c, ctx.bbs, refreshTable, ctx.kings.color(c));
                return;
            }

            const auto subCount = ctx.updates.sub.size();
            const auto addCount = ctx.updates.add.size();

            if (subCount == 0 && addCount == 0) {
                return;
            }

            const auto king = ctx.kings.color(c);

            if (addCount == 1 && subCount == 1) // regular non-capture
            {
                const auto [subPiece, subSquare] = ctx.updates.sub[0];
                const auto [addPiece, addSquare] = ctx.updates.add[0];

                const auto sub = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece, subSquare, king);
                const auto add = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece, addSquare, king);

                curr.acc.subAddFrom(prev, g_network.featureTransformer(), c, sub, add);
            } else if (addCount == 1 && subCount == 2) // any capture
            {
                const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
                const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
                const auto [addPiece, addSquare] = ctx.updates.add[0];

                const auto sub0 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece0, subSquare0, king);
                const auto sub1 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece1, subSquare1, king);
                const auto add = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece, addSquare, king);

                curr.acc.subSubAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add);
            } else if (addCount == 2 && subCount == 2) // castling
            {
                const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
                const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
                const auto [addPiece0, addSquare0] = ctx.updates.add[0];
                const auto [addPiece1, addSquare1] = ctx.updates.add[1];

                const auto sub0 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece0, subSquare0, king);
                const auto sub1 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece1, subSquare1, king);
                const auto add0 = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece0, addSquare0, king);
                const auto add1 = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece1, addSquare1, king);

                curr.acc.subSubAddAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add0, add1);
            } else {
                assert(false && "Materialising a piece from nowhere?");
            }

            curr.setUpdated(c);
        }

        static inline void updateBoth(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            RefreshTable& refreshTable,
            const UpdateContext& ctx
        ) {
            update(prev, curr, refreshTable, ctx, Colors::kBlack);
            update(prev, curr, refreshTable, ctx, Colors::kWhite);
        }

        inline void ensureUpToDate(const BitboardSet& bbs, KingPair kings) {
            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                if (!m_curr->isDirty(c)) {
                    continue;
                }

                // if the current accumulator needs a refresh, just do it
                if (m_curr->ctx.updates.requiresRefresh(c)) {
                    refreshAccumulator(*m_curr, c, bbs, m_refreshTable, kings.color(c));
                    continue;
                }

                // scan back to the last non-dirty accumulator, or an accumulator that requires a refresh.
                // root accumulator is always up-to-date
                auto* curr = m_curr - 1;
                for (; curr->isDirty(c) && !curr->ctx.updates.requiresRefresh(c); --curr) {}

                assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresRefresh(c));

                // if the found accumulator requires a refresh, just give up and refresh the current one
                if (curr->ctx.updates.requiresRefresh(c)) {
                    refreshAccumulator(*m_curr, c, bbs, m_refreshTable, kings.color(c));
                } else {
                    // otherwise go forward and incrementally update all accumulators in between
                    do {
                        const auto& prev = *curr;

                        ++curr;
                        update(prev.acc, *curr, m_refreshTable, curr->ctx, c);
                    } while (curr != m_curr);
                }
            }
        }

        [[nodiscard]] static inline i32 evaluate(
            const Accumulator& accumulator,
            const PositionBoards& boards,
            const KingPair& kings,
            Color stm
        ) {
            assert(stm != Colors::kNone);

            if constexpr (InputFeatureSet::kThreatInputs) {
                util::simd::Array<i16, kL1Size> black;
                util::simd::Array<i16, kL1Size> white;

                std::ranges::copy(accumulator.black(), black.begin());
                std::ranges::copy(accumulator.white(), white.begin());

                addThreatFeatures(black, Colors::kBlack, boards, kings.black());
                addThreatFeatures(white, Colors::kWhite, boards, kings.white());

                return stm == Colors::kBlack ? g_network.propagate(boards.bbs(), black, white)[0]
                                             : g_network.propagate(boards.bbs(), white, black)[0];
            }

            return stm == Colors::kBlack
                     ? g_network.propagate(boards.bbs(), accumulator.black(), accumulator.white())[0]
                     : g_network.propagate(boards.bbs(), accumulator.white(), accumulator.black())[0];
        }

        static inline void refreshAccumulator(
            UpdatableAccumulator& accumulator,
            Color c,
            const BitboardSet& bbs,
            RefreshTable& refreshTable,
            Square king
        ) {
            const auto tableIdx = InputFeatureSet::getRefreshTableEntry(c, king);

            auto& rtEntry = refreshTable.table[tableIdx];
            auto& prevBoards = rtEntry.colorBbs(c);

            StaticVector<u32, 32> adds;
            StaticVector<u32, 32> subs;
            for (u32 pieceIdx = 0; pieceIdx < Pieces::kNone.raw(); ++pieceIdx) {
                const auto piece = Piece::fromRaw(pieceIdx);

                const auto prev = prevBoards.forPiece(piece);
                const auto curr = bbs.forPiece(piece);

                auto added = curr & ~prev;
                auto removed = prev & ~curr;

                while (added) {
                    const auto sq = added.popLowestSquare();
                    const auto feature = nnue::features::psq::featureIndex<InputFeatureSet>(c, piece, sq, king);
                    adds.push(feature);
                }

                while (removed) {
                    const auto sq = removed.popLowestSquare();
                    const auto feature = nnue::features::psq::featureIndex<InputFeatureSet>(c, piece, sq, king);
                    subs.push(feature);
                }
            }

            while (adds.size() >= 4) {
                const auto add0 = adds.pop();
                const auto add1 = adds.pop();
                const auto add2 = adds.pop();
                const auto add3 = adds.pop();
                rtEntry.accumulator.activateFourFeatures(g_network.featureTransformer(), c, add0, add1, add2, add3);
            }

            while (adds.size() >= 1) {
                const auto add = adds.pop();
                rtEntry.accumulator.activateFeature(g_network.featureTransformer(), c, add);
            }

            while (subs.size() >= 4) {
                const auto sub0 = subs.pop();
                const auto sub1 = subs.pop();
                const auto sub2 = subs.pop();
                const auto sub3 = subs.pop();
                rtEntry.accumulator.deactivateFourFeatures(g_network.featureTransformer(), c, sub0, sub1, sub2, sub3);
            }

            while (subs.size() >= 1) {
                const auto sub = subs.pop();
                rtEntry.accumulator.deactivateFeature(g_network.featureTransformer(), c, sub);
            }

            accumulator.acc.copyFrom(c, rtEntry.accumulator);
            prevBoards = bbs;

            accumulator.setUpdated(c);
        }

        static inline void resetAccumulator(Accumulator& accumulator, Color c, const BitboardSet& bbs, Square king) {
            assert(c != Colors::kNone);
            assert(king != Squares::kNone);

            // loop through each coloured piece, and activate the features
            // corresponding to that piece on each of the squares it occurs on
            for (u32 pieceIdx = 0; pieceIdx < Pieces::kCount; ++pieceIdx) {
                const auto piece = Piece::fromRaw(pieceIdx);

                auto board = bbs.forPiece(piece);
                while (!board.empty()) {
                    const auto sq = board.popLowestSquare();

                    const auto feature = nnue::features::psq::featureIndex<InputFeatureSet>(c, piece, sq, king);
                    accumulator.activateFeature(g_network.featureTransformer(), c, feature);
                }
            }
        }

        static inline void resetAccumulator(
            UpdatableAccumulator& accumulator,
            Color c,
            const BitboardSet& bbs,
            Square king
        ) {
            resetAccumulator(accumulator.acc, c, bbs, king);
            accumulator.setUpdated(c);
        }
    };

    inline void BoardObserver::prepareKingMove(Color color, Square src, Square dst) {
        if (InputFeatureSet::refreshRequired(color, src, dst)) {
            ctx.updates.setRefresh(color);
        }
    }

    inline void BoardObserver::pieceMoved(Piece piece, Square src, Square dst) {
        ctx.updates.pushSubAdd(piece, src, dst);
    }

    inline void BoardObserver::pieceCaptured(Piece piece, Square src, Square dst, Piece captured) {
        ctx.updates.pushSubAdd(piece, src, dst);
        ctx.updates.pushSub(captured, dst);
    }

    inline void BoardObserver::pawnPromoted(Piece pawn, Square src, Square dst, Piece promo) {
        ctx.updates.pushSub(pawn, src);
        ctx.updates.pushAdd(promo, dst);
    }

    inline void BoardObserver::pawnPromoteCaptured(Piece pawn, Square src, Square dst, Piece promo, Piece captured) {
        ctx.updates.pushSub(captured, dst);
        ctx.updates.pushSub(pawn, src);
        ctx.updates.pushAdd(promo, dst);
    }

    inline void BoardObserver::castled(
        Piece king,
        Square kingSrc,
        Square kingDst,
        Piece rook,
        Square rookSrc,
        Square rookDst
    ) {
        prepareKingMove(king.color(), kingSrc, kingDst);
        ctx.updates.pushSubAdd(king, kingSrc, kingDst);
        ctx.updates.pushSubAdd(rook, rookSrc, rookDst);
    }

    inline void BoardObserver::enPassanted(Piece pawn, Square src, Square dst, Piece enemyPawn, Square captureSquare) {
        ctx.updates.pushSubAdd(pawn, src, dst);
        ctx.updates.pushSub(enemyPawn, captureSquare);
    }

    inline void BoardObserver::finalize(BitboardSet bbs, KingPair kings) {
        ctx.bbs = bbs;
        ctx.kings = kings;
    }
} // namespace stormphrax::eval
