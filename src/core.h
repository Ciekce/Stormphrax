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

    class Square {
    public:
        constexpr Square() = default;

        constexpr Square(const Square&) = default;
        constexpr Square(Square&&) = default;

        [[nodiscard]] constexpr u8 raw() const {
            return m_id;
        }

        [[nodiscard]] constexpr usize idx() const {
            return static_cast<usize>(m_id);
        }

        [[nodiscard]] constexpr i32 rank() const {
            assert(m_id != kNoneId);
            return static_cast<i32>(m_id) / 8;
        }

        [[nodiscard]] constexpr i32 file() const {
            assert(m_id != kNoneId);
            return static_cast<i32>(m_id) % 8;
        }

        [[nodiscard]] constexpr Square flipFile() const {
            assert(m_id != kNoneId);
            return fromRaw(m_id ^ 0b000111);
        }

        [[nodiscard]] constexpr Square flipRank() const {
            assert(m_id != kNoneId);
            return fromRaw(m_id ^ 0b111000);
        }

        // for en passant
        [[nodiscard]] constexpr Square flipRankParity() const {
            assert(m_id != kNoneId);
            return fromRaw(m_id ^ 0b001000);
        }

        [[nodiscard]] constexpr Square flipDoublePush() const {
            assert(m_id != kNoneId);
            return fromRaw(m_id ^ 0b010000);
        }

        [[nodiscard]] constexpr Square relative(Color c) const {
            assert(m_id != kNoneId);
            if (c == Colors::kBlack) {
                return *this;
            } else {
                return flipRank();
            }
        }

        [[nodiscard]] constexpr u64 bit() const {
            assert(m_id != kNoneId);
            return u64{1} << m_id;
        }

        [[nodiscard]] constexpr Square offset(i32 offset) const {
            assert(m_id + offset >= 0);
            assert(m_id + offset < kNoneId);
            return fromRaw(m_id + offset);
        }

        [[nodiscard]] constexpr Square withFile(i32 file) const {
            assert(m_id != kNoneId);
            assert(file >= 0 && file < 8);
            return fromFileRank(file, rank());
        }

        [[nodiscard]] constexpr Square withRank(i32 rank) const {
            assert(m_id != kNoneId);
            assert(rank >= 0 && rank < 8);
            return fromFileRank(file(), rank);
        }

        [[nodiscard]] static constexpr Square fromRaw(u8 id) {
            assert(id <= kNoneId);
            return Square{id};
        }

        [[nodiscard]] static constexpr Square fromFileRank(i32 file, i32 rank) {
            assert(rank >= 0 && rank <= 7);
            assert(file >= 0 && file <= 7);
            return fromRaw(rank * 8 + file);
        }

        [[nodiscard]] static constexpr Square fromStr(std::string_view str) {
            if (str.length() != 2) {
                return Square{kNoneId};
            }

            if (str[0] < 'a' || str[0] > 'h' || str[1] < '1' || str[1] > '8') {
                return Square{kNoneId};
            }

            const i32 file = str[0] - 'a';
            const i32 rank = str[1] - '1';

            return fromFileRank(file, rank);
        }

        [[nodiscard]] constexpr explicit operator bool() const {
            return m_id != kNoneId;
        }

        [[nodiscard]] constexpr bool operator==(const Square&) const = default;

        constexpr Square& operator=(const Square&) = default;
        constexpr Square& operator=(Square&&) = default;

    private:
        explicit constexpr Square(u8 id) :
                m_id{id} {}

        u8 m_id{};

        enum : u8 {
            // clang-format off
            kA1Id, kB1Id, kC1Id, kD1Id, kE1Id, kF1Id, kG1Id, kH1Id,
            kA2Id, kB2Id, kC2Id, kD2Id, kE2Id, kF2Id, kG2Id, kH2Id,
            kA3Id, kB3Id, kC3Id, kD3Id, kE3Id, kF3Id, kG3Id, kH3Id,
            kA4Id, kB4Id, kC4Id, kD4Id, kE4Id, kF4Id, kG4Id, kH4Id,
            kA5Id, kB5Id, kC5Id, kD5Id, kE5Id, kF5Id, kG5Id, kH5Id,
            kA6Id, kB6Id, kC6Id, kD6Id, kE6Id, kF6Id, kG6Id, kH6Id,
            kA7Id, kB7Id, kC7Id, kD7Id, kE7Id, kF7Id, kG7Id, kH7Id,
            kA8Id, kB8Id, kC8Id, kD8Id, kE8Id, kF8Id, kG8Id, kH8Id,
            // clang-format on
            kNoneId,
        };

        friend struct Squares;
    };

    struct Squares {
        Squares() = delete;

        static constexpr Square kA1{Square::kA1Id};
        static constexpr Square kB1{Square::kB1Id};
        static constexpr Square kC1{Square::kC1Id};
        static constexpr Square kD1{Square::kD1Id};
        static constexpr Square kE1{Square::kE1Id};
        static constexpr Square kF1{Square::kF1Id};
        static constexpr Square kG1{Square::kG1Id};
        static constexpr Square kH1{Square::kH1Id};
        static constexpr Square kA2{Square::kA2Id};
        static constexpr Square kB2{Square::kB2Id};
        static constexpr Square kC2{Square::kC2Id};
        static constexpr Square kD2{Square::kD2Id};
        static constexpr Square kE2{Square::kE2Id};
        static constexpr Square kF2{Square::kF2Id};
        static constexpr Square kG2{Square::kG2Id};
        static constexpr Square kH2{Square::kH2Id};
        static constexpr Square kA3{Square::kA3Id};
        static constexpr Square kB3{Square::kB3Id};
        static constexpr Square kC3{Square::kC3Id};
        static constexpr Square kD3{Square::kD3Id};
        static constexpr Square kE3{Square::kE3Id};
        static constexpr Square kF3{Square::kF3Id};
        static constexpr Square kG3{Square::kG3Id};
        static constexpr Square kH3{Square::kH3Id};
        static constexpr Square kA4{Square::kA4Id};
        static constexpr Square kB4{Square::kB4Id};
        static constexpr Square kC4{Square::kC4Id};
        static constexpr Square kD4{Square::kD4Id};
        static constexpr Square kE4{Square::kE4Id};
        static constexpr Square kF4{Square::kF4Id};
        static constexpr Square kG4{Square::kG4Id};
        static constexpr Square kH4{Square::kH4Id};
        static constexpr Square kA5{Square::kA5Id};
        static constexpr Square kB5{Square::kB5Id};
        static constexpr Square kC5{Square::kC5Id};
        static constexpr Square kD5{Square::kD5Id};
        static constexpr Square kE5{Square::kE5Id};
        static constexpr Square kF5{Square::kF5Id};
        static constexpr Square kG5{Square::kG5Id};
        static constexpr Square kH5{Square::kH5Id};
        static constexpr Square kA6{Square::kA6Id};
        static constexpr Square kB6{Square::kB6Id};
        static constexpr Square kC6{Square::kC6Id};
        static constexpr Square kD6{Square::kD6Id};
        static constexpr Square kE6{Square::kE6Id};
        static constexpr Square kF6{Square::kF6Id};
        static constexpr Square kG6{Square::kG6Id};
        static constexpr Square kH6{Square::kH6Id};
        static constexpr Square kA7{Square::kA7Id};
        static constexpr Square kB7{Square::kB7Id};
        static constexpr Square kC7{Square::kC7Id};
        static constexpr Square kD7{Square::kD7Id};
        static constexpr Square kE7{Square::kE7Id};
        static constexpr Square kF7{Square::kF7Id};
        static constexpr Square kG7{Square::kG7Id};
        static constexpr Square kH7{Square::kH7Id};
        static constexpr Square kA8{Square::kA8Id};
        static constexpr Square kB8{Square::kB8Id};
        static constexpr Square kC8{Square::kC8Id};
        static constexpr Square kD8{Square::kD8Id};
        static constexpr Square kE8{Square::kE8Id};
        static constexpr Square kF8{Square::kF8Id};
        static constexpr Square kG8{Square::kG8Id};
        static constexpr Square kH8{Square::kH8Id};
        static constexpr Square kNone{Square::kNoneId};

        static constexpr usize kCount = kNone.idx();
    };

    static constexpr i32 kFileA = 0;
    static constexpr i32 kFileB = 1;
    static constexpr i32 kFileC = 2;
    static constexpr i32 kFileD = 3;
    static constexpr i32 kFileE = 4;
    static constexpr i32 kFileF = 5;
    static constexpr i32 kFileG = 6;
    static constexpr i32 kFileH = 7;

    static constexpr i32 kRank1 = 0;
    static constexpr i32 kRank2 = 1;
    static constexpr i32 kRank3 = 2;
    static constexpr i32 kRank4 = 3;
    static constexpr i32 kRank5 = 4;
    static constexpr i32 kRank6 = 5;
    static constexpr i32 kRank7 = 6;
    static constexpr i32 kRank8 = 7;

    [[nodiscard]] constexpr i32 relativeRank(Color c, i32 rank) {
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
            return black() != Squares::kNone && white() != Squares::kNone && black() != white();
        }
    };

    struct CastlingRooks {
        struct RookPair {
            Square kingside{Squares::kNone};
            Square queenside{Squares::kNone};

            inline void clear() {
                kingside = Squares::kNone;
                queenside = Squares::kNone;
            }

            inline void unset(Square sq) {
                assert(sq != Squares::kNone);

                if (sq == kingside) {
                    kingside = Squares::kNone;
                } else if (sq == queenside) {
                    queenside = Squares::kNone;
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
