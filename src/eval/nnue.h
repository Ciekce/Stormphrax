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
    void updatePieceThreats(
        NnueUpdates& updates,
        const PositionBoards& boards,
        Piece piece,
        Square sq,
        bool discovery = true,
        Bitboard discoveryMask = Bitboard::kAll
    );

    struct BoardObserver {
        UpdateContext& ctx;

        void prepareKingMove(Color color, Square src, Square dst);

        void pieceAdded(
            const PositionBoards& boards,
            Piece piece,
            Square sq,
            bool discovery,
            Bitboard discoveryMask = Bitboard::kAll
        );
        void pieceRemoved(
            const PositionBoards& boards,
            Piece piece,
            Square sq,
            bool discovery,
            Bitboard discoveryMask = Bitboard::kAll
        );

        void finalize(const PositionBoards& boards, KingPair kings);
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

        inline void reset(const PositionBoards& boards, KingPair kings) {
            assert(kings.isValid());

            m_refreshTable.init(g_network.featureTransformer());

            m_curr = &m_accumulatorStack[0];

            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                const auto king = kings.color(c);
                const auto entry = InputFeatureSet::getRefreshTableEntry(c, king);

                auto& rtEntry = m_refreshTable.table[entry];
                resetAccumulator<false>(rtEntry.accumulator, c, boards, king);

                m_curr->acc.copyFrom(c, rtEntry.accumulator);
                rtEntry.colorBbs(c) = boards.bbs();

                if constexpr (InputFeatureSet::kThreatInputs) {
                    addThreatFeatures(m_curr->acc.forColor(c), c, boards, king);
                }
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

            ensureUpToDate(boards, kings);

            return evaluate(m_curr->acc, boards, stm);
        }

        [[nodiscard]] static inline i32 evaluateOnce(const PositionBoards& boards, KingPair kings, Color stm) {
            assert(kings.isValid());
            assert(stm != Colors::kNone);

            Accumulator accumulator{};

            accumulator.initBoth(g_network.featureTransformer());

            resetAccumulator(accumulator, Colors::kBlack, boards, kings.black());
            resetAccumulator(accumulator, Colors::kWhite, boards, kings.white());

            return evaluate(accumulator, boards, stm);
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
                refreshAccumulator(curr, c, ctx.boards, refreshTable, ctx.kings.color(c));
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

            if constexpr (InputFeatureSet::kThreatInputs) {
                auto acc = curr.acc.forColor(c);
                for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsAdded) {
                    const auto feature =
                        nnue::features::threats::featureIndex(c, king, attacker, attackerSq, attacked, attackedSq);
                    if (feature >= nnue::features::threats::kTotalThreatFeatures) {
                        continue;
                    }
                    const auto* start = &g_network.featureTransformer().threatWeights[feature * kL1Size];
                    for (i32 i = 0; i < kL1Size; ++i) {
                        acc[i] += start[i];
                    }
                }
                for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsRemoved) {
                    const auto feature =
                        nnue::features::threats::featureIndex(c, king, attacker, attackerSq, attacked, attackedSq);
                    if (feature >= nnue::features::threats::kTotalThreatFeatures) {
                        continue;
                    }
                    const auto* start = &g_network.featureTransformer().threatWeights[feature * kL1Size];
                    for (i32 i = 0; i < kL1Size; ++i) {
                        acc[i] -= start[i];
                    }
                }
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

        inline void ensureUpToDate(const PositionBoards& boards, KingPair kings) {
            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                if (!m_curr->isDirty(c)) {
                    continue;
                }

                // if the current accumulator needs a refresh, just do it
                if (m_curr->ctx.updates.requiresRefresh(c)) {
                    refreshAccumulator(*m_curr, c, boards, m_refreshTable, kings.color(c));
                    continue;
                }

                // scan back to the last non-dirty accumulator, or an accumulator that requires a refresh.
                // root accumulator is always up-to-date
                auto* curr = m_curr - 1;
                for (; curr->isDirty(c) && !curr->ctx.updates.requiresRefresh(c); --curr) {}

                assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresRefresh(c));

                // if the found accumulator requires a refresh, just give up and refresh the current one
                if (curr->ctx.updates.requiresRefresh(c)) {
                    refreshAccumulator(*m_curr, c, boards, m_refreshTable, kings.color(c));
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
            Color stm
        ) {
            assert(stm != Colors::kNone);
            return stm == Colors::kBlack
                     ? g_network.propagate(boards.bbs(), accumulator.black(), accumulator.white())[0]
                     : g_network.propagate(boards.bbs(), accumulator.white(), accumulator.black())[0];
        }

        static inline void refreshAccumulator(
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

            accumulator.acc.copyFrom(c, rtEntry.accumulator);
            prevBoards = bbs;

            if constexpr (InputFeatureSet::kThreatInputs) {
                addThreatFeatures(accumulator.acc.forColor(c), c, boards, king);
            }

            accumulator.setUpdated(c);
        }

        template <bool kActivateThreats = true>
        static inline void resetAccumulator(
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

            if constexpr (InputFeatureSet::kThreatInputs && kActivateThreats) {
                addThreatFeatures(accumulator.forColor(c), c, boards, king);
            }
        }
    };

    inline void BoardObserver::prepareKingMove(Color color, Square src, Square dst) {
        if (InputFeatureSet::refreshRequired(color, src, dst)) {
            ctx.updates.setRefresh(color);
        }

        if constexpr (InputFeatureSet::kThreatInputs) {
            if ((src.file() >= kFileE) != (dst.file() >= kFileE)) {
                ctx.updates.setRefresh(color);
            }
        }
    }

    inline void BoardObserver::pieceAdded(
        const PositionBoards& boards,
        Piece piece,
        Square sq,
        bool discovery,
        Bitboard discoveryMask
    ) {
        ctx.updates.pushAdd(piece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreats<true>(ctx.updates, boards, piece, sq, discovery, discoveryMask);
        }
    }

    inline void BoardObserver::pieceRemoved(
        const PositionBoards& boards,
        Piece piece,
        Square sq,
        bool discovery,
        Bitboard discoveryMask
    ) {
        ctx.updates.pushSub(piece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreats<false>(ctx.updates, boards, piece, sq, discovery, discoveryMask);
        }
    }

    inline void BoardObserver::finalize(const PositionBoards& boards, KingPair kings) {
        ctx.boards = boards;
        ctx.kings = kings;
    }
} // namespace stormphrax::eval
