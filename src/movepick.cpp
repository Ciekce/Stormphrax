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

#include "movepick.h"

#include <limits>

#include "see.h"
#include "tunable.h"

namespace stormphrax {
    Move MoveGenerator::next() {
        using namespace tunable;

        switch (m_stage) {
            case MovegenStage::kTtMove: {
                ++m_stage;

                if (m_ttMove && m_pos.isLegal(m_ttMove)) {
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
                    && m_pos.isLegal(m_killers.killer))
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

                if (m_ttMove && m_pos.isLegal(m_ttMove)) {
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

                if (m_ttMove && m_pos.isLegal(m_ttMove)) {
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

                if (m_ttMove && m_pos.isLegal(m_ttMove)) {
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

    void MoveGenerator::scoreNoisies() {
        for (u32 i = m_idx; i < m_end; ++i) {
            auto& scoredMove = m_data.moves[i];

            const auto move = scoredMove.move;
            auto& score = scoredMove.score;

            const auto captured = m_pos.captureTarget(move);

            score += m_history.getNoisy(move, captured, m_pos.threats()) / 8;
            score += see::value(captured);

            if (move.type() == MoveType::kPromotion) {
                score += see::value(PieceTypes::kQueen) - see::value(PieceTypes::kPawn);
            }
        }
    }

    void MoveGenerator::scoreQuiets() {
        using namespace tunable;

        const auto threats = m_pos.threats();

        for (u32 i = m_idx; i < m_end; ++i) {
            auto& scoredMove = m_data.moves[i];

            const auto move = scoredMove.move;
            auto& score = scoredMove.score;

            const auto moving = m_pos.boards().pieceOn(move.fromSq());

            score += m_history.getButterfly(threats, move) * movepickButterflyWeight();
            score += m_history.getPieceTo(threats, moving, move) * movepickPieceToWeight();

            score += getConthist(m_continuations, m_ply, moving, move, 1) * movepickCont1Weight();
            score += getConthist(m_continuations, m_ply, moving, move, 2) * movepickCont2Weight();
            score += getConthist(m_continuations, m_ply, moving, move, 4) * movepickCont4Weight();
            score += getConthist(m_continuations, m_ply, moving, move, 6) * movepickCont6Weight();

            score /= 1024;

            score +=
                directCheckBonus() * (m_pos.givesDirectCheck(move) && see::see(m_pos, move, directCheckSeeThreshold()));
        }
    }

    u32 MoveGenerator::findNext() {
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
} // namespace stormphrax
