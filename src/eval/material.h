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

#include "../core.h"
#include "../util/cemath.h"
#include "../util/multi_array.h"
#include "tapered.h"

namespace stormphrax::eval {
#define S(Mg, Eg) \
    TaperedScore { \
        (Mg), (Eg) \
    }

    namespace values {
        constexpr auto kPawn = S(82, 94);
        constexpr auto kKnight = S(337, 281);
        constexpr auto kBishop = S(365, 297);
        constexpr auto kRook = S(477, 512);
        constexpr auto kQueen = S(1025, 936);

        constexpr auto kKing = S(0, 0);

        constexpr auto kBaseValues = std::array{
            kPawn,
            kKnight,
            kBishop,
            kRook,
            kQueen,
            kKing,
            TaperedScore{},
        };

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
            TaperedScore{},
        };
    } // namespace values
#undef S

    namespace psqt {
        extern const util::MultiArray<TaperedScore, 12, 64> g_pieceSquareTables;
    }

    [[nodiscard]] inline TaperedScore pieceSquareValue(Piece piece, Square square) {
        return psqt::g_pieceSquareTables[static_cast<i32>(piece)][static_cast<i32>(square)];
    }

    [[nodiscard]] constexpr TaperedScore pieceValue(PieceType piece) {
        return values::kBaseValues[static_cast<i32>(piece)];
    }

    [[nodiscard]] constexpr TaperedScore pieceValue(Piece piece) {
        return values::kValues[static_cast<i32>(piece)];
    }

    constexpr auto kPhase = std::array{0, 0, 1, 1, 2, 2, 2, 2, 4, 4, 0, 0};

    struct MaterialScore {
        TaperedScore score{};
        i32 phase{};

        constexpr void subAdd(Piece piece, Square src, Square dst) {
            score -= pieceSquareValue(piece, src);
            score += pieceSquareValue(piece, dst);
        }

        constexpr void add(Piece piece, Square square) {
            phase += kPhase[static_cast<i32>(piece)];
            score += pieceSquareValue(piece, square);
        }

        constexpr void sub(Piece piece, Square square) {
            phase -= kPhase[static_cast<i32>(piece)];
            score -= pieceSquareValue(piece, square);
        }

        [[nodiscard]] constexpr Score get() const {
            return util::ilerp<24>(score.endgame(), score.midgame(), std::min(phase, 24));
        }

        [[nodiscard]] constexpr bool operator==(const MaterialScore& other) const = default;
    };
} // namespace stormphrax::eval
