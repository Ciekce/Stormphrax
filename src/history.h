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

#pragma once

#include "types.h"

#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "bitboard.h"
#include "move.h"
#include "tunable.h"
#include "util/multi_array.h"

namespace stormphrax {
    using HistoryScore = i16;

    struct HistoryEntry {
        i16 value{};

        HistoryEntry() = default;
        HistoryEntry(HistoryScore v) :
                value{v} {}

        [[nodiscard]] inline operator HistoryScore() const {
            return value;
        }

        [[nodiscard]] inline HistoryEntry& operator=(HistoryScore v) {
            value = v;
            return *this;
        }

        inline void update(HistoryScore bonus) {
            value += bonus - value * std::abs(bonus) / tunable::maxHistory();
        }

        inline void updateWithBase(HistoryScore bonus, i32 base) {
            value += bonus - base * std::abs(bonus) / tunable::maxHistory();
        }
    };

    inline HistoryScore historyBonus(i32 depth) {
        return static_cast<HistoryScore>(std::clamp(
            depth * tunable::historyBonusDepthScale() - tunable::historyBonusOffset(),
            0,
            tunable::maxHistoryBonus()
        ));
    }

    inline HistoryScore historyPenalty(i32 depth) {
        return static_cast<HistoryScore>(-std::clamp(
            depth * tunable::historyPenaltyDepthScale() - tunable::historyPenaltyOffset(),
            0,
            tunable::maxHistoryPenalty()
        ));
    }

    class ContinuationSubtable {
    public:
        //TODO take two args when c++23 is usable
        inline HistoryScore operator[](std::pair<Piece, Move> move) const {
            const auto [piece, mv] = move;
            return m_data[piece.idx()][mv.toSq().idx()];
        }

        inline HistoryEntry& operator[](std::pair<Piece, Move> move) {
            const auto [piece, mv] = move;
            return m_data[piece.idx()][mv.toSq().idx()];
        }

    private:
        // [piece type][to]
        util::MultiArray<HistoryEntry, Pieces::kCount, Squares::kCount> m_data{};
    };

    class HistoryTables {
    public:
        inline void clear() {
            std::memset(&m_main, 0, sizeof(m_main));
            std::memset(&m_mainFactorizer, 0, sizeof(m_mainFactorizer));
            std::memset(&m_continuation, 0, sizeof(m_continuation));
            std::memset(&m_noisy, 0, sizeof(m_noisy));
        }

        [[nodiscard]] inline const ContinuationSubtable& contTable(Piece moving, Square to) const {
            return m_continuation[moving.idx()][to.idx()];
        }

        [[nodiscard]] inline ContinuationSubtable& contTable(Piece moving, Square to) {
            return m_continuation[moving.idx()][to.idx()];
        }

        inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Square kingSq,
            Bitboard threats,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            const auto base =
                getConthist(continuations, ply, moving, move) + getMainHist(kingSq, threats, moving, move) / 2;

            updateConthist(continuations, ply, moving, move, base, bonus, 1);
            updateConthist(continuations, ply, moving, move, base, bonus, 2);
            updateConthist(continuations, ply, moving, move, base, bonus, 4);
        }

        inline void updateQuietScore(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Square kingSq,
            Bitboard threats,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            butterflyEntry(kingSq, threats, move).update(bonus);
            pieceToEntry(kingSq, threats, moving, move).update(bonus);

            butterflyFactorizerEntry(threats, move).update(bonus);
            pieceToFactorizerEntry(threats, moving, move).update(bonus);

            updateConthist(continuations, ply, kingSq, threats, moving, move, bonus);
        }

        inline void updateNoisyScore(Move move, Piece captured, Bitboard threats, HistoryScore bonus) {
            noisyEntry(move, captured, threats[move.toSq()]).update(bonus);
        }

        [[nodiscard]] inline i32 getMainHist(Square kingSq, Bitboard threats, Piece moving, Move move) const {
            const auto main = butterflyEntry(kingSq, threats, move) + pieceToEntry(kingSq, threats, moving, move);
            const auto factorizer =
                butterflyFactorizerEntry(threats, move) + pieceToFactorizerEntry(threats, moving, move);
            return (main + factorizer) / 4;
        }

        [[nodiscard]] inline i32 getConthist(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Piece moving,
            Move move
        ) const {
            i32 score{};

            score += conthistScore(continuations, ply, moving, move, 1);
            score += conthistScore(continuations, ply, moving, move, 2);
            score += conthistScore(continuations, ply, moving, move, 4) / 2;

            return score;
        }

        [[nodiscard]] inline i32 quietScore(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Square kingSq,
            Bitboard threats,
            Piece moving,
            Move move
        ) const {
            i32 score{};

            score += getMainHist(kingSq, threats, moving, move);
            score += getConthist(continuations, ply, moving, move);

            return score;
        }

        [[nodiscard]] inline i32 noisyScore(Move move, Piece captured, Bitboard threats) const {
            return noisyEntry(move, captured, threats[move.toSq()]);
        }

    private:
        struct MainHistory {
            // [from][to][from attacked][to attacked]
            util::MultiArray<HistoryEntry, Squares::kCount, Squares::kCount, 2, 2> butterfly{};
            // [piece][to]
            util::MultiArray<HistoryEntry, Pieces::kCount, Squares::kCount, 2, 2> pieceTo{};
        };

        // [friendly king sq]
        std::array<MainHistory, Squares::kCount> m_main{};
        MainHistory m_mainFactorizer{};

        // [prev piece][to][curr piece type][to]
        util::MultiArray<ContinuationSubtable, Pieces::kCount, Squares::kCount> m_continuation{};

        // [from][to][captured][defended]
        // additional slot for non-capture queen promos
        util::MultiArray<HistoryEntry, Squares::kCount, Squares::kCount, Pieces::kCount + 1, 2> m_noisy{};

        static inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Piece moving,
            Move move,
            i32 base,
            HistoryScore bonus,
            i32 offset
        ) {
            if (offset <= ply) {
                conthistEntry(continuations, ply, offset)[{moving, move}].updateWithBase(bonus, base);
            }
        }

        static inline HistoryScore conthistScore(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Piece moving,
            Move move,
            i32 offset
        ) {
            if (offset <= ply) {
                return conthistEntry(continuations, ply, offset)[{moving, move}];
            }

            return 0;
        }

        [[nodiscard]] inline const HistoryEntry& butterflyEntry(Square kingSq, Bitboard threats, Move move) const {
            return m_main[kingSq.idx()]
                .butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& butterflyEntry(Square kingSq, Bitboard threats, Move move) {
            return m_main[kingSq.idx()]
                .butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& pieceToEntry(
            Square kingSq,
            Bitboard threats,
            Piece moving,
            Move move
        ) const {
            return m_main[kingSq.idx()]
                .pieceTo[moving.idx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& pieceToEntry(Square kingSq, Bitboard threats, Piece moving, Move move) {
            return m_main[kingSq.idx()]
                .pieceTo[moving.idx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& butterflyFactorizerEntry(Bitboard threats, Move move) const {
            return m_mainFactorizer
                .butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& butterflyFactorizerEntry(Bitboard threats, Move move) {
            return m_mainFactorizer
                .butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& pieceToFactorizerEntry(
            Bitboard threats,
            Piece moving,
            Move move
        ) const {
            return m_mainFactorizer.pieceTo[moving.idx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& pieceToFactorizerEntry(Bitboard threats, Piece moving, Move move) {
            return m_mainFactorizer.pieceTo[moving.idx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] static inline const ContinuationSubtable& conthistEntry(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            i32 offset
        ) {
            return *continuations[ply - offset];
        }

        [[nodiscard]] static inline ContinuationSubtable& conthistEntry(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            i32 offset
        ) {
            return *continuations[ply - offset];
        }

        [[nodiscard]] inline const HistoryEntry& noisyEntry(Move move, Piece captured, bool defended) const {
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][captured.idx()][defended];
        }

        [[nodiscard]] inline HistoryEntry& noisyEntry(Move move, Piece captured, bool defended) {
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][captured.idx()][defended];
        }
    };
} // namespace stormphrax
