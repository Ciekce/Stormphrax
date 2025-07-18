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
            return m_data[static_cast<i32>(piece)][static_cast<i32>(mv.toSq())];
        }

        inline HistoryEntry& operator[](std::pair<Piece, Move> move) {
            const auto [piece, mv] = move;
            return m_data[static_cast<i32>(piece)][static_cast<i32>(mv.toSq())];
        }

    private:
        // [piece type][to]
        util::MultiArray<HistoryEntry, 12, 64> m_data{};
    };

    class HistoryTables {
    public:
        inline void clear() {
            std::memset(&m_butterfly, 0, sizeof(m_butterfly));
            std::memset(&m_pieceTo, 0, sizeof(m_pieceTo));
            std::memset(&m_pawn, 0, sizeof(m_pawn));
            std::memset(&m_continuation, 0, sizeof(m_continuation));
            std::memset(&m_noisy, 0, sizeof(m_noisy));
        }

        [[nodiscard]] inline const ContinuationSubtable& contTable(Piece moving, Square to) const {
            return m_continuation[static_cast<i32>(moving)][static_cast<i32>(to)];
        }

        [[nodiscard]] inline ContinuationSubtable& contTable(Piece moving, Square to) {
            return m_continuation[static_cast<i32>(moving)][static_cast<i32>(to)];
        }

        inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            updateConthist(continuations, ply, moving, move, bonus, 1);
            updateConthist(continuations, ply, moving, move, bonus, 2);
            updateConthist(continuations, ply, moving, move, bonus, 4);
        }

        inline void updateQuietScore(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Bitboard threats,
            u64 pawnKey,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            butterflyEntry(threats, move).update(bonus);
            pieceToEntry(threats, moving, move).update(bonus);
            pawnEntry(pawnKey, moving, move).update(bonus);
            updateConthist(continuations, ply, moving, move, bonus);
        }

        inline void updateNoisyScore(Move move, Piece captured, Bitboard threats, HistoryScore bonus) {
            noisyEntry(move, captured, threats[move.toSq()]).update(bonus);
        }

        [[nodiscard]] inline i32 quietScore(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Bitboard threats,
            Piece moving,
            Move move
        ) const {
            i32 score{};

            score += (butterflyEntry(threats, move) + pieceToEntry(threats, moving, move)) / 2;

            score += conthistScore(continuations, ply, moving, move, 1);
            score += conthistScore(continuations, ply, moving, move, 2);
            score += conthistScore(continuations, ply, moving, move, 4) / 2;

            return score;
        }

        [[nodiscard]] inline i32 quietOrderingScore(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Bitboard threats,
            u64 pawnKey,
            Piece moving,
            Move move
        ) const {
            auto score = quietScore(continuations, ply, threats, moving, move);

            score += pawnEntry(pawnKey, moving, move) / 2;

            return score;
        }

        [[nodiscard]] inline i32 noisyScore(Move move, Piece captured, Bitboard threats) const {
            return noisyEntry(move, captured, threats[move.toSq()]);
        }

    private:
        static constexpr usize kPawnBits = 9;
        static constexpr usize kPawnSize = 1 << kPawnBits;

        // [from][to][from attacked][to attacked]
        util::MultiArray<HistoryEntry, 64, 64, 2, 2> m_butterfly{};
        // [piece][to]
        util::MultiArray<HistoryEntry, 12, 64, 2, 2> m_pieceTo{};
        // [pawn key][piece][to]
        util::MultiArray<HistoryEntry, kPawnSize, 12, 64> m_pawn;
        // [prev piece][to][curr piece type][to]
        util::MultiArray<ContinuationSubtable, 12, 64> m_continuation{};

        // [from][to][captured][defended]
        // additional slot for non-capture queen promos
        util::MultiArray<HistoryEntry, 64, 64, 13, 2> m_noisy{};

        static inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Piece moving,
            Move move,
            HistoryScore bonus,
            i32 offset
        ) {
            if (offset <= ply) {
                conthistEntry(continuations, ply, offset)[{moving, move}].update(bonus);
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

        [[nodiscard]] inline const HistoryEntry& butterflyEntry(Bitboard threats, Move move) const {
            return m_butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& butterflyEntry(Bitboard threats, Move move) {
            return m_butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& pieceToEntry(Bitboard threats, Piece moving, Move move) const {
            return m_pieceTo[static_cast<i32>(moving)][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& pieceToEntry(Bitboard threats, Piece moving, Move move) {
            return m_pieceTo[static_cast<i32>(moving)][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& pawnEntry(u64 pawnKey, Piece moving, Move move) const {
            return m_pawn[pawnKey % kPawnSize][static_cast<i32>(moving)][move.toSqIdx()];
        }

        [[nodiscard]] inline HistoryEntry& pawnEntry(u64 pawnKey, Piece moving, Move move) {
            return m_pawn[pawnKey % kPawnSize][static_cast<i32>(moving)][move.toSqIdx()];
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
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][static_cast<i32>(captured)][defended];
        }

        [[nodiscard]] inline HistoryEntry& noisyEntry(Move move, Piece captured, bool defended) {
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][static_cast<i32>(captured)][defended];
        }
    };
} // namespace stormphrax
