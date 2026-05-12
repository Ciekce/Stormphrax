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

#include "see.h"

#include "attacks/attacks.h"
#include "rays.h"

namespace stormphrax::see {
    namespace {
        [[nodiscard]] inline PieceType popLeastValuable(
            const Position& pos,
            Bitboard& occ,
            Bitboard attackers,
            Color c
        ) {
            for (const auto pt : tunable::g_seeOrderedPts) {
                const auto bb = attackers & pos.bb(pt, c);
                if (!bb.empty()) {
                    occ ^= bb.lowestBit();
                    return pt;
                }
            }

            return PieceTypes::kNone;
        }
    } // namespace

    i32 gain(const Position& pos, Move move) {
        const auto type = move.type();

        if (type == MoveType::kCastling) {
            return 0;
        } else if (type == MoveType::kEnPassant) {
            return value(PieceTypes::kPawn);
        }

        auto score = value(pos.pieceOn(move.toSq()));

        if (type == MoveType::kPromotion) {
            score += value(move.promo()) - value(PieceTypes::kPawn);
        }

        return score;
    }

    bool see(const Position& pos, Move move, Score threshold) {
        const auto color = pos.stm();

        auto score = gain(pos, move) - threshold;

        if (score < 0) {
            return false;
        }

        auto next = move.type() == MoveType::kPromotion ? move.promo() : pos.pieceOn(move.fromSq()).type();

        score -= value(next);

        if (score >= 0) {
            return true;
        }

        const auto sq = move.toSq();

        auto occ = pos.occ() ^ move.fromSq().bit() ^ sq.bit();

        const auto queens = pos.bb(PieceTypes::kQueen);

        const auto bishops = queens | pos.bb(PieceTypes::kBishop);
        const auto rooks = queens | pos.bb(PieceTypes::kRook);

        const auto blackPinned = pos.pinned(Colors::kBlack);
        const auto whitePinned = pos.pinned(Colors::kWhite);

        const auto blackKingRay = rayIntersecting(pos.blackKing(), sq);
        const auto whiteKingRay = rayIntersecting(pos.whiteKing(), sq);

        const auto allowed = ~(blackPinned | whitePinned) | (blackPinned & blackKingRay) | (whitePinned & whiteKingRay);

        auto attackers = pos.allAttackersTo(sq, occ) & allowed;

        auto us = color.flip();

        while (true) {
            const auto ourAttackers = attackers & pos.bb(us);

            if (ourAttackers.empty()) {
                break;
            }

            next = popLeastValuable(pos, occ, ourAttackers, us);

            if (next == PieceTypes::kPawn || next == PieceTypes::kBishop || next == PieceTypes::kQueen) {
                attackers |= attacks::getBishopAttacks(sq, occ) & bishops;
            }

            if (next == PieceTypes::kRook || next == PieceTypes::kQueen) {
                attackers |= attacks::getRookAttacks(sq, occ) & rooks;
            }

            attackers &= occ;

            score = -score - 1 - value(next);
            us = us.flip();

            if (score >= 0) {
                // our only attacker is our king, but the opponent still has defenders
                if (next == PieceTypes::kKing && !(attackers & pos.bb(us)).empty()) {
                    us = us.flip();
                }
                break;
            }
        }

        return color != us;
    }
} // namespace stormphrax::see
