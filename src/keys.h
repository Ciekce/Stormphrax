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

#include "core.h"
#include "util/rng.h"

namespace stormphrax::keys {
    namespace sizes {
        constexpr usize PieceSquares = 12 * 64;
        constexpr usize Color = 1;
        constexpr usize Castling = 16;
        constexpr usize EnPassant = 8;

        constexpr auto Total = PieceSquares + Color + Castling + EnPassant;
    } // namespace sizes

    namespace offsets {
        constexpr usize PieceSquares = 0;
        constexpr auto Color = PieceSquares + sizes::PieceSquares;
        constexpr auto Castling = Color + sizes::Color;
        constexpr auto EnPassant = Castling + sizes::Castling;
    } // namespace offsets

    constexpr auto Keys = [] {
        constexpr auto Seed = U64(0xD06C659954EC904A);

        std::array<u64, sizes::Total> keys{};

        util::rng::Jsf64Rng rng{Seed};

        for (auto& key : keys) {
            key = rng.nextU64();
        }

        return keys;
    }();

    inline u64 pieceSquare(Piece piece, Square square) {
        if (piece == Piece::None || square == Square::None) {
            return 0;
        }

        return Keys[offsets::PieceSquares + static_cast<usize>(square) * 12 + static_cast<usize>(piece)];
    }

    // for flipping
    inline u64 color() {
        return Keys[offsets::Color];
    }

    inline u64 color(Color c) {
        return c == Color::White ? 0 : color();
    }

    inline u64 castling(const CastlingRooks& castlingRooks) {
        constexpr usize BlackShort = 0x01;
        constexpr usize BlackLong = 0x02;
        constexpr usize WhiteShort = 0x04;
        constexpr usize WhiteLong = 0x08;

        usize flags{};

        if (castlingRooks.black().kingside != Square::None) {
            flags |= BlackShort;
        }
        if (castlingRooks.black().queenside != Square::None) {
            flags |= BlackLong;
        }
        if (castlingRooks.white().kingside != Square::None) {
            flags |= WhiteShort;
        }
        if (castlingRooks.white().queenside != Square::None) {
            flags |= WhiteLong;
        }

        return Keys[offsets::Castling + flags];
    }

    inline u64 enPassant(u32 file) {
        return Keys[offsets::EnPassant + file];
    }

    inline u64 enPassant(Square square) {
        if (square == Square::None) {
            return 0;
        }

        return Keys[offsets::EnPassant + squareFile(square)];
    }
} // namespace stormphrax::keys
