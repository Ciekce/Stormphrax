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

#include "../bitboard.h"

namespace stormphrax {
    class BitboardSet {
    public:
        BitboardSet() = default;
        ~BitboardSet() = default;

        [[nodiscard]] inline Bitboard& forColor(Color color) {
            return m_colors[static_cast<i32>(color)];
        }

        [[nodiscard]] inline Bitboard forColor(Color color) const {
            return m_colors[static_cast<i32>(color)];
        }

        [[nodiscard]] inline Bitboard& forPiece(PieceType piece) {
            return m_pieces[static_cast<i32>(piece)];
        }

        [[nodiscard]] inline Bitboard forPiece(PieceType piece) const {
            return m_pieces[static_cast<i32>(piece)];
        }

        [[nodiscard]] inline Bitboard forPiece(PieceType piece, Color c) const {
            return m_pieces[static_cast<i32>(piece)] & forColor(c);
        }

        [[nodiscard]] inline Bitboard forPiece(Piece piece) const {
            return forPiece(pieceType(piece), pieceColor(piece));
        }

        [[nodiscard]] inline Bitboard blackOccupancy() const {
            return m_colors[0];
        }

        [[nodiscard]] inline Bitboard whiteOccupancy() const {
            return m_colors[1];
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard occupancy() const {
            return m_colors[static_cast<i32>(kC)];
        }

        [[nodiscard]] inline Bitboard occupancy(Color c) const {
            return m_colors[static_cast<i32>(c)];
        }

        [[nodiscard]] inline Bitboard occupancy() const {
            return m_colors[0] | m_colors[1];
        }

        [[nodiscard]] inline Bitboard pawns() const {
            return forPiece(PieceType::kPawn);
        }

        [[nodiscard]] inline Bitboard knights() const {
            return forPiece(PieceType::kKnight);
        }

        [[nodiscard]] inline Bitboard bishops() const {
            return forPiece(PieceType::kBishop);
        }

        [[nodiscard]] inline Bitboard rooks() const {
            return forPiece(PieceType::kRook);
        }

        [[nodiscard]] inline Bitboard queens() const {
            return forPiece(PieceType::kQueen);
        }

        [[nodiscard]] inline Bitboard kings() const {
            return forPiece(PieceType::kKing);
        }

        [[nodiscard]] inline Bitboard blackPawns() const {
            return pawns() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whitePawns() const {
            return pawns() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard blackKnights() const {
            return knights() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteKnights() const {
            return knights() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard blackBishops() const {
            return bishops() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteBishops() const {
            return bishops() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard blackRooks() const {
            return rooks() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteRooks() const {
            return rooks() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard blackQueens() const {
            return queens() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteQueens() const {
            return queens() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard blackKings() const {
            return kings() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteKings() const {
            return kings() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard minors() const {
            return knights() | bishops();
        }

        [[nodiscard]] inline Bitboard blackMinors() const {
            return minors() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteMinors() const {
            return minors() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard majors() const {
            return rooks() | queens();
        }

        [[nodiscard]] inline Bitboard blackMajors() const {
            return majors() & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteMajors() const {
            return majors() & whiteOccupancy();
        }

        [[nodiscard]] inline Bitboard nonPk() const {
            return occupancy() ^ pawns() ^ kings();
        }

        [[nodiscard]] inline Bitboard blackNonPk() const {
            return blackOccupancy() ^ (pawns() | kings()) & blackOccupancy();
        }

        [[nodiscard]] inline Bitboard whiteNonPk() const {
            return whiteOccupancy() ^ (pawns() | kings()) & whiteOccupancy();
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard pawns() const {
            if constexpr (kC == Color::kBlack) {
                return blackPawns();
            } else {
                return whitePawns();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard knights() const {
            if constexpr (kC == Color::kBlack) {
                return blackKnights();
            } else {
                return whiteKnights();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard bishops() const {
            if constexpr (kC == Color::kBlack) {
                return blackBishops();
            } else {
                return whiteBishops();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard rooks() const {
            if constexpr (kC == Color::kBlack) {
                return blackRooks();
            } else {
                return whiteRooks();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard queens() const {
            if constexpr (kC == Color::kBlack) {
                return blackQueens();
            } else {
                return whiteQueens();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard kings() const {
            if constexpr (kC == Color::kBlack) {
                return blackKings();
            } else {
                return whiteKings();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard minors() const {
            if constexpr (kC == Color::kBlack) {
                return blackMinors();
            } else {
                return whiteMinors();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard majors() const {
            if constexpr (kC == Color::kBlack) {
                return blackMajors();
            } else {
                return whiteMajors();
            }
        }

        template <Color kC>
        [[nodiscard]] inline Bitboard nonPk() const {
            if constexpr (kC == Color::kBlack) {
                return blackNonPk();
            } else {
                return whiteNonPk();
            }
        }

        [[nodiscard]] inline Bitboard pawns(Color color) const {
            return forPiece(PieceType::kPawn, color);
        }

        [[nodiscard]] inline Bitboard knights(Color color) const {
            return forPiece(PieceType::kKnight, color);
        }

        [[nodiscard]] inline Bitboard bishops(Color color) const {
            return forPiece(PieceType::kBishop, color);
        }

        [[nodiscard]] inline Bitboard rooks(Color color) const {
            return forPiece(PieceType::kRook, color);
        }

        [[nodiscard]] inline Bitboard queens(Color color) const {
            return forPiece(PieceType::kQueen, color);
        }

        [[nodiscard]] inline Bitboard kings(Color color) const {
            return forPiece(PieceType::kKing, color);
        }

        [[nodiscard]] inline Bitboard minors(Color color) const {
            return color == Color::kBlack ? blackMinors() : whiteMinors();
        }

        [[nodiscard]] inline Bitboard majors(Color color) const {
            return color == Color::kBlack ? blackMajors() : whiteMajors();
        }

        [[nodiscard]] inline Bitboard nonPk(Color color) const {
            return color == Color::kBlack ? blackNonPk() : whiteNonPk();
        }

        [[nodiscard]] inline bool operator==(const BitboardSet& other) const = default;

    private:
        std::array<Bitboard, 2> m_colors{};
        std::array<Bitboard, 6> m_pieces{};
    };

    class PositionBoards {
    public:
        PositionBoards() {
            m_mailbox.fill(Piece::kNone);
        }

        ~PositionBoards() = default;

        [[nodiscard]] inline const BitboardSet& bbs() const {
            return m_bbs;
        }

        [[nodiscard]] inline BitboardSet& bbs() {
            return m_bbs;
        }

        [[nodiscard]] inline PieceType pieceTypeAt(Square square) const {
            assert(square != Square::kNone);

            const auto piece = m_mailbox[static_cast<i32>(square)];
            return piece == Piece::kNone ? PieceType::kNone : pieceType(piece);
        }

        [[nodiscard]] inline Piece pieceAt(Square square) const {
            assert(square != Square::kNone);
            return m_mailbox[static_cast<i32>(square)];
        }

        [[nodiscard]] inline Piece pieceAt(u32 rank, u32 file) const {
            return pieceAt(toSquare(rank, file));
        }

        inline void setPiece(Square square, Piece piece) {
            assert(square != Square::kNone);
            assert(piece != Piece::kNone);

            assert(pieceAt(square) == Piece::kNone);

            slot(square) = piece;

            const auto mask = Bitboard::fromSquare(square);

            m_bbs.forPiece(pieceType(piece)) ^= mask;
            m_bbs.forColor(pieceColor(piece)) ^= mask;
        }

        inline void movePiece(Square src, Square dst, Piece piece) {
            assert(src != Square::kNone);
            assert(dst != Square::kNone);

            if (slot(src) == piece) {
                [[likely]] slot(src) = Piece::kNone;
            }

            slot(dst) = piece;

            const auto mask = Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst);

            m_bbs.forPiece(pieceType(piece)) ^= mask;
            m_bbs.forColor(pieceColor(piece)) ^= mask;
        }

        inline void moveAndChangePiece(Square src, Square dst, Piece moving, PieceType promo) {
            assert(src != Square::kNone);
            assert(dst != Square::kNone);
            assert(src != dst);

            assert(moving != Piece::kNone);
            assert(promo != PieceType::kNone);

            assert(pieceAt(src) == moving);
            assert(slot(src) == moving);

            slot(src) = Piece::kNone;
            slot(dst) = copyPieceColor(moving, promo);

            m_bbs.forPiece(pieceType(moving))[src] = false;
            m_bbs.forPiece(promo)[dst] = true;

            const auto mask = Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst);
            m_bbs.forColor(pieceColor(moving)) ^= mask;
        }

        inline void removePiece(Square square, Piece piece) {
            assert(square != Square::kNone);
            assert(piece != Piece::kNone);

            assert(pieceAt(square) == piece);

            slot(square) = Piece::kNone;

            m_bbs.forPiece(pieceType(piece))[square] = false;
            m_bbs.forColor(pieceColor(piece))[square] = false;
        }

        inline void regenFromBbs() {
            m_mailbox.fill(Piece::kNone);

            for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx) {
                const auto piece = static_cast<Piece>(pieceIdx);

                auto board = m_bbs.forPiece(piece);
                while (!board.empty()) {
                    const auto sq = board.popLowestSquare();
                    assert(slot(sq) == Piece::kNone);
                    slot(sq) = piece;
                }
            }
        }

        [[nodiscard]] inline bool operator==(const PositionBoards& other) const = default;

    private:
        [[nodiscard]] inline Piece& slot(Square square) {
            return m_mailbox[static_cast<i32>(square)];
        }

        BitboardSet m_bbs{};
        std::array<Piece, 64> m_mailbox{};
    };
} // namespace stormphrax
