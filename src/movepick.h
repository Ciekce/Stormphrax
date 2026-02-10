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

#include <limits>

#include "history.h"
#include "movegen.h"
#include "see.h"
#include "stats.h"
#include "tunable.h"

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
        [[nodiscard]] inline Move next() {
            using namespace tunable;

            switch (m_stage) {
                case MovegenStage::kTtMove: {
                    ++m_stage;

                    if (m_ttMove && m_pos.isPseudolegal(m_ttMove)) {
                        return m_ttMove;
                    }

                    [[fallthrough]];
                }

                case MovegenStage::kGenNoisy: {
                    generateNoisy(m_data.moves, m_pos);
                    m_end = m_data.moves.size();
                    scoreNoisies();

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kGoodNoisy: {
                    while (m_idx < m_end) {
                        const auto idx = findNext();
                        const auto [move, score] = m_data.moves[idx];

                        if (move == m_ttMove) {
                            continue;
                        }

                        const auto threshold = -score / 4 + goodNoisySeeOffset();

                        if (!see::see(m_pos, move, threshold)) {
                            m_data.moves[m_badNoisyEnd++] = m_data.moves[idx];
                        } else {
                            return move;
                        }
                    }

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kKiller: {
                    ++m_stage;

                    if (!m_skipQuiets && m_killers.killer && m_killers.killer != m_ttMove
                        && m_pos.isPseudolegal(m_killers.killer))
                    {
                        return m_killers.killer;
                    }

                    [[fallthrough]];
                }

                case MovegenStage::kGenQuiet: {
                    if (!m_skipQuiets) {
                        generateQuiet(m_data.moves, m_pos);
                        m_end = m_data.moves.size();
                        scoreQuiets();
                    }

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kQuiet: {
                    if (!m_skipQuiets) {
                        if (const auto move = selectNext([this](auto move) { return !isSpecial(move); })) {
                            return move;
                        }
                    }

                    m_idx = 0;
                    m_end = m_badNoisyEnd;

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kBadNoisy: {
                    if (const auto move = selectNext<false>([this](auto move) { return move != m_ttMove; })) {
                        return move;
                    }

                    m_stage = MovegenStage::kEnd;
                    return kNullMove;
                }

                case MovegenStage::kQsearchTtMove: {
                    ++m_stage;

                    if (m_ttMove && m_pos.isPseudolegal(m_ttMove)) {
                        return m_ttMove;
                    }

                    [[fallthrough]];
                }

                case MovegenStage::kQsearchGenNoisy: {
                    generateNoisy(m_data.moves, m_pos);
                    m_end = m_data.moves.size();
                    scoreNoisies();

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kQsearchNoisy: {
                    if (const auto move = selectNext([this](auto move) { return move != m_ttMove; })) {
                        return move;
                    }

                    m_stage = MovegenStage::kEnd;
                    return kNullMove;
                }

                case MovegenStage::kQsearchEvasionsTtMove: {
                    ++m_stage;

                    if (m_ttMove && m_pos.isPseudolegal(m_ttMove)) {
                        return m_ttMove;
                    }

                    [[fallthrough]];
                }

                case MovegenStage::kQsearchEvasionsGenNoisy: {
                    generateNoisy(m_data.moves, m_pos);
                    m_end = m_data.moves.size();
                    scoreNoisies();

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kQsearchEvasionsNoisy: {
                    if (const auto move = selectNext([this](auto move) { return move != m_ttMove; })) {
                        return move;
                    }

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kQsearchEvasionsGenQuiet: {
                    if (!m_skipQuiets) {
                        generateQuiet(m_data.moves, m_pos);
                        m_end = m_data.moves.size();
                        scoreQuiets();
                    }

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kQsearchEvasionsQuiet: {
                    if (!m_skipQuiets) {
                        if (const auto move = selectNext([this](auto move) { return move != m_ttMove; })) {
                            return move;
                        }
                    }

                    m_stage = MovegenStage::kEnd;
                    return kNullMove;
                }

                case MovegenStage::kProbcutTtMove: {
                    ++m_stage;

                    if (m_ttMove && m_pos.isPseudolegal(m_ttMove)) {
                        return m_ttMove;
                    }

                    [[fallthrough]];
                }

                case MovegenStage::kProbcutGenNoisy: {
                    generateNoisy(m_data.moves, m_pos);
                    m_end = m_data.moves.size();
                    scoreNoisies();

                    ++m_stage;
                    [[fallthrough]];
                }

                case MovegenStage::kProbcutNoisy: {
                    if (const auto move = selectNext([this](auto move) { return move != m_ttMove; })) {
                        return move;
                    }

                    m_stage = MovegenStage::kEnd;
                    return kNullMove;
                }

                default:
                    return kNullMove;
            }
        }

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
            i32 ply
        ) {
            const auto stage = pos.isCheck() ? MovegenStage::kQsearchEvasionsTtMove : MovegenStage::kQsearchTtMove;
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

        inline void scoreNoisy(ScoredMove& scoredMove) {
            const auto move = scoredMove.move;
            auto& score = scoredMove.score;

            const auto captured = m_pos.captureTarget(move);

            score += m_history.noisyScore(move, captured, m_pos.threats()) / 8;
            score += see::value(captured);

            if (move.type() == MoveType::kPromotion) {
                score += see::value(PieceTypes::kQueen) - see::value(PieceTypes::kPawn);
            }
        }

        inline void scoreNoisies() {
            for (u32 i = m_idx; i < m_end; ++i) {
                scoreNoisy(m_data.moves[i]);
            }
        }

        inline void scoreQuiet(ScoredMove& move) {
            move.score = m_history.quietScore(
                m_continuations,
                m_ply,
                m_pos.threats(),
                m_pos.boards().pieceOn(move.move.fromSq()),
                move.move
            );
        }

        inline void scoreQuiets() {
            for (u32 i = m_idx; i < m_end; ++i) {
                scoreQuiet(m_data.moves[i]);
            }
        }

        [[nodiscard]] inline u32 findNext() {
            const auto toU64 = [](i32 s) {
                i64 widened = s;
                widened -= std::numeric_limits<i32>::min();
                return static_cast<u64>(widened) << 32;
            };

            auto best = toU64(m_data.moves[m_idx].score) | (256 - m_idx);
            for (auto i = m_idx + 1; i < m_end; ++i) {
                const auto curr = toU64(m_data.moves[i].score) | (256 - i);
                best = std::max(best, curr);
            }
            const auto bestIdx = 256 - (best & 0xFFFFFFFF);
            if (bestIdx != m_idx) {
                std::swap(m_data.moves[m_idx], m_data.moves[bestIdx]);
            }

            return m_idx++;
        }

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
