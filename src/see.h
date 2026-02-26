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

#include "attacks/attacks.h"
#include "core.h"
#include "position/position.h"
#include "rays.h"
#include "tunable.h"

namespace stormphrax::see {
    constexpr i32 value(Piece piece) {
        return tunable::g_seeValues[piece.idx()];
    }

    constexpr i32 value(PieceType pt) {
        return tunable::g_seeValues[pt.idx() * 2];
    }

    inline i32 gain(const PositionBoards& boards, Move move) {
        const auto type = move.type();

        if (type == MoveType::kCastling) {
            return 0;
        } else if (type == MoveType::kEnPassant) {
            return value(PieceTypes::kPawn);
        }

        auto score = value(boards.pieceOn(move.toSq()));

        if (type == MoveType::kPromotion) {
            score += value(move.promo()) - value(PieceTypes::kPawn);
        }

        return score;
    }

    [[nodiscard]] inline PieceType popLeastValuable(
        const BitboardSet& bbs,
        Bitboard& occ,
        Bitboard attackers,
        Color color
    ) {
        for (i32 i = 0; i < 6; ++i) {
            const auto pt = PieceType::fromRaw(i);
            const auto board = attackers & bbs.forPiece(pt, color);

            if (!board.empty()) {
                occ ^= board.lowestBit();
                return pt;
            }
        }

        return PieceTypes::kNone;
    }

    // basically ported from ethereal and weiss (their implementation is the same)
    inline bool see(const Position& pos, Move move, Score threshold) {
        const auto& boards = pos.boards();
        const auto& bbs = boards.bbs();

        const auto color = pos.stm();

        auto score = gain(boards, move) - threshold;

        if (score < 0) {
            return false;
        }

        auto next = move.type() == MoveType::kPromotion ? move.promo() : boards.pieceOn(move.fromSq()).type();

        score -= value(next);

        if (score >= 0) {
            return true;
        }

        const auto sq = move.toSq();

        auto occupancy = bbs.occupancy() ^ move.fromSq().bit() ^ sq.bit();

        const auto queens = bbs.queens();

        const auto bishops = queens | bbs.bishops();
        const auto rooks = queens | bbs.rooks();

        const std::array kingRays = {
            rayPast(pos.blackKing(), sq),
            rayPast(pos.whiteKing(), sq),
        };

        auto attackers = pos.allAttackersTo(sq, occupancy);

        auto us = color.flip();

        while (true) {
            auto ourAttackers = attackers & bbs.forColor(us);

            if (pos.pinners(us.flip()) & occupancy) {
                ourAttackers &= ~(pos.pinned(us) & ~kingRays[us.idx()]);
            }

            if (ourAttackers.empty()) {
                break;
            }

            next = popLeastValuable(bbs, occupancy, ourAttackers, us);

            if (next == PieceTypes::kPawn || next == PieceTypes::kBishop || next == PieceTypes::kQueen) {
                attackers |= attacks::getBishopAttacks(sq, occupancy) & bishops;
            }

            if (next == PieceTypes::kRook || next == PieceTypes::kQueen) {
                attackers |= attacks::getRookAttacks(sq, occupancy) & rooks;
            }

            attackers &= occupancy;

            score = -score - 1 - value(next);
            us = us.flip();

            if (score >= 0) {
                // our only attacker is our king, but the opponent still has defenders
                if (next == PieceTypes::kKing && !(attackers & bbs.forColor(us)).empty()) {
                    us = us.flip();
                }
                break;
            }
        }

        return color != us;
    }
} // namespace stormphrax::see
