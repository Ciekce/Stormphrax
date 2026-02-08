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

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <utility>

#include "util/bitfield.h"
#include "util/cemath.h"

namespace stormphrax {
    enum class Piece : u8 {
        kBlackPawn = 0,
        kWhitePawn,
        kBlackKnight,
        kWhiteKnight,
        kBlackBishop,
        kWhiteBishop,
        kBlackRook,
        kWhiteRook,
        kBlackQueen,
        kWhiteQueen,
        kBlackKing,
        kWhiteKing,
        kNone,
    };

    enum class PieceType {
        kPawn = 0,
        kKnight,
        kBishop,
        kRook,
        kQueen,
        kKing,
        kNone,
    };

    enum class Color : i8 {
        kBlack = 0,
        kWhite,
        kNone,
    };

    [[nodiscard]] constexpr Color oppColor(Color color) {
        assert(color != Color::kNone);
        return static_cast<Color>(!static_cast<i32>(color));
    }

    [[nodiscard]] constexpr Piece colorPiece(PieceType piece, Color color) {
        assert(piece != PieceType::kNone);
        assert(color != Color::kNone);

        return static_cast<Piece>((static_cast<i32>(piece) << 1) + static_cast<i32>(color));
    }

    [[nodiscard]] constexpr PieceType pieceType(Piece piece) {
        assert(piece != Piece::kNone);
        return static_cast<PieceType>(static_cast<i32>(piece) >> 1);
    }

    [[nodiscard]] constexpr PieceType pieceTypeOrNone(Piece piece) {
        if (piece == Piece::kNone) {
            return PieceType::kNone;
        }
        return static_cast<PieceType>(static_cast<i32>(piece) >> 1);
    }

    [[nodiscard]] constexpr Color pieceColor(Piece piece) {
        assert(piece != Piece::kNone);
        return static_cast<Color>(static_cast<i32>(piece) & 1);
    }

    [[nodiscard]] constexpr Piece flipPieceColor(Piece piece) {
        assert(piece != Piece::kNone);
        return static_cast<Piece>(static_cast<i32>(piece) ^ 0x1);
    }

    [[nodiscard]] constexpr Piece copyPieceColor(Piece piece, PieceType target) {
        assert(piece != Piece::kNone);
        assert(target != PieceType::kNone);

        return colorPiece(target, pieceColor(piece));
    }

    [[nodiscard]] constexpr bool isMajor(PieceType piece) {
        assert(piece != PieceType::kNone);
        return piece == PieceType::kRook || piece == PieceType::kQueen;
    }

    [[nodiscard]] constexpr bool isMajor(Piece piece) {
        assert(piece != Piece::kNone);
        return isMajor(pieceType(piece));
    }

    [[nodiscard]] constexpr bool isMinor(PieceType piece) {
        assert(piece != PieceType::kNone);
        return piece == PieceType::kKnight || piece == PieceType::kBishop;
    }

    [[nodiscard]] constexpr bool isMinor(Piece piece) {
        assert(piece != Piece::kNone);
        return isMinor(pieceType(piece));
    }

    [[nodiscard]] constexpr bool isValidPromotion(PieceType piece) {
        return piece == PieceType::kKnight || piece == PieceType::kBishop || piece == PieceType::kRook
            || piece == PieceType::kQueen;
    }

    [[nodiscard]] constexpr Piece pieceFromChar(char c) {
        switch (c) {
            case 'p':
                return Piece::kBlackPawn;
            case 'P':
                return Piece::kWhitePawn;
            case 'n':
                return Piece::kBlackKnight;
            case 'N':
                return Piece::kWhiteKnight;
            case 'b':
                return Piece::kBlackBishop;
            case 'B':
                return Piece::kWhiteBishop;
            case 'r':
                return Piece::kBlackRook;
            case 'R':
                return Piece::kWhiteRook;
            case 'q':
                return Piece::kBlackQueen;
            case 'Q':
                return Piece::kWhiteQueen;
            case 'k':
                return Piece::kBlackKing;
            case 'K':
                return Piece::kWhiteKing;
            default:
                return Piece::kNone;
        }
    }

    [[nodiscard]] constexpr PieceType pieceTypeFromChar(char c) {
        switch (c) {
            case 'p':
                return PieceType::kPawn;
            case 'n':
                return PieceType::kKnight;
            case 'b':
                return PieceType::kBishop;
            case 'r':
                return PieceType::kRook;
            case 'q':
                return PieceType::kQueen;
            case 'k':
                return PieceType::kKing;
            default:
                return PieceType::kNone;
        }
    }

    // upside down
    enum class Square : u8 {
        // clang-format off
        kA1, kB1, kC1, kD1, kE1, kF1, kG1, kH1,
        kA2, kB2, kC2, kD2, kE2, kF2, kG2, kH2,
        kA3, kB3, kC3, kD3, kE3, kF3, kG3, kH3,
        kA4, kB4, kC4, kD4, kE4, kF4, kG4, kH4,
        kA5, kB5, kC5, kD5, kE5, kF5, kG5, kH5,
        kA6, kB6, kC6, kD6, kE6, kF6, kG6, kH6,
        kA7, kB7, kC7, kD7, kE7, kF7, kG7, kH7,
        kA8, kB8, kC8, kD8, kE8, kF8, kG8, kH8,
        kNone,
        // clang-format on
    };

    [[nodiscard]] constexpr Square toSquare(u32 rank, u32 file) {
        assert(rank < 8);
        assert(file < 8);

        return static_cast<Square>((rank << 3) | file);
    }

    [[nodiscard]] constexpr i32 squareRank(Square square) {
        assert(square != Square::kNone);
        return static_cast<i32>(square) >> 3;
    }

    [[nodiscard]] constexpr i32 squareFile(Square square) {
        assert(square != Square::kNone);
        return static_cast<i32>(square) & 0x7;
    }

    [[nodiscard]] constexpr Square flipSquareRank(Square square) {
        assert(square != Square::kNone);
        return static_cast<Square>(static_cast<i32>(square) ^ 0b111000);
    }

    [[nodiscard]] constexpr Square flipSquareFile(Square square) {
        assert(square != Square::kNone);
        return static_cast<Square>(static_cast<i32>(square) ^ 0b000111);
    }

    [[nodiscard]] constexpr u64 squareBit(Square square) {
        assert(square != Square::kNone);
        return U64(1) << static_cast<i32>(square);
    }

    [[nodiscard]] constexpr u64 squareBitChecked(Square square) {
        if (square == Square::kNone) {
            return U64(0);
        }

        return U64(1) << static_cast<i32>(square);
    }

    template <Color C>
    constexpr i32 relativeRank(i32 rank) {
        assert(rank >= 0 && rank < 8);

        if constexpr (C == Color::kBlack) {
            return 7 - rank;
        } else {
            return rank;
        }
    }

    constexpr i32 relativeRank(Color c, i32 rank) {
        assert(rank >= 0 && rank < 8);
        return c == Color::kBlack ? 7 - rank : rank;
    }

    struct KingPair {
        std::array<Square, 2> kings{};

        [[nodiscard]] inline Square black() const {
            return kings[0];
        }

        [[nodiscard]] inline Square white() const {
            return kings[1];
        }

        [[nodiscard]] inline Square& black() {
            return kings[0];
        }

        [[nodiscard]] inline Square& white() {
            return kings[1];
        }

        [[nodiscard]] inline Square color(Color c) const {
            assert(c != Color::kNone);
            return kings[static_cast<i32>(c)];
        }

        [[nodiscard]] inline Square& color(Color c) {
            assert(c != Color::kNone);
            return kings[static_cast<i32>(c)];
        }

        [[nodiscard]] inline bool operator==(const KingPair& other) const = default;

        [[nodiscard]] inline bool isValid() {
            return black() != Square::kNone && white() != Square::kNone && black() != white();
        }
    };

    struct CastlingRooks {
        struct RookPair {
            Square kingside{Square::kNone};
            Square queenside{Square::kNone};

            inline void clear() {
                kingside = Square::kNone;
                queenside = Square::kNone;
            }

            inline void unset(Square square) {
                assert(square != Square::kNone);

                if (square == kingside) {
                    kingside = Square::kNone;
                } else if (square == queenside) {
                    queenside = Square::kNone;
                }
            }

            [[nodiscard]] inline bool operator==(const RookPair&) const = default;
        };

        std::array<RookPair, 2> rooks;

        [[nodiscard]] inline const RookPair& black() const {
            return rooks[0];
        }

        [[nodiscard]] inline const RookPair& white() const {
            return rooks[1];
        }

        [[nodiscard]] inline RookPair& black() {
            return rooks[0];
        }

        [[nodiscard]] inline RookPair& white() {
            return rooks[1];
        }

        [[nodiscard]] inline const RookPair& color(Color c) const {
            assert(c != Color::kNone);
            return rooks[static_cast<i32>(c)];
        }

        [[nodiscard]] inline RookPair& color(Color c) {
            assert(c != Color::kNone);
            return rooks[static_cast<i32>(c)];
        }

        [[nodiscard]] inline bool operator==(const CastlingRooks&) const = default;
    };

    using Score = i32;

    constexpr auto kScoreInf = 32767;
    constexpr auto kScoreMate = 32766;
    constexpr auto kScoreTbWin = 30000;
    constexpr auto kScoreWin = 25000;

    constexpr auto kScoreNone = -kScoreInf;

    constexpr i32 kMaxDepth = 248;

    constexpr auto kScoreMaxMate = kScoreMate - kMaxDepth;
} // namespace stormphrax

template <>
struct fmt::formatter<stormphrax::Piece> : fmt::formatter<std::string_view> {
    format_context::iterator format(stormphrax::Piece value, format_context& ctx) const;
};

template <>
struct fmt::formatter<stormphrax::PieceType> : fmt::formatter<std::string_view> {
    format_context::iterator format(stormphrax::PieceType value, format_context& ctx) const;
};

template <>
struct fmt::formatter<stormphrax::Square> : fmt::nested_formatter<char> {
    format_context::iterator format(stormphrax::Square value, format_context& ctx) const;
};
