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
    class Color {
    public:
        constexpr Color() = default;

        constexpr Color(const Color&) = default;
        constexpr Color(Color&&) = default;

        [[nodiscard]] constexpr u8 raw() const {
            return m_id;
        }

        [[nodiscard]] constexpr usize idx() const {
            return static_cast<usize>(m_id);
        }

        [[nodiscard]] constexpr Color flip() const {
            assert(m_id != kNoneId);
            return Color{static_cast<u8>(m_id ^ 1)};
        }

        [[nodiscard]] static constexpr Color fromRaw(u8 id) {
            assert(id <= kNoneId);
            return Color{id};
        }

        [[nodiscard]] constexpr explicit operator bool() const {
            return m_id != kNoneId;
        }

        [[nodiscard]] constexpr bool operator==(const Color&) const = default;

        constexpr Color& operator=(const Color&) = default;
        constexpr Color& operator=(Color&&) = default;

    private:
        explicit constexpr Color(u8 id) :
                m_id{id} {}

        u8 m_id{};

        enum : u8 {
            kBlackId = 0,
            kWhiteId,
            kNoneId,
        };

        friend struct Colors;
    };

    struct Colors {
        Colors() = delete;

        static constexpr Color kBlack{Color::kBlackId};
        static constexpr Color kWhite{Color::kWhiteId};
        static constexpr Color kNone{Color::kNoneId};

        static constexpr usize kCount = kNone.idx();
    };

    class Piece;

    class PieceType {
    public:
        constexpr PieceType() = default;

        constexpr PieceType(const PieceType&) = default;
        constexpr PieceType(PieceType&&) = default;

        [[nodiscard]] constexpr u8 raw() const {
            return m_id;
        }

        [[nodiscard]] constexpr usize idx() const {
            return static_cast<usize>(m_id);
        }

        [[nodiscard]] constexpr Piece withColor(Color c) const;

        [[nodiscard]] constexpr bool isMajor() const {
            return m_id == kRookId || m_id == kQueenId;
        }

        [[nodiscard]] constexpr bool isMinor() const {
            return m_id == kKnightId || m_id == kBishopId;
        }

        [[nodiscard]] constexpr bool isValidPromotion() const {
            return m_id == kKnightId || m_id == kBishopId || m_id == kRookId || m_id == kQueenId;
        }

        [[nodiscard]] constexpr char asChar() const {
            switch (m_id) {
                case kPawnId:
                    return 'P';
                case kKnightId:
                    return 'N';
                case kBishopId:
                    return 'B';
                case kRookId:
                    return 'R';
                case kQueenId:
                    return 'Q';
                case kKingId:
                    return 'K';
                default:
                    return '?';
            }
        }

        [[nodiscard]] static constexpr PieceType fromRaw(u8 id) {
            assert(id <= kNoneId);
            return PieceType{id};
        }

        [[nodiscard]] static constexpr PieceType fromChar(char c) {
            switch (c) {
                case 'P':
                    return PieceType{kPawnId};
                case 'N':
                    return PieceType{kKnightId};
                case 'B':
                    return PieceType{kBishopId};
                case 'R':
                    return PieceType{kRookId};
                case 'Q':
                    return PieceType{kQueenId};
                case 'K':
                    return PieceType{kKingId};
                default:
                    return PieceType{kNoneId};
            }
        }

        [[nodiscard]] constexpr explicit operator bool() const {
            return m_id != kNoneId;
        }

        [[nodiscard]] constexpr bool operator==(const PieceType&) const = default;

        constexpr PieceType& operator=(const PieceType&) = default;
        constexpr PieceType& operator=(PieceType&&) = default;

    private:
        explicit constexpr PieceType(u8 id) :
                m_id{id} {}

        u8 m_id{};

        enum : u8 {
            kPawnId = 0,
            kKnightId,
            kBishopId,
            kRookId,
            kQueenId,
            kKingId,
            kNoneId,
        };

        friend struct PieceTypes;
    };

    struct PieceTypes {
        PieceTypes() = delete;

        static constexpr PieceType kPawn{PieceType::kPawnId};
        static constexpr PieceType kKnight{PieceType::kKnightId};
        static constexpr PieceType kBishop{PieceType::kBishopId};
        static constexpr PieceType kRook{PieceType::kRookId};
        static constexpr PieceType kQueen{PieceType::kQueenId};
        static constexpr PieceType kKing{PieceType::kKingId};
        static constexpr PieceType kNone{PieceType::kNoneId};

        static constexpr usize kCount = kNone.idx();

        static constexpr std::array kAll = {
            kPawn,
            kKnight,
            kBishop,
            kRook,
            kQueen,
            kKing,
        };
    };

    class Piece {
    public:
        constexpr Piece() = default;

        constexpr Piece(const Piece&) = default;
        constexpr Piece(Piece&&) = default;

        [[nodiscard]] constexpr u8 raw() const {
            return m_id;
        }

        [[nodiscard]] constexpr usize idx() const {
            return static_cast<usize>(m_id);
        }

        [[nodiscard]] constexpr PieceType type() const {
            assert(m_id != kNoneId);
            return PieceType::fromRaw(m_id >> 1);
        }

        [[nodiscard]] constexpr PieceType typeOrNone() const {
            return PieceType::fromRaw(m_id >> 1);
        }

        [[nodiscard]] constexpr Color color() const {
            assert(m_id != kNoneId);
            return Color::fromRaw(m_id & 0b1);
        }

        [[nodiscard]] constexpr Piece flipColor() const {
            assert(m_id != kNoneId);
            return fromRaw(m_id ^ 0b1);
        }

        [[nodiscard]] constexpr Piece copyColor(PieceType target) const {
            return target.withColor(color());
        }

        [[nodiscard]] constexpr char asChar() const {
            switch (m_id) {
                case kBlackPawnId:
                    return 'p';
                case kWhitePawnId:
                    return 'P';
                case kBlackKnightId:
                    return 'n';
                case kWhiteKnightId:
                    return 'N';
                case kBlackBishopId:
                    return 'b';
                case kWhiteBishopId:
                    return 'B';
                case kBlackRookId:
                    return 'r';
                case kWhiteRookId:
                    return 'R';
                case kBlackQueenId:
                    return 'q';
                case kWhiteQueenId:
                    return 'Q';
                case kBlackKingId:
                    return 'k';
                case kWhiteKingId:
                    return 'K';
                default:
                    return '?';
            }
        }

        [[nodiscard]] static constexpr Piece fromRaw(u8 id) {
            assert(id <= kNoneId);
            return Piece{id};
        }

        [[nodiscard]] static constexpr Piece fromChar(char c) {
            switch (c) {
                case 'p':
                    return Piece{kBlackPawnId};
                case 'P':
                    return Piece{kWhitePawnId};
                case 'n':
                    return Piece{kBlackKnightId};
                case 'N':
                    return Piece{kWhiteKnightId};
                case 'b':
                    return Piece{kBlackBishopId};
                case 'B':
                    return Piece{kWhiteBishopId};
                case 'r':
                    return Piece{kBlackRookId};
                case 'R':
                    return Piece{kWhiteRookId};
                case 'q':
                    return Piece{kBlackQueenId};
                case 'Q':
                    return Piece{kWhiteQueenId};
                case 'k':
                    return Piece{kBlackKingId};
                case 'K':
                    return Piece{kWhiteKingId};
                default:
                    return Piece{kNoneId};
            }
        }

        [[nodiscard]] constexpr explicit operator bool() const {
            return m_id != kNoneId;
        }

        [[nodiscard]] constexpr bool operator==(const Piece&) const = default;

        constexpr Piece& operator=(const Piece&) = default;
        constexpr Piece& operator=(Piece&&) = default;

    private:
        explicit constexpr Piece(u8 id) :
                m_id{id} {}

        u8 m_id{};

        enum : u8 {
            kBlackPawnId = 0,
            kWhitePawnId,
            kBlackKnightId,
            kWhiteKnightId,
            kBlackBishopId,
            kWhiteBishopId,
            kBlackRookId,
            kWhiteRookId,
            kBlackQueenId,
            kWhiteQueenId,
            kBlackKingId,
            kWhiteKingId,
            kNoneId,
        };

        friend struct Pieces;
    };

    constexpr Piece PieceType::withColor(Color c) const {
        assert(*this != PieceTypes::kNone);
        assert(c != Colors::kNone);

        return Piece::fromRaw((m_id << 1) | c.raw());
    }

    struct Pieces {
        Pieces() = delete;

        static constexpr Piece kBlackPawn{Piece::kBlackPawnId};
        static constexpr Piece kWhitePawn{Piece::kWhitePawnId};
        static constexpr Piece kBlackKnight{Piece::kBlackKnightId};
        static constexpr Piece kWhiteKnight{Piece::kWhiteKnightId};
        static constexpr Piece kBlackBishop{Piece::kBlackBishopId};
        static constexpr Piece kWhiteBishop{Piece::kWhiteBishopId};
        static constexpr Piece kBlackRook{Piece::kBlackRookId};
        static constexpr Piece kWhiteRook{Piece::kWhiteRookId};
        static constexpr Piece kBlackQueen{Piece::kBlackQueenId};
        static constexpr Piece kWhiteQueen{Piece::kWhiteQueenId};
        static constexpr Piece kBlackKing{Piece::kBlackKingId};
        static constexpr Piece kWhiteKing{Piece::kWhiteKingId};
        static constexpr Piece kNone{Piece::kNoneId};

        static constexpr usize kCount = kNone.idx();
    };

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

    constexpr i32 relativeRank(Color c, i32 rank) {
        assert(rank >= 0 && rank < 8);
        return c == Colors::kBlack ? 7 - rank : rank;
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
            assert(c != Colors::kNone);
            return kings[c.idx()];
        }

        [[nodiscard]] inline Square& color(Color c) {
            assert(c != Colors::kNone);
            return kings[c.idx()];
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
            assert(c != Colors::kNone);
            return rooks[c.idx()];
        }

        [[nodiscard]] inline RookPair& color(Color c) {
            assert(c != Colors::kNone);
            return rooks[c.idx()];
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
