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
        PositionBoards boards{};
        KingPair kings{};
    };

    void addThreatFeatures(std::span<i16, kL1Size> acc, Color c, const PositionBoards& boards, Square king);

    template <bool kAdd>
    void updatePieceThreatsOnChange(NnueUpdates& updates, const PositionBoards& boards, Piece piece, Square sq);
    void updatePieceThreatsOnMutate(
        NnueUpdates& updates,
        const PositionBoards& boards,
        Piece oldPiece,
        Piece newPiece,
        Square sq
    );
    void updatePieceThreatsOnMove(
        NnueUpdates& updates,
        const PositionBoards& boards,
        Piece oldPiece,
        Square src,
        Piece newPiece,
        Square dst
    );

    struct BoardObserver {
        UpdateContext& ctx;

        void prepareKingMove(Color color, Square src, Square dst);

        void pieceAdded(const PositionBoards& boards, Piece piece, Square sq);
        void pieceRemoved(const PositionBoards& boards, Piece piece, Square sq);
        void pieceMutated(const PositionBoards& boards, Piece oldPiece, Piece newPiece, Square sq);
        void pieceMoved(const PositionBoards& boards, Piece piece, Square src, Square dst);
        void piecePromoted(const PositionBoards& boards, Piece oldPiece, Square src, Piece newPiece, Square dst);

        void finalize(const PositionBoards& boards, KingPair kings);
    };

    class NnueState {
    private:
        struct UpdatableAccumulator {
            Accumulator psqAcc{};
            std::array<Accumulator, InputFeatureSet::kThreatInputs> threatAcc{};

            UpdateContext ctx{};

            std::array<bool, 2> psqDirty{};
            std::array<bool, 2> threatDirty{};

            inline void setPsqDirty() {
                psqDirty[0] = psqDirty[1] = true;
            }

            inline void setPsqUpdated(Color c) {
                assert(c != Colors::kNone);
                psqDirty[c.idx()] = false;
            }

            [[nodiscard]] inline bool isPsqDirty(Color c) const {
                assert(c != Colors::kNone);
                return psqDirty[c.idx()];
            }

            inline void setThreatDirty() {
                threatDirty[0] = threatDirty[1] = true;
            }

            inline void setThreatUpdated(Color c) {
                assert(c != Colors::kNone);
                threatDirty[c.idx()] = false;
            }

            [[nodiscard]] inline bool isThreatDirty(Color c) const {
                assert(c != Colors::kNone);
                return threatDirty[c.idx()];
            }
        };

    public:
        NnueState() {
            m_accumulatorStack.resize(256);
        }

        inline void reset(const PositionBoards& boards, KingPair kings) {
            assert(kings.isValid());

            m_refreshTable.init(g_network.featureTransformer());

            m_curr = &m_accumulatorStack[0];

            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                const auto king = kings.color(c);
                const auto entry = InputFeatureSet::getRefreshTableEntry(c, king);

                auto& rtEntry = m_refreshTable.table[entry];
                resetPsqAccumulator(rtEntry.accumulator, c, boards, king);

                m_curr->psqAcc.copyFrom(c, rtEntry.accumulator);
                rtEntry.colorBbs(c) = boards.bbs();

                if constexpr (InputFeatureSet::kThreatInputs) {
                    resetThreatAccumulator(m_curr->threatAcc[0], c, boards, king);
                }
            }
        }

        inline BoardObserver push() {
            ++m_curr;

            m_curr->ctx = {};
            m_curr->setPsqDirty();
            m_curr->setThreatDirty();

            return BoardObserver{m_curr->ctx};
        }

        inline void applyImmediately(const UpdateContext& ctx) {
            updateBothPsq(m_curr->psqAcc, *m_curr, m_refreshTable, ctx);
            if constexpr (InputFeatureSet::kThreatInputs) {
                updateBothThreat(m_curr->threatAcc[0], *m_curr, ctx);
            }
        }

        inline void pop() {
            assert(m_curr > &m_accumulatorStack[0]);
            --m_curr;
        }

        [[nodiscard]] inline i32 evaluate(const PositionBoards& boards, KingPair kings, Color stm) {
            assert(m_curr >= &m_accumulatorStack[0] && m_curr <= &m_accumulatorStack.back());
            assert(stm != Colors::kNone);

            ensureUpToDate(boards, kings);

            if constexpr (InputFeatureSet::kThreatInputs) {
                return evaluate(m_curr->psqAcc, &m_curr->threatAcc[0], boards, stm);
            } else {
                return evaluate(m_curr->psqAcc, nullptr, boards, stm);
            }
        }

        [[nodiscard]] static inline i32 evaluateOnce(const PositionBoards& boards, KingPair kings, Color stm) {
            assert(kings.isValid());
            assert(stm != Colors::kNone);

            Accumulator psqAccumulator{};

            psqAccumulator.initBoth(g_network.featureTransformer());

            resetPsqAccumulator(psqAccumulator, Colors::kBlack, boards, kings.black());
            resetPsqAccumulator(psqAccumulator, Colors::kWhite, boards, kings.white());

            if constexpr (InputFeatureSet::kThreatInputs) {
                Accumulator threatAccumulator{};

                resetThreatAccumulator(threatAccumulator, Colors::kBlack, boards, kings.black());
                resetThreatAccumulator(threatAccumulator, Colors::kWhite, boards, kings.white());

                return evaluate(psqAccumulator, &threatAccumulator, boards, stm);
            } else {
                return evaluate(psqAccumulator, nullptr, boards, stm);
            }
        }

    private:
        std::vector<UpdatableAccumulator> m_accumulatorStack{};
        UpdatableAccumulator* m_curr{};

        RefreshTable m_refreshTable{};

        static inline void updatePsq(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            RefreshTable& refreshTable,
            const UpdateContext& ctx,
            Color c
        ) {
            if (ctx.updates.requiresPsqRefresh(c)) {
                refreshPsqAccumulator(curr, c, ctx.boards, refreshTable, ctx.kings.color(c));
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

                curr.psqAcc.subAddFrom(prev, g_network.featureTransformer(), c, sub, add);
            } else if (addCount == 1 && subCount == 2) // any capture
            {
                const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
                const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
                const auto [addPiece, addSquare] = ctx.updates.add[0];

                const auto sub0 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece0, subSquare0, king);
                const auto sub1 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece1, subSquare1, king);
                const auto add = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece, addSquare, king);

                curr.psqAcc.subSubAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add);
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

                curr.psqAcc.subSubAddAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add0, add1);
            } else {
                assert(false && "Materialising a piece from nowhere?");
            }

            curr.setPsqUpdated(c);
        }

        static inline void updateThreat(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            const UpdateContext& ctx,
            Color c
        ) {
            if (ctx.updates.requiresThreatRefresh(c)) {
                refreshThreatAccumulator(curr, c, ctx.boards, ctx.kings.color(c));
                return;
            }

            curr.threatAcc[0].copyFrom(c, prev);

            if (ctx.updates.threatsAdded.empty() && ctx.updates.threatsRemoved.empty()) {
                return;
            }

            const auto king = ctx.kings.color(c);

            const auto* threatWeights = g_network.featureTransformer().threatWeights.data();

            auto acc = curr.threatAcc[0].forColor(c);

            StaticVector<u32, 128> adds{};
            StaticVector<u32, 128> subs{};

            for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsAdded) {
                const auto feature =
                    nnue::features::threats::featureIndex(c, king, attacker, attackerSq, attacked, attackedSq);
                if (feature < nnue::features::threats::kTotalThreatFeatures) {
                    __builtin_prefetch(threatWeights + feature * kL1Size);
                    adds.push(feature);
                }
            }

            for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsRemoved) {
                const auto feature =
                    nnue::features::threats::featureIndex(c, king, attacker, attackerSq, attacked, attackedSq);
                if (feature < nnue::features::threats::kTotalThreatFeatures) {
                    __builtin_prefetch(threatWeights + feature * kL1Size);
                    subs.push(feature);
                }
            }

            for (const auto add : adds) {
                for (i32 i = 0; i < kL1Size; ++i) {
                    acc[i] += threatWeights[add * kL1Size + i];
                }
            }

            for (const auto sub : subs) {
                for (i32 i = 0; i < kL1Size; ++i) {
                    acc[i] -= threatWeights[sub * kL1Size + i];
                }
            }

            curr.setThreatUpdated(c);
        }

        static inline void updateBothPsq(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            RefreshTable& refreshTable,
            const UpdateContext& ctx
        ) {
            updatePsq(prev, curr, refreshTable, ctx, Colors::kBlack);
            updatePsq(prev, curr, refreshTable, ctx, Colors::kWhite);
        }

        static inline void updateBothThreat(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            const UpdateContext& ctx
        ) {
            updateThreat(prev, curr, ctx, Colors::kBlack);
            updateThreat(prev, curr, ctx, Colors::kWhite);
        }

        inline void ensureUpToDate(const PositionBoards& boards, KingPair kings) {
            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                if (!m_curr->isPsqDirty(c)) {
                    continue;
                }

                // if the current accumulator needs a refresh, just do it
                if (m_curr->ctx.updates.requiresPsqRefresh(c)) {
                    refreshPsqAccumulator(*m_curr, c, boards, m_refreshTable, kings.color(c));
                    continue;
                }

                // scan back to the last non-dirty accumulator, or an accumulator that requires a refresh.
                // root accumulator is always up-to-date
                auto* curr = m_curr - 1;
                for (; curr->isPsqDirty(c) && !curr->ctx.updates.requiresPsqRefresh(c); --curr) {}

                assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresPsqRefresh(c));

                // if the found accumulator requires a refresh, just give up and refresh the current one
                if (curr->ctx.updates.requiresPsqRefresh(c)) {
                    refreshPsqAccumulator(*m_curr, c, boards, m_refreshTable, kings.color(c));
                } else {
                    // otherwise go forward and incrementally update all accumulators in between
                    do {
                        const auto& prev = *curr;

                        ++curr;
                        updatePsq(prev.psqAcc, *curr, m_refreshTable, curr->ctx, c);
                    } while (curr != m_curr);
                }
            }

            if constexpr (InputFeatureSet::kThreatInputs) {
                // same logic as above
                for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                    if (!m_curr->isThreatDirty(c)) {
                        continue;
                    }

                    if (m_curr->ctx.updates.requiresThreatRefresh(c)) {
                        refreshThreatAccumulator(*m_curr, c, boards, kings.color(c));
                        continue;
                    }

                    auto* curr = m_curr - 1;
                    for (; curr->isThreatDirty(c) && !curr->ctx.updates.requiresThreatRefresh(c); --curr) {}

                    assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresThreatRefresh(c));

                    if (curr->ctx.updates.requiresThreatRefresh(c)) {
                        refreshThreatAccumulator(*m_curr, c, boards, kings.color(c));
                    } else {
                        do {
                            const auto& prev = *curr;

                            ++curr;
                            updateThreat(prev.threatAcc[0], *curr, curr->ctx, c);
                        } while (curr != m_curr);
                    }
                }
            }
        }

        [[nodiscard]] static inline i32 evaluate(
            const Accumulator& psqAccumulator,
            const Accumulator* threatAccumulator,
            const PositionBoards& boards,
            Color stm
        ) {
            assert(stm != Colors::kNone);
            if constexpr (InputFeatureSet::kThreatInputs) {
                return stm == Colors::kBlack ? g_network.propagate(
                                                   boards.bbs(),
                                                   psqAccumulator.black(),
                                                   psqAccumulator.white(),
                                                   threatAccumulator->black(),
                                                   threatAccumulator->white()
                                               )[0]
                                             : g_network.propagate(
                                                   boards.bbs(),
                                                   psqAccumulator.white(),
                                                   psqAccumulator.black(),
                                                   threatAccumulator->white(),
                                                   threatAccumulator->black()
                                               )[0];
            } else {
                // just pass the psq accumulators again, they're unused
                return stm == Colors::kBlack ? g_network.propagate(
                                                   boards.bbs(),
                                                   psqAccumulator.black(),
                                                   psqAccumulator.white(),
                                                   psqAccumulator.black(),
                                                   psqAccumulator.white()
                                               )[0]
                                             : g_network.propagate(
                                                   boards.bbs(),
                                                   psqAccumulator.white(),
                                                   psqAccumulator.black(),
                                                   psqAccumulator.black(),
                                                   psqAccumulator.white()
                                               )[0];
            }
        }

        static inline void refreshPsqAccumulator(
            UpdatableAccumulator& accumulator,
            Color c,
            const PositionBoards& boards,
            RefreshTable& refreshTable,
            Square king
        ) {
            const auto& bbs = boards.bbs();
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

            accumulator.psqAcc.copyFrom(c, rtEntry.accumulator);
            prevBoards = bbs;

            accumulator.setPsqUpdated(c);
        }

        static inline void refreshThreatAccumulator(
            UpdatableAccumulator& accumulator,
            Color c,
            const PositionBoards& boards,
            Square king
        ) {
            if constexpr (InputFeatureSet::kThreatInputs) {
                resetThreatAccumulator(accumulator.threatAcc[0], c, boards, king);
                accumulator.setThreatUpdated(c);
            }
        }

        static inline void resetPsqAccumulator(
            Accumulator& accumulator,
            Color c,
            const PositionBoards& boards,
            Square king
        ) {
            assert(c != Colors::kNone);
            assert(king != Squares::kNone);

            // loop through each coloured piece, and activate the features
            // corresponding to that piece on each of the squares it occurs on
            for (u32 pieceIdx = 0; pieceIdx < Pieces::kCount; ++pieceIdx) {
                const auto piece = Piece::fromRaw(pieceIdx);

                auto board = boards.bbs().forPiece(piece);
                while (!board.empty()) {
                    const auto sq = board.popLowestSquare();

                    const auto feature = nnue::features::psq::featureIndex<InputFeatureSet>(c, piece, sq, king);
                    accumulator.activateFeature(g_network.featureTransformer(), c, feature);
                }
            }
        }

        static inline void resetThreatAccumulator(
            Accumulator& accumulator,
            Color c,
            const PositionBoards& boards,
            Square king
        ) {
            assert(c != Colors::kNone);
            assert(king != Squares::kNone);

            if constexpr (InputFeatureSet::kThreatInputs) {
                accumulator.clear(c);
                addThreatFeatures(accumulator.forColor(c), c, boards, king);
            }
        }
    };

    inline void BoardObserver::prepareKingMove(Color color, Square src, Square dst) {
        if (InputFeatureSet::refreshRequired(color, src, dst)) {
            ctx.updates.setPsqRefresh(color);
        }

        if constexpr (InputFeatureSet::kThreatInputs) {
            if ((src.file() >= kFileE) != (dst.file() >= kFileE)) {
                ctx.updates.setThreatRefresh(color);
            }
        }
    }

    inline void BoardObserver::pieceAdded(const PositionBoards& boards, Piece piece, Square sq) {
        ctx.updates.pushAdd(piece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnChange<true>(ctx.updates, boards, piece, sq);
        }
    }

    inline void BoardObserver::pieceRemoved(const PositionBoards& boards, Piece piece, Square sq) {
        ctx.updates.pushSub(piece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnChange<false>(ctx.updates, boards, piece, sq);
        }
    }

    inline void BoardObserver::pieceMutated(const PositionBoards& boards, Piece oldPiece, Piece newPiece, Square sq) {
        ctx.updates.pushSub(oldPiece, sq);
        ctx.updates.pushAdd(newPiece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnMutate(ctx.updates, boards, oldPiece, newPiece, sq);
        }
    }

    inline void BoardObserver::pieceMoved(const PositionBoards& boards, Piece piece, Square src, Square dst) {
        ctx.updates.pushSub(piece, src);
        ctx.updates.pushAdd(piece, dst);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnMove(ctx.updates, boards, piece, src, piece, dst);
        }
    }

    inline void BoardObserver::piecePromoted(
        const PositionBoards& boards,
        Piece oldPiece,
        Square src,
        Piece newPiece,
        Square dst
    ) {
        ctx.updates.pushSub(oldPiece, src);
        ctx.updates.pushAdd(newPiece, dst);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnMove(ctx.updates, boards, oldPiece, src, newPiece, dst);
        }
    }

    inline void BoardObserver::finalize(const PositionBoards& boards, KingPair kings) {
        ctx.boards = boards;
        ctx.kings = kings;
    }
} // namespace stormphrax::eval
