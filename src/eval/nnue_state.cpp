/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "nnue_state.h"

#include "../util/static_vector.h"

namespace stormphrax::eval {
    namespace {
        void updatePsq(
            const Network& network,
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            const UpdateContext& ctx,
            Color c
        ) {
            assert(!ctx.updates.requiresPsqRefresh(c));

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

                curr.psqAcc.subAddFrom(prev, network.featureTransformer(), c, sub, add);
            } else if (addCount == 1 && subCount == 2) // any capture
            {
                const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
                const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
                const auto [addPiece, addSquare] = ctx.updates.add[0];

                const auto sub0 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece0, subSquare0, king);
                const auto sub1 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece1, subSquare1, king);
                const auto add = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece, addSquare, king);

                curr.psqAcc.subSubAddFrom(prev, network.featureTransformer(), c, sub0, sub1, add);
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

                curr.psqAcc.subSubAddAddFrom(prev, network.featureTransformer(), c, sub0, sub1, add0, add1);
            } else {
                assert(false && "Materialising a piece from nowhere?");
            }

            curr.setPsqUpdated(c);
        }

        // Updates a threat accumulator in place.
        // Assumed to already be initialised to the previous accumulator
        void applyThreatUpdates(const Network& network, UpdatableAccumulator& curr, const UpdateContext& ctx, Color c) {
            assert(!ctx.updates.requiresThreatRefresh(c));

            if (ctx.updates.threatsAdded.empty() && ctx.updates.threatsRemoved.empty()) {
                return;
            }

            const auto king = ctx.kings.color(c);

            auto acc = curr.threatAcc[0].forColor(c);

            usize addIndex = 0, subIndex = 0;
            StaticVector<u32, nnue::features::threats::kMaxThreatsAdded> addFeatures;
            StaticVector<u32, nnue::features::threats::kMaxThreatsAdded> subFeatures;

            for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsAdded) {
                const auto feature =
                    nnue::features::threats::featureIndex(c, king, attacker, attackerSq, attacked, attackedSq);
                if (feature >= nnue::features::threats::kTotalThreatFeatures) {
                    continue;
                }
                addFeatures.push(feature);
            }

            for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsRemoved) {
                const auto feature =
                    nnue::features::threats::featureIndex(c, king, attacker, attackerSq, attacked, attackedSq);
                if (feature >= nnue::features::threats::kTotalThreatFeatures) {
                    continue;
                }
                subFeatures.push(feature);
            }

            while (addIndex < addFeatures.size() && subIndex < subFeatures.size()) {
                const auto addFeature = addFeatures[addIndex++];
                const auto subFeature = subFeatures[subIndex++];
                const auto* add = &network.featureTransformer().threatWeights[addFeature * kL1Size];
                const auto* sub = &network.featureTransformer().threatWeights[subFeature * kL1Size];
                for (i32 i = 0; i < kL1Size; ++i) {
                    acc[i] += add[i];
                    acc[i] -= sub[i];
                }
            }

            while (addIndex < addFeatures.size()) {
                const auto addFeature = addFeatures[addIndex++];
                const auto* add = &network.featureTransformer().threatWeights[addFeature * kL1Size];
                for (i32 i = 0; i < kL1Size; ++i) {
                    acc[i] += add[i];
                }
            }

            while (subIndex < subFeatures.size()) {
                const auto subFeature = subFeatures[subIndex++];
                const auto* sub = &network.featureTransformer().threatWeights[subFeature * kL1Size];
                for (i32 i = 0; i < kL1Size; ++i) {
                    acc[i] -= sub[i];
                }
            }

            curr.setThreatUpdated(c);
        }

        [[nodiscard]] i32 evaluateNetwork(
            const Network& network,
            const Accumulator& psqAccumulator,
            const Accumulator* threatAccumulator,
            const Position& pos,
            Color stm
        ) {
            assert(stm != Colors::kNone);
            if constexpr (InputFeatureSet::kThreatInputs) {
                return stm == Colors::kBlack //
                         ? network.propagate(
                               pos,
                               psqAccumulator.black(),
                               psqAccumulator.white(),
                               threatAccumulator->black(),
                               threatAccumulator->white()
                           )[0]
                         : network.propagate(
                               pos,
                               psqAccumulator.white(),
                               psqAccumulator.black(),
                               threatAccumulator->white(),
                               threatAccumulator->black()
                           )[0];
            } else {
                // just pass the psq accumulators again, they're unused
                return stm == Colors::kBlack //
                         ? network.propagate(
                               pos,
                               psqAccumulator.black(),
                               psqAccumulator.white(),
                               psqAccumulator.black(),
                               psqAccumulator.white()
                           )[0]
                         : network.propagate(
                               pos,
                               psqAccumulator.white(),
                               psqAccumulator.black(),
                               psqAccumulator.black(),
                               psqAccumulator.white()
                           )[0];
            }
        }

        void resetPsqAccumulator(const Network& network, Accumulator& accumulator, Color c, const Position& pos) {
            assert(c != Colors::kNone);

            const auto kingSq = pos.king(c);

            for (const auto [piece, sq] : pos) {
                const auto feature = nnue::features::psq::featureIndex<InputFeatureSet>(c, piece, sq, kingSq);
                accumulator.activateFeature(network.featureTransformer(), c, feature);
            }
        }

        void resetThreatAccumulator(const Network& network, Accumulator& accumulator, Color c, const Position& pos) {
            assert(c != Colors::kNone);
            if constexpr (InputFeatureSet::kThreatInputs) {
                accumulator.clear(c);
                addThreatFeatures(network, accumulator.forColor(c), c, pos);
            }
        }

        void refreshPsqAccumulator(
            const Network& network,
            UpdatableAccumulator& accumulator,
            Color c,
            const Position& pos,
            RefreshTable& refreshTable
        ) {
            const auto& bbs = pos.bbs();

            const auto kingSq = pos.king(c);
            const auto tableIdx = InputFeatureSet::getRefreshTableEntry(c, kingSq);

            auto& rtEntry = refreshTable.table[tableIdx];
            auto& prevBbs = rtEntry.colorBbs(c);

            StaticVector<u32, 32> adds;
            StaticVector<u32, 32> subs;

            for (u32 pieceIdx = 0; pieceIdx < Pieces::kNone.raw(); ++pieceIdx) {
                const auto piece = Piece::fromRaw(pieceIdx);

                const auto prev = prevBbs.bb(piece);
                const auto curr = bbs.bb(piece);

                for (const auto addedSq : curr & ~prev) {
                    const auto feature = nnue::features::psq::featureIndex<InputFeatureSet>(c, piece, addedSq, kingSq);
                    adds.push(feature);
                }

                for (const auto removedSq : prev & ~curr) {
                    const auto feature =
                        nnue::features::psq::featureIndex<InputFeatureSet>(c, piece, removedSq, kingSq);
                    subs.push(feature);
                }
            }

            while (adds.size() >= 4) {
                const auto add0 = adds.pop();
                const auto add1 = adds.pop();
                const auto add2 = adds.pop();
                const auto add3 = adds.pop();
                rtEntry.accumulator.activateFourFeatures(network.featureTransformer(), c, add0, add1, add2, add3);
            }

            while (adds.size() >= 1) {
                const auto add = adds.pop();
                rtEntry.accumulator.activateFeature(network.featureTransformer(), c, add);
            }

            while (subs.size() >= 4) {
                const auto sub0 = subs.pop();
                const auto sub1 = subs.pop();
                const auto sub2 = subs.pop();
                const auto sub3 = subs.pop();
                rtEntry.accumulator.deactivateFourFeatures(network.featureTransformer(), c, sub0, sub1, sub2, sub3);
            }

            while (subs.size() >= 1) {
                const auto sub = subs.pop();
                rtEntry.accumulator.deactivateFeature(network.featureTransformer(), c, sub);
            }

            accumulator.psqAcc.copyFrom(c, rtEntry.accumulator);
            prevBbs = bbs;

            accumulator.setPsqUpdated(c);
        }

        void refreshThreatAccumulator(
            const Network& network,
            UpdatableAccumulator& accumulator,
            Color c,
            const Position& pos
        ) {
            if constexpr (InputFeatureSet::kThreatInputs) {
                resetThreatAccumulator(network, accumulator.threatAcc[0], c, pos);
                accumulator.setThreatUpdated(c);
            }
        }
    } // namespace

    void NnueState::reset(const Position& pos) {
        assert(m_network);

        m_refreshTable.init(m_network->featureTransformer());

        m_top = &m_accumulatorStack[0];

        for (const auto c : {Colors::kBlack, Colors::kWhite}) {
            const auto king = pos.king(c);
            const auto entry = InputFeatureSet::getRefreshTableEntry(c, king);

            auto& rtEntry = m_refreshTable.table[entry];
            resetPsqAccumulator(*m_network, rtEntry.accumulator, c, pos);

            m_top->psqAcc.copyFrom(c, rtEntry.accumulator);
            rtEntry.colorBbs(c) = pos.bbs();

            if constexpr (InputFeatureSet::kThreatInputs) {
                resetThreatAccumulator(*m_network, m_top->threatAcc[0], c, pos);
            }
        }
    }

    BoardObserver NnueState::push() {
        ++m_top;

        m_top->ctx = {};
        m_top->setPsqDirty();
        m_top->setThreatDirty();

        return BoardObserver{m_top->ctx};
    }

    void NnueState::applyImmediately(const UpdateContext& ctx, const Position& pos) {
        assert(m_network);
        for (const auto c : {Colors::kBlack, Colors::kWhite}) {
            assert(pos.king(c) == ctx.kings.color(c));

            if (ctx.updates.requiresPsqRefresh(c)) {
                refreshPsqAccumulator(*m_network, *m_top, c, pos, m_refreshTable);
            } else {
                updatePsq(*m_network, m_top->psqAcc, *m_top, ctx, c);
            }

            if constexpr (InputFeatureSet::kThreatInputs) {
                if (ctx.updates.requiresThreatRefresh(c)) {
                    refreshThreatAccumulator(*m_network, *m_top, c, pos);
                } else {
                    applyThreatUpdates(*m_network, *m_top, ctx, c);
                }
            }
        }
    }

    void NnueState::pop() {
        assert(m_top > &m_accumulatorStack[0]);
        --m_top;
    }

    i32 NnueState::evaluate(const Position& pos, Color stm) {
        assert(m_network);
        assert(m_top >= &m_accumulatorStack[0] && m_top <= &m_accumulatorStack.back());
        assert(stm != Colors::kNone);

        ensureUpToDate(pos);

        if constexpr (InputFeatureSet::kThreatInputs) {
            return evaluateNetwork(*m_network, m_top->psqAcc, &m_top->threatAcc[0], pos, stm);
        } else {
            return evaluateNetwork(*m_network, m_top->psqAcc, nullptr, pos, stm);
        }
    }

    i32 NnueState::evaluateOnce(const Position& pos, Color stm) {
        assert(stm != Colors::kNone);

        const auto& network = *getNetwork(0);

        Accumulator psqAccumulator{};

        psqAccumulator.initBoth(network.featureTransformer());

        resetPsqAccumulator(network, psqAccumulator, Colors::kBlack, pos);
        resetPsqAccumulator(network, psqAccumulator, Colors::kWhite, pos);

        if constexpr (InputFeatureSet::kThreatInputs) {
            Accumulator threatAccumulator{};

            resetThreatAccumulator(network, threatAccumulator, Colors::kBlack, pos);
            resetThreatAccumulator(network, threatAccumulator, Colors::kWhite, pos);

            return evaluateNetwork(network, psqAccumulator, &threatAccumulator, pos, stm);
        } else {
            return evaluateNetwork(network, psqAccumulator, nullptr, pos, stm);
        }
    }

    void NnueState::ensureUpToDate(const Position& pos) {
        assert(m_network);

        for (const auto c : {Colors::kBlack, Colors::kWhite}) {
            if (!m_top->isPsqDirty(c)) {
                continue;
            }

            // if the current accumulator needs a refresh, just do it
            if (m_top->ctx.updates.requiresPsqRefresh(c)) {
                refreshPsqAccumulator(*m_network, *m_top, c, pos, m_refreshTable);
                continue;
            }

            // scan back to the last non-dirty accumulator, or an accumulator that requires a refresh.
            // root accumulator is always up-to-date
            auto* curr = m_top - 1;
            for (; curr->isPsqDirty(c) && !curr->ctx.updates.requiresPsqRefresh(c); --curr) {}

            assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresPsqRefresh(c));

            // if the found accumulator requires a refresh, just give up and refresh the current one
            if (curr->ctx.updates.requiresPsqRefresh(c)) {
                refreshPsqAccumulator(*m_network, *m_top, c, pos, m_refreshTable);
            } else {
                // otherwise go forward and incrementally update all accumulators in between
                do {
                    const auto& prev = *curr++;
                    updatePsq(*m_network, prev.psqAcc, *curr, curr->ctx, c);
                } while (curr != m_top);
            }
        }

        if constexpr (InputFeatureSet::kThreatInputs) {
            // same logic as above
            for (const auto c : {Colors::kBlack, Colors::kWhite}) {
                if (!m_top->isThreatDirty(c)) {
                    continue;
                }

                if (m_top->ctx.updates.requiresThreatRefresh(c)) {
                    refreshThreatAccumulator(*m_network, *m_top, c, pos);
                    continue;
                }

                auto* curr = m_top - 1;
                for (; curr->isThreatDirty(c) && !curr->ctx.updates.requiresThreatRefresh(c); --curr) {}

                assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresThreatRefresh(c));

                if (curr->ctx.updates.requiresThreatRefresh(c)) {
                    refreshThreatAccumulator(*m_network, *m_top, c, pos);
                } else {
                    do {
                        const auto& prev = *curr++;
                        curr->threatAcc[0].copyFrom(c, prev.threatAcc[0]);
                        applyThreatUpdates(*m_network, *curr, curr->ctx, c);
                    } while (curr != m_top);
                }
            }
        }
    }
} // namespace stormphrax::eval
