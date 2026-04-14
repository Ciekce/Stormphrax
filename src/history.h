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

    [[nodiscard]] inline HistoryScore getConthist(
        std::span<ContinuationSubtable* const> continuations,
        i32 ply,
        Piece moving,
        Move move,
        i32 offset
    ) {
        if (offset <= ply) {
            return (*continuations[ply - offset])[{moving, move}];
        }

        return 0;
    }

    class HistoryTables {
    public:
        inline void clear() {
            std::memset(&m_butterfly, 0, sizeof(m_butterfly));
            std::memset(&m_pieceTo, 0, sizeof(m_pieceTo));
            std::memset(&m_continuation, 0, sizeof(m_continuation));
            std::memset(&m_noisy, 0, sizeof(m_noisy));
        }

        [[nodiscard]] inline const ContinuationSubtable& contTable(Piece moving, Square to) const {
            return m_continuation[moving.idx()][to.idx()];
        }

        [[nodiscard]] inline ContinuationSubtable& contTable(Piece moving, Square to) {
            return m_continuation[moving.idx()][to.idx()];
        }

        inline void updateMainHistory(
            Bitboard threats,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            butterflyEntry(threats, move).update(bonus);
            pieceToEntry(threats, moving, move).update(bonus);
        }

        inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Bitboard threats,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            using namespace tunable;

            i32 base = 0;

            base += getButterfly(threats, move) * contBaseButterflyWeight();
            base += getPieceTo(threats, moving, move) * contBasePieceToWeight();

            base += getConthist(continuations, ply, moving, move, 1) * contBaseCont1Weight();
            base += getConthist(continuations, ply, moving, move, 2) * contBaseCont2Weight();
            base += getConthist(continuations, ply, moving, move, 4) * contBaseCont4Weight();

            base /= 1024;

            updateConthist(continuations, ply, moving, move, base, bonus, 1);
            updateConthist(continuations, ply, moving, move, base, bonus, 2);
            updateConthist(continuations, ply, moving, move, base, bonus, 4);
        }

        inline void updateQuietScore(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Bitboard threats,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            updateMainHistory(threats, moving, move, bonus);
            updateConthist(continuations, ply, threats, moving, move, bonus);
        }

        inline void updateNoisyScore(Move move, Piece captured, Bitboard threats, HistoryScore bonus) {
            noisyEntry(move, captured, threats[move.toSq()]).update(bonus);
        }

        [[nodiscard]] inline i32 getButterfly(Bitboard threats, Move move) const {
            return butterflyEntry(threats, move);
        }

        [[nodiscard]] inline i32 getPieceTo(Bitboard threats, Piece moving, Move move) const {
            return pieceToEntry(threats, moving, move);
        }

        [[nodiscard]] inline i32 getNoisy(Move move, Piece captured, Bitboard threats) const {
            return noisyEntry(move, captured, threats[move.toSq()]);
        }

    private:
        // [from][to][from attacked][to attacked]
        util::MultiArray<HistoryEntry, Squares::kCount, Squares::kCount, 2, 2> m_butterfly{};
        // [piece][to]
        util::MultiArray<HistoryEntry, Pieces::kCount, Squares::kCount, 2, 2> m_pieceTo{};
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
                (*continuations[ply - offset])[{moving, move}].updateWithBase(bonus, base);
            }
        }

        [[nodiscard]] inline const HistoryEntry& butterflyEntry(Bitboard threats, Move move) const {
            return m_butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& butterflyEntry(Bitboard threats, Move move) {
            return m_butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& pieceToEntry(Bitboard threats, Piece moving, Move move) const {
            return m_pieceTo[moving.idx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& pieceToEntry(Bitboard threats, Piece moving, Move move) {
            return m_pieceTo[moving.idx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& noisyEntry(Move move, Piece captured, bool defended) const {
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][captured.idx()][defended];
        }

        [[nodiscard]] inline HistoryEntry& noisyEntry(Move move, Piece captured, bool defended) {
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][captured.idx()][defended];
        }
    };
} // namespace stormphrax
