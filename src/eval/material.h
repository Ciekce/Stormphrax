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

#include "../types.h"

#include <array>

#include "../core.h"
#include "../util/cemath.h"
#include "tapered.h"

namespace stormphrax::eval {
#define S(Mg, Eg) \
    TaperedScore { \
        (Mg), (Eg) \
    }

    namespace values {
        constexpr auto kPawn = S(89, 100);
        constexpr auto kKnight = S(382, 331);
        constexpr auto kBishop = S(402, 362);
        constexpr auto kRook = S(512, 647);
        constexpr auto kQueen = S(1109, 1246);

        constexpr auto kKing = S(0, 0);

        constexpr auto kPtValues = std::array{kPawn, kKnight, kBishop, kRook, kQueen, kKing, TaperedScore{}};

        constexpr auto kValues = std::array{
            kPawn,
            kPawn,
            kKnight,
            kKnight,
            kBishop,
            kBishop,
            kRook,
            kRook,
            kQueen,
            kQueen,
            kKing,
            kKing,
            TaperedScore{}
        };
    } // namespace values
#undef S

    namespace psqt {
        extern const std::array<std::array<TaperedScore, Squares::kCount>, Pieces::kCount> g_pieceSquareTables;
    }

    inline auto pieceSquareValue(Piece piece, Square square) {
        return psqt::g_pieceSquareTables[piece.idx()][square.idx()];
    }

    constexpr auto pieceValue(PieceType piece) {
        return values::kPtValues[piece.idx()];
    }

    constexpr auto pieceValue(Piece piece) {
        return values::kValues[piece.idx()];
    }

    constexpr auto kPhase = std::array{0, 0, 1, 1, 2, 2, 2, 2, 4, 4, 0, 0};

    struct MaterialState {
        static constexpr i32 kMaxPhase = 24;

        TaperedScore material{};
        i32 phase{};

        constexpr void subAdd(Piece piece, Square src, Square dst) {
            material -= pieceSquareValue(piece, src);
            material += pieceSquareValue(piece, dst);
        }

        constexpr void add(Piece piece, Square square) {
            phase += kPhase[piece.idx()];
            material += pieceSquareValue(piece, square);
        }

        constexpr void sub(Piece piece, Square square) {
            phase -= kPhase[piece.idx()];
            material -= pieceSquareValue(piece, square);
        }

        [[nodiscard]] inline i32 interp(TaperedScore tapered) const {
            return util::ilerp<kMaxPhase>(tapered.endgame(), tapered.midgame(), std::min(phase, kMaxPhase));
        }

        [[nodiscard]] constexpr bool operator==(const MaterialState& other) const = default;
    };
} // namespace stormphrax::eval

