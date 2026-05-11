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

#include "history.h"
#include "movegen.h"

namespace stormphrax {
    struct KillerTable {
        Move killer{};

        inline void push(Move move) {
            killer = move;
        }

        inline void clear() {
            killer = kNullMove;
        }
    };

    struct MovegenData {
        ScoredMoveList moves;
    };

    enum class MovegenStage : i32 {
        kTtMove = 0,
        kGenNoisy,
        kGoodNoisy,
        kKiller,
        kGenQuiet,
        kQuiet,
        kBadNoisy,
        kQsearchTtMove,
        kQsearchGenNoisy,
        kQsearchNoisy,
        kQsearchEvasionsTtMove,
        kQsearchEvasionsGenNoisy,
        kQsearchEvasionsNoisy,
        kQsearchEvasionsGenQuiet,
        kQsearchEvasionsQuiet,
        kProbcutTtMove,
        kProbcutGenNoisy,
        kProbcutNoisy,
        kEnd,
    };

    inline MovegenStage& operator++(MovegenStage& v) {
        v = static_cast<MovegenStage>(static_cast<i32>(v) + 1);
        return v;
    }

    inline std::strong_ordering operator<=>(MovegenStage a, MovegenStage b) {
        return static_cast<i32>(a) <=> static_cast<i32>(b);
    }

    class MoveGenerator {
    public:
        [[nodiscard]] Move next();

        inline void skipQuiets() {
            m_skipQuiets = true;
        }

        [[nodiscard]] inline MovegenStage stage() const {
            return m_stage;
        }

        [[nodiscard]] static inline MoveGenerator main(
            const Position& pos,
            MovegenData& data,
            Move ttMove,
            const KillerTable& killers,
            const HistoryTables& history,
            std::span<ContinuationSubtable* const> continuations,
            i32 ply
        ) {
            return MoveGenerator{MovegenStage::kTtMove, pos, data, ttMove, &killers, history, continuations, ply};
        }

        [[nodiscard]] static inline MoveGenerator qsearch(
            const Position& pos,
            MovegenData& data,
            Move ttMove,
            const HistoryTables& history,
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            bool forceEvasions
        ) {
            const auto stage =
                forceEvasions || pos.isCheck() ? MovegenStage::kQsearchEvasionsTtMove : MovegenStage::kQsearchTtMove;
            return MoveGenerator{stage, pos, data, ttMove, nullptr, history, continuations, ply};
        }

        [[nodiscard]] static inline MoveGenerator probcut(
            const Position& pos,
            Move ttMove,
            MovegenData& data,
            const HistoryTables& history
        ) {
            return MoveGenerator{MovegenStage::kProbcutTtMove, pos, data, ttMove, nullptr, history, {}, 0};
        }

    private:
        explicit MoveGenerator(
            MovegenStage initialStage,
            const Position& pos,
            MovegenData& data,
            Move ttMove,
            const KillerTable* killers,
            const HistoryTables& history,
            std::span<ContinuationSubtable* const> continuations,
            i32 ply
        ) :
                m_stage{initialStage},
                m_pos{pos},
                m_data{data},
                m_ttMove{ttMove},
                m_history{history},
                m_continuations{continuations},
                m_ply{ply} {
            if (killers) {
                m_killers = *killers;
            }
            m_data.moves.clear();
        }

        void scoreNoisies();
        void scoreQuiets();

        [[nodiscard]] u32 findNext();

        template <bool kSort = true>
        [[nodiscard]] inline Move selectNext(auto predicate) {
            while (m_idx < m_end) {
                const auto idx = kSort ? findNext() : m_idx++;
                const auto move = m_data.moves[idx].move;

                if (predicate(move)) {
                    return move;
                }
            }

            return kNullMove;
        }

        [[nodiscard]] inline bool isSpecial(Move move) {
            return move == m_ttMove || move == m_killers.killer;
        }

        MovegenStage m_stage;

        const Position& m_pos;
        MovegenData& m_data;

        Move m_ttMove;

        KillerTable m_killers{};
        const HistoryTables& m_history;

        std::span<ContinuationSubtable* const> m_continuations;
        i32 m_ply{};

        bool m_skipQuiets{false};

        u32 m_idx{};
        u32 m_end{};

        u32 m_badNoisyEnd{};
    };
} // namespace stormphrax
