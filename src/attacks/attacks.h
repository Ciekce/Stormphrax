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

#include "../types.h"

#include <array>
#include <cassert>

#include "../bitboard.h"
#include "../core.h"
#include "../util/bits.h"
#include "util.h"

#if SP_HAS_BMI2
    #include "bmi2/attacks.h"
#else
    #include "black_magic/attacks.h"
#endif

namespace stormphrax::attacks {
    constexpr auto kKnightAttacks = [] {
        std::array<Bitboard, 64> dst{};

        for (usize i = 0; i < dst.size(); ++i) {
            const auto bit = Bitboard::fromSquare(static_cast<Square>(i));

            auto& attacks = dst[i];

            attacks |= bit.shiftUpUpLeft();
            attacks |= bit.shiftUpUpRight();
            attacks |= bit.shiftUpLeftLeft();
            attacks |= bit.shiftUpRightRight();
            attacks |= bit.shiftDownLeftLeft();
            attacks |= bit.shiftDownRightRight();
            attacks |= bit.shiftDownDownLeft();
            attacks |= bit.shiftDownDownRight();
        }

        return dst;
    }();

    constexpr auto kKingAttacks = [] {
        std::array<Bitboard, 64> dst{};

        for (usize i = 0; i < dst.size(); ++i) {
            const auto bit = Bitboard::fromSquare(static_cast<Square>(i));

            auto& attacks = dst[i];

            attacks |= bit.shiftUp();
            attacks |= bit.shiftDown();
            attacks |= bit.shiftLeft();
            attacks |= bit.shiftRight();
            attacks |= bit.shiftUpLeft();
            attacks |= bit.shiftUpRight();
            attacks |= bit.shiftDownLeft();
            attacks |= bit.shiftDownRight();
        }

        return dst;
    }();

    template <Color Us>
    consteval std::array<Bitboard, 64> generatePawnAttacks() {
        std::array<Bitboard, 64> dst{};

        for (usize i = 0; i < dst.size(); ++i) {
            const auto bit = Bitboard::fromSquare(static_cast<Square>(i));

            dst[i] |= bit.shiftUpLeftRelative<Us>();
            dst[i] |= bit.shiftUpRightRelative<Us>();
        }

        return dst;
    }

    constexpr auto kBlackPawnAttacks = generatePawnAttacks<Color::kBlack>();
    constexpr auto kWhitePawnAttacks = generatePawnAttacks<Color::kWhite>();

    constexpr Bitboard getKnightAttacks(Square src) {
        return kKnightAttacks[static_cast<usize>(src)];
    }

    constexpr Bitboard getKingAttacks(Square src) {
        return kKingAttacks[static_cast<usize>(src)];
    }

    constexpr Bitboard getPawnAttacks(Square src, Color color) {
        const auto& attacks = color == Color::kWhite ? kWhitePawnAttacks : kBlackPawnAttacks;
        return attacks[static_cast<usize>(src)];
    }

    inline Bitboard getQueenAttacks(Square src, Bitboard occupancy) {
        return getRookAttacks(src, occupancy) | getBishopAttacks(src, occupancy);
    }

    inline Bitboard getNonPawnPieceAttacks(PieceType piece, Square src, Bitboard occupancy = Bitboard{}) {
        assert(piece != PieceType::kNone);
        assert(piece != PieceType::kPawn);

        switch (piece) {
            case PieceType::kKnight:
                return getKnightAttacks(src);
            case PieceType::kBishop:
                return getBishopAttacks(src, occupancy);
            case PieceType::kRook:
                return getRookAttacks(src, occupancy);
            case PieceType::kQueen:
                return getQueenAttacks(src, occupancy);
            case PieceType::kKing:
                return getKingAttacks(src);
            default:
                __builtin_unreachable();
        }
    }
} // namespace stormphrax::attacks
