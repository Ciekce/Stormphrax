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
        return tunable::g_seeValues[static_cast<i32>(piece)];
    }

    constexpr i32 value(PieceType piece) {
        return tunable::g_seeValues[piece.idx() * 2];
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
            const auto piece = PieceType::fromRaw(i);
            auto board = attackers & bbs.forPiece(piece, color);

            if (!board.empty()) {
                occ ^= board.lowestBit();
                return piece;
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

        auto next = move.type() == MoveType::kPromotion ? move.promo() : pieceType(boards.pieceOn(move.fromSq()));

        score -= value(next);

        if (score >= 0) {
            return true;
        }

        const auto square = move.toSq();

        auto occupancy = bbs.occupancy() ^ squareBit(move.fromSq()) ^ squareBit(square);

        const auto queens = bbs.queens();

        const auto bishops = queens | bbs.bishops();
        const auto rooks = queens | bbs.rooks();

        const auto blackPinned = pos.pinned(Colors::kBlack);
        const auto whitePinned = pos.pinned(Colors::kWhite);

        const auto blackKingRay = rayIntersecting(pos.blackKing(), square);
        const auto whiteKingRay = rayIntersecting(pos.whiteKing(), square);

        const auto allowed = ~(blackPinned | whitePinned) | (blackPinned & blackKingRay) | (whitePinned & whiteKingRay);

        auto attackers = pos.allAttackersTo(square, occupancy) & allowed;

        auto us = color.flip();

        while (true) {
            const auto ourAttackers = attackers & bbs.forColor(us);

            if (ourAttackers.empty()) {
                break;
            }

            next = popLeastValuable(bbs, occupancy, ourAttackers, us);

            if (next == PieceTypes::kPawn || next == PieceTypes::kBishop || next == PieceTypes::kQueen) {
                attackers |= attacks::getBishopAttacks(square, occupancy) & bishops;
            }

            if (next == PieceTypes::kRook || next == PieceTypes::kQueen) {
                attackers |= attacks::getRookAttacks(square, occupancy) & rooks;
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
