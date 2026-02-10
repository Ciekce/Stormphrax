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
        constexpr usize kPieceSquares = 12 * 64;
        constexpr usize kColor = 1;
        constexpr usize kCastling = 16;
        constexpr usize kEnPassant = 8;

        constexpr auto kTotal = kPieceSquares + kColor + kCastling + kEnPassant;
    } // namespace sizes

    namespace offsets {
        constexpr usize kPieceSquares = 0;
        constexpr auto kColor = kPieceSquares + sizes::kPieceSquares;
        constexpr auto kCastling = kColor + sizes::kColor;
        constexpr auto kEnPassant = kCastling + sizes::kCastling;
    } // namespace offsets

    constexpr auto kKeys = [] {
        constexpr auto kSeed = U64(0xD06C659954EC904A);

        std::array<u64, sizes::kTotal> keys{};

        util::rng::Jsf64Rng rng{kSeed};

        for (auto& key : keys) {
            key = rng.nextU64();
        }

        return keys;
    }();

    inline u64 pieceSquare(Piece piece, Square square) {
        if (piece == Pieces::kNone || square == Square::kNone) {
            return 0;
        }

        return kKeys[offsets::kPieceSquares + static_cast<usize>(square) * 12 + piece.idx()];
    }

    // for flipping
    inline u64 color() {
        return kKeys[offsets::kColor];
    }

    inline u64 color(Color c) {
        return c == Colors::kWhite ? 0 : color();
    }

    inline u64 castling(const CastlingRooks& castlingRooks) {
        constexpr usize BlackShort = 0x01;
        constexpr usize BlackLong = 0x02;
        constexpr usize WhiteShort = 0x04;
        constexpr usize WhiteLong = 0x08;

        usize flags{};

        if (castlingRooks.black().kingside != Square::kNone) {
            flags |= BlackShort;
        }
        if (castlingRooks.black().queenside != Square::kNone) {
            flags |= BlackLong;
        }
        if (castlingRooks.white().kingside != Square::kNone) {
            flags |= WhiteShort;
        }
        if (castlingRooks.white().queenside != Square::kNone) {
            flags |= WhiteLong;
        }

        return kKeys[offsets::kCastling + flags];
    }

    inline u64 enPassant(u32 file) {
        return kKeys[offsets::kEnPassant + file];
    }

    inline u64 enPassant(Square square) {
        if (square == Square::kNone) {
            return 0;
        }

        return kKeys[offsets::kEnPassant + squareFile(square)];
    }
} // namespace stormphrax::keys
