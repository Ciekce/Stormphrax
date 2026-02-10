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
            const auto bit = Bitboard::fromSquare(Square::fromRaw(i));

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
            const auto bit = Bitboard::fromSquare(Square::fromRaw(i));

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

    consteval std::array<Bitboard, 64> generatePawnAttacks(Color us) {
        std::array<Bitboard, 64> dst{};

        for (usize i = 0; i < dst.size(); ++i) {
            const auto bit = Bitboard::fromSquare(Square::fromRaw(i));

            dst[i] |= bit.shiftUpLeftRelative(us);
            dst[i] |= bit.shiftUpRightRelative(us);
        }

        return dst;
    }

    constexpr auto kBlackPawnAttacks = generatePawnAttacks(Colors::kBlack);
    constexpr auto kWhitePawnAttacks = generatePawnAttacks(Colors::kWhite);

    constexpr Bitboard getKnightAttacks(Square src) {
        return kKnightAttacks[src.idx()];
    }

    constexpr Bitboard getKingAttacks(Square src) {
        return kKingAttacks[src.idx()];
    }

    constexpr Bitboard getPawnAttacks(Square src, Color color) {
        const auto& attacks = color == Colors::kWhite ? kWhitePawnAttacks : kBlackPawnAttacks;
        return attacks[src.idx()];
    }

    inline Bitboard getQueenAttacks(Square src, Bitboard occupancy) {
        return getRookAttacks(src, occupancy) | getBishopAttacks(src, occupancy);
    }

    inline Bitboard getNonPawnPieceAttacks(PieceType pt, Square src, Bitboard occupancy = Bitboard{}) {
        assert(pt != PieceTypes::kNone);
        assert(pt != PieceTypes::kPawn);

        switch (pt.raw()) {
            case PieceTypes::kKnight.raw():
                return getKnightAttacks(src);
            case PieceTypes::kBishop.raw():
                return getBishopAttacks(src, occupancy);
            case PieceTypes::kRook.raw():
                return getRookAttacks(src, occupancy);
            case PieceTypes::kQueen.raw():
                return getQueenAttacks(src, occupancy);
            case PieceTypes::kKing.raw():
                return getKingAttacks(src);
            default:
                __builtin_unreachable();
        }
    }
} // namespace stormphrax::attacks
