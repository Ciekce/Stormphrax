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

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <numeric>

#include "../attacks/attacks.h"
#include "../util/static_vector.h"

#include "nnue.h"

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

            if (addCount == 1 && subCount == 1) { // regular non-capture
                const auto [subPiece, subSquare] = ctx.updates.sub[0];
                const auto [addPiece, addSquare] = ctx.updates.add[0];

                const auto sub = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece, subSquare, king);
                const auto add = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece, addSquare, king);

                curr.psqAcc.subAddFrom(prev, network.featureTransformer(), c, sub, add);
            } else if (addCount == 1 && subCount == 2) { // any capture
                const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
                const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
                const auto [addPiece, addSquare] = ctx.updates.add[0];

                const auto sub0 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece0, subSquare0, king);
                const auto sub1 = nnue::features::psq::featureIndex<InputFeatureSet>(c, subPiece1, subSquare1, king);
                const auto add = nnue::features::psq::featureIndex<InputFeatureSet>(c, addPiece, addSquare, king);

                curr.psqAcc.subSubAddFrom(prev, network.featureTransformer(), c, sub0, sub1, add);
            } else if (addCount == 2 && subCount == 2) { // castling
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

        template <
            bool kZeroInit = false,
            typename AddIndices = std::span<const u16>,
            typename SubIndices = std::span<const u16>>
        SP_ALWAYS_INLINE_NDEBUG inline void applyThreatRows(
            std::span<i16, kL1Size> acc,
            const FeatureTransformer& ft,
            const AddIndices& addIndices,
            const SubIndices& subIndices
        ) {
            namespace simd = util::simd;

            static constexpr usize kChunk = simd::kChunkSize<i16>;
            static_assert(kL1Size % kChunk == 0);

            static constexpr usize kAccChunks = kL1Size / kChunk;

#if SP_HAS_AVX512
            static constexpr usize kTileTarget = 32;
#else
            static constexpr usize kTileTarget = 8;
#endif

            static constexpr usize kTile = kAccChunks < kTileTarget ? kAccChunks : kTileTarget;

            static_assert(kAccChunks % kTile == 0);

            for (usize base = 0; base < kAccChunks; base += kTile) {
                std::array<simd::Vector<i16>, kTile> v;

                for (usize t = 0; t < kTile; ++t) {
                    if constexpr (kZeroInit) {
                        v[t] = simd::zero<i16>();
                    } else {
                        v[t] = simd::load<i16>(&acc[(base + t) * kChunk]);
                    }
                }

                for (const auto index : subIndices) {
                    const auto* sub = ft.threatWeightPtr(index);
                    for (usize t = 0; t < kTile; ++t) {
                        v[t] = simd::sub<i16>(v[t], simd::widenLoadI8ToI16(&sub[(base + t) * kChunk]));
                    }
                }

                for (const auto index : addIndices) {
                    const auto* add = ft.threatWeightPtr(index);
                    for (usize t = 0; t < kTile; ++t) {
                        v[t] = simd::add<i16>(v[t], simd::widenLoadI8ToI16(&add[(base + t) * kChunk]));
                    }
                }

                for (usize t = 0; t < kTile; ++t) {
                    simd::store<i16>(&acc[(base + t) * kChunk], v[t]);
                }
            }
        }

#if SP_HAS_VBMI2
        SP_ALWAYS_INLINE_NDEBUG inline __m512i ppIdxEpi16(__m512i a, __m512i b) {
            const auto hi = _mm512_max_epu16(a, b);
            const auto lo = _mm512_min_epu16(a, b);
            const auto prod = _mm512_mullo_epi16(hi, _mm512_sub_epi16(hi, _mm512_set1_epi16(1)));
            return _mm512_add_epi16(_mm512_srli_epi16(prod, 1), lo);
        }

        SP_ALWAYS_INLINE_NDEBUG inline __m256i ppIdxEpi16(__m256i a, __m256i b) {
            const auto hi = _mm256_max_epu16(a, b);
            const auto lo = _mm256_min_epu16(a, b);
            const auto prod = _mm256_mullo_epi16(hi, _mm256_sub_epi16(hi, _mm256_set1_epi16(1)));
            return _mm256_add_epi16(_mm256_srli_epi16(prod, 1), lo);
        }
#endif

        template <typename AddList, typename SubList>
        SP_ALWAYS_INLINE_NDEBUG inline void generatePpRows(
            Color c,
            Square kingSq,
            Bitboard blackBefore,
            Bitboard whiteBefore,
            Bitboard blackAfter,
            Bitboard whiteAfter,
            AddList& addIndices,
            SubList& subIndices
        ) {
            using namespace nnue::features::threats;

            u16* const addIdx = &*addIndices.end();
            u16* const subIdx = &*subIndices.end();

#if SP_HAS_VBMI2
            const u8 sqMask = (c == Colors::kBlack ? 0b111000 : 0) ^ (kingSq.file() >= kFileE ? 0b000111 : 0);

            const auto friendlyBefore = c == Colors::kBlack ? blackBefore : whiteBefore;
            const auto friendlyAfter = c == Colors::kBlack ? blackAfter : whiteAfter;

            const u64 afterAll = blackAfter | whiteAfter;

            const auto addedAll = (blackAfter & ~blackBefore) | (whiteAfter & ~whiteBefore);
            const auto removedAll = (blackBefore & ~blackAfter) | (whiteBefore & ~whiteAfter);

            const u64 unchBb = afterAll & ~addedAll;

            static constexpr auto kIota = [] {
                std::array<u8, Squares::kCount> table{};
                std::iota(table.begin(), table.end(), 0);
                return table;
            }();

            const auto iota = _mm512_loadu_si512(kIota.data());
            const auto adjusted =
                _mm512_sub_epi8(_mm512_xor_si512(iota, _mm512_set1_epi8(sqMask)), _mm512_set1_epi8(8));
            const auto ids =
                _mm512_mask_blend_epi8(friendlyAfter, _mm512_add_epi8(adjusted, _mm512_set1_epi8(48)), adjusted);

            const auto compressed = _mm512_maskz_compress_epi8(unchBb, ids);
            const auto idsU16 = _mm256_cvtepu8_epi16(_mm512_castsi512_si128(compressed));
            const auto unchDoubled = _mm512_broadcast_i64x4(idsU16);

            const auto unchCount = std::popcount(unchBb);
            const u16 unchMask = (1 << unchCount) - 1;

            const auto pawnIdFor = [&](Square sq, bool enemy) -> u16 {
                return (sq.idx() ^ sqMask) - 8 + (enemy ? 48 : 0);
            };

            const auto bandMask = [&](Square sq) -> u16 { return _pext_u64(kPpMasks[sq.idx()] & unchBb, unchBb); };

            const auto ppIdx = [](u16 a, u16 b) -> u16 {
                const auto hi = std::max(a, b);
                const auto lo = std::min(a, b);
                return hi * (hi - 1) / 2 + lo;
            };

            const auto nRemoved = removedAll.popcount();
            assert(nRemoved > 0);

            auto remaining = removedAll;

            const auto r0sq = remaining.popLowestSquare();
            const auto r1sq = nRemoved >= 2 ? remaining.popLowestSquare() : Squares::kNone;

            const auto r0id = pawnIdFor(r0sq, !friendlyBefore.hasSq(r0sq));
            const auto r1id = nRemoved >= 2 ? pawnIdFor(r1sq, !friendlyBefore.hasSq(r1sq)) : u16{};

            const auto r0mask = unchMask & bandMask(r0sq);
            const auto r1mask = nRemoved >= 2 ? unchMask & bandMask(r1sq) : 0;

            const u32 rMask = r0mask | r1mask << 16;

            const auto rv =
                _mm512_insertf64x4(_mm512_castsi256_si512(_mm256_set1_epi16(r0id)), _mm256_set1_epi16(r1id), 1);
            _mm512_storeu_epi16(subIdx, _mm512_maskz_compress_epi16(rMask, ppIdxEpi16(rv, unchDoubled)));
            usize nSub = std::popcount(rMask);

            if (nRemoved >= 2) {
                assert(kPpMasks[r0sq.idx()].hasSq(r1sq));
                subIdx[nSub++] = ppIdx(r0id, r1id);
            }

            usize nAdd = 0;
            if (!addedAll.empty()) {
                const auto aSq = addedAll.lowestSquare();

                const u16 aid = pawnIdFor(aSq, !friendlyAfter.hasSq(aSq));
                const u16 aMask = unchMask & bandMask(aSq);

                const auto ai = ppIdxEpi16(_mm256_set1_epi16(aid), idsU16);

                _mm256_storeu_epi16(addIdx, _mm256_maskz_compress_epi16(aMask, ai));
                nAdd = std::popcount(aMask);
            }

            addIndices.resize(addIndices.size() + nAdd);
            subIndices.resize(subIndices.size() + nSub);
#else
            auto beforeRemaining = blackBefore | whiteBefore;
            auto afterRemaining = blackAfter | whiteAfter;

            const auto added = std::array{blackAfter & ~blackBefore, whiteAfter & ~whiteBefore};
            const auto removed = std::array{blackBefore & ~blackAfter, whiteBefore & ~whiteAfter};

            usize nAdd = 0;
            usize nSub = 0;

            for (const auto pawnColor : {Colors::kBlack, Colors::kWhite}) {
                for (const auto a : added[pawnColor.idx()]) {
                    afterRemaining &= ~a.bit();

                    const auto mask = kPpMasks[a.idx()] & afterRemaining;

                    for (const auto b : blackAfter & mask) {
                        addIdx[nAdd++] = ppFeatureIndex(c, kingSq, pawnColor, a, Colors::kBlack, b);
                    }

                    for (const auto b : whiteAfter & mask) {
                        addIdx[nAdd++] = ppFeatureIndex(c, kingSq, pawnColor, a, Colors::kWhite, b);
                    }
                }

                for (const auto a : removed[pawnColor.idx()]) {
                    beforeRemaining &= ~a.bit();

                    const auto mask = kPpMasks[a.idx()] & beforeRemaining;

                    for (const auto b : blackBefore & mask) {
                        subIdx[nSub++] = ppFeatureIndex(c, kingSq, pawnColor, a, Colors::kBlack, b);
                    }

                    for (const auto b : whiteBefore & mask) {
                        subIdx[nSub++] = ppFeatureIndex(c, kingSq, pawnColor, a, Colors::kWhite, b);
                    }
                }
            }

            addIndices.resize(addIndices.size() + nAdd);
            subIndices.resize(subIndices.size() + nSub);
#endif
        }

        void addThreatFeatures(const Network& network, std::span<i16, kL1Size> acc, Color c, const Position& pos) {
            using namespace nnue::features::threats;

            const auto& ft = network.featureTransformer();
            const auto kingSq = pos.king(c);

            StaticVector<u16, 256> indices;

            const auto occ = pos.occ();
            const auto kings = pos.bb(PieceTypes::kKing);

            for (const auto from : occ & ~kings) {
                const auto piece = pos.pieceOn(from);
                for (const auto to : occ & attacks::getAttacks(piece, from, occ) & ~kings) {
                    const auto attacked = pos.pieceOn(to);
                    const auto feature = threatFeatureIndex(c, kingSq, piece, from, attacked, to);
                    indices.pushIf(static_cast<u16>(feature), feature >= 0);
                }
            }

            if constexpr (InputFeatureSet::kPawnPawnInputs) {
                const auto ourPawns = pos.bb(PieceTypes::kPawn, c);
                const auto theirPawns = pos.bb(PieceTypes::kPawn, c.flip());

                for (const auto [a, remaining] : ourPawns.iterWithRemaining()) {
                    const auto mask = kPpMasks[a.idx()];

                    for (const auto b : remaining & mask) {
                        indices.push(ppFeatureIndex(c, kingSq, c, a, c, b));
                    }

                    for (const auto b : theirPawns & mask) {
                        indices.push(ppFeatureIndex(c, kingSq, c, a, c.flip(), b));
                    }
                }

                for (const auto [a, remaining] : theirPawns.iterWithRemaining()) {
                    const auto mask = kPpMasks[a.idx()];
                    for (const auto b : remaining & mask) {
                        indices.push(ppFeatureIndex(c, kingSq, c.flip(), a, c.flip(), b));
                    }
                }
            }

            applyThreatRows<true>(acc, ft, indices, std::span<const u16>{});
        }

        void applyThreatUpdates(const Network& network, UpdatableAccumulator& curr, const UpdateContext& ctx, Color c) {
            assert(!ctx.updates.requiresThreatRefresh(c));

            using namespace nnue::features::threats;

            const auto kingSq = ctx.kings.color(c);
            const auto& ft = network.featureTransformer();

            auto acc = curr.threatAcc[0].forColor(c);

            StaticVector<u16, kMaxThreatsAdded + 16 * InputFeatureSet::kPawnPawnInputs> addIndices;
            StaticVector<u16, kMaxThreatsRemoved + 40 * InputFeatureSet::kPawnPawnInputs> subIndices;

            for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsAdded) {
                const auto feature = threatFeatureIndex(c, kingSq, attacker, attackerSq, attacked, attackedSq);
                addIndices.pushIf(static_cast<u16>(feature), feature >= 0);
            }

            for (const auto [attacker, attackerSq, attacked, attackedSq] : ctx.updates.threatsRemoved) {
                const auto feature = threatFeatureIndex(c, kingSq, attacker, attackerSq, attacked, attackedSq);
                subIndices.pushIf(static_cast<u16>(feature), feature >= 0);
            }

            if constexpr (InputFeatureSet::kPawnPawnInputs) {
                const auto blackBefore = ctx.updates.pawnBbsBefore[Colors::kBlack.idx()];
                const auto blackAfter = ctx.updates.pawnBbsAfter[Colors::kBlack.idx()];

                const auto whiteBefore = ctx.updates.pawnBbsBefore[Colors::kWhite.idx()];
                const auto whiteAfter = ctx.updates.pawnBbsAfter[Colors::kWhite.idx()];

                if (blackBefore != blackAfter || whiteBefore != whiteAfter) {
                    generatePpRows(c, kingSq, blackBefore, whiteBefore, blackAfter, whiteAfter, addIndices, subIndices);
                }
            }

            applyThreatRows(acc, ft, addIndices, subIndices);

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
