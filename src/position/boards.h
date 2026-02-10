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
        [[nodiscard]] inline Bitboard& forColor(Color color) {
            return m_colors[color.idx()];
        }

        [[nodiscard]] inline Bitboard forColor(Color color) const {
            return m_colors[color.idx()];
        }

        [[nodiscard]] inline Bitboard& forPiece(PieceType pt) {
            return m_pieces[pt.idx()];
        }

        [[nodiscard]] inline Bitboard forPiece(PieceType pt) const {
            return m_pieces[pt.idx()];
        }

        [[nodiscard]] inline Bitboard forPiece(PieceType pt, Color c) const {
            return m_pieces[pt.idx()] & forColor(c);
        }

        [[nodiscard]] inline Bitboard forPiece(Piece piece) const {
            return forPiece(piece.type(), piece.color());
        }

        [[nodiscard]] inline Bitboard blackOccupancy() const {
            return m_colors[0];
        }

        [[nodiscard]] inline Bitboard whiteOccupancy() const {
            return m_colors[1];
        }

        [[nodiscard]] inline Bitboard occupancy(Color c) const {
            return m_colors[c.idx()];
        }

        [[nodiscard]] inline Bitboard occupancy() const {
            return m_colors[0] | m_colors[1];
        }

        [[nodiscard]] inline Bitboard pawns() const {
            return forPiece(PieceTypes::kPawn);
        }

        [[nodiscard]] inline Bitboard knights() const {
            return forPiece(PieceTypes::kKnight);
        }

        [[nodiscard]] inline Bitboard bishops() const {
            return forPiece(PieceTypes::kBishop);
        }

        [[nodiscard]] inline Bitboard rooks() const {
            return forPiece(PieceTypes::kRook);
        }

        [[nodiscard]] inline Bitboard queens() const {
            return forPiece(PieceTypes::kQueen);
        }

        [[nodiscard]] inline Bitboard kings() const {
            return forPiece(PieceTypes::kKing);
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

        [[nodiscard]] inline Bitboard pawns(Color color) const {
            return forPiece(PieceTypes::kPawn, color);
        }

        [[nodiscard]] inline Bitboard knights(Color color) const {
            return forPiece(PieceTypes::kKnight, color);
        }

        [[nodiscard]] inline Bitboard bishops(Color color) const {
            return forPiece(PieceTypes::kBishop, color);
        }

        [[nodiscard]] inline Bitboard rooks(Color color) const {
            return forPiece(PieceTypes::kRook, color);
        }

        [[nodiscard]] inline Bitboard queens(Color color) const {
            return forPiece(PieceTypes::kQueen, color);
        }

        [[nodiscard]] inline Bitboard kings(Color color) const {
            return forPiece(PieceTypes::kKing, color);
        }

        [[nodiscard]] inline Bitboard minors(Color color) const {
            return color == Colors::kBlack ? blackMinors() : whiteMinors();
        }

        [[nodiscard]] inline Bitboard majors(Color color) const {
            return color == Colors::kBlack ? blackMajors() : whiteMajors();
        }

        [[nodiscard]] inline Bitboard nonPk(Color color) const {
            return color == Colors::kBlack ? blackNonPk() : whiteNonPk();
        }

        [[nodiscard]] inline bool operator==(const BitboardSet& other) const = default;

    private:
        std::array<Bitboard, 2> m_colors{};
        std::array<Bitboard, 6> m_pieces{};
    };

    class PositionBoards {
    public:
        PositionBoards() {
            m_mailbox.fill(Pieces::kNone);
        }

        [[nodiscard]] inline const BitboardSet& bbs() const {
            return m_bbs;
        }

        [[nodiscard]] inline BitboardSet& bbs() {
            return m_bbs;
        }

        [[nodiscard]] inline PieceType pieceTypeAt(Square sq) const {
            assert(sq != Squares::kNone);

            const auto piece = m_mailbox[sq.idx()];
            return piece == Pieces::kNone ? PieceTypes::kNone : piece.type();
        }

        [[nodiscard]] inline Piece pieceOn(Square sq) const {
            assert(sq != Squares::kNone);
            return m_mailbox[sq.idx()];
        }

        [[nodiscard]] inline Piece pieceOn(i32 rank, i32 file) const {
            return pieceOn(Square::fromFileRank(file, rank));
        }

        inline void setPiece(Square sq, Piece piece) {
            assert(sq != Squares::kNone);
            assert(piece != Pieces::kNone);

            assert(pieceOn(sq) == Pieces::kNone);

            slot(sq) = piece;

            const auto mask = Bitboard::fromSquare(sq);

            m_bbs.forPiece(piece.type()) ^= mask;
            m_bbs.forColor(piece.color()) ^= mask;
        }

        inline void movePiece(Square src, Square dst, Piece piece) {
            assert(src != Squares::kNone);
            assert(dst != Squares::kNone);

            if (slot(src) == piece) {
                [[likely]] slot(src) = Pieces::kNone;
            }

            slot(dst) = piece;

            const auto mask = Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst);

            m_bbs.forPiece(piece.type()) ^= mask;
            m_bbs.forColor(piece.color()) ^= mask;
        }

        inline void moveAndChangePiece(Square src, Square dst, Piece moving, PieceType promo) {
            assert(src != Squares::kNone);
            assert(dst != Squares::kNone);
            assert(src != dst);

            assert(moving != Pieces::kNone);
            assert(promo != PieceTypes::kNone);

            assert(pieceOn(src) == moving);
            assert(slot(src) == moving);

            slot(src) = Pieces::kNone;
            slot(dst) = moving.copyColor(promo);

            m_bbs.forPiece(moving.type())[src] = false;
            m_bbs.forPiece(promo)[dst] = true;

            const auto mask = Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst);
            m_bbs.forColor(moving.color()) ^= mask;
        }

        inline void removePiece(Square sq, Piece piece) {
            assert(sq != Squares::kNone);
            assert(piece != Pieces::kNone);

            assert(pieceOn(sq) == piece);

            slot(sq) = Pieces::kNone;

            m_bbs.forPiece(piece.type())[sq] = false;
            m_bbs.forColor(piece.color())[sq] = false;
        }

        inline void regenFromBbs() {
            m_mailbox.fill(Pieces::kNone);

            for (u32 pieceIdx = 0; pieceIdx < Pieces::kCount; ++pieceIdx) {
                const auto piece = Piece::fromRaw(pieceIdx);

                auto board = m_bbs.forPiece(piece);
                while (!board.empty()) {
                    const auto sq = board.popLowestSquare();
                    assert(slot(sq) == Pieces::kNone);
                    slot(sq) = piece;
                }
            }
        }

        [[nodiscard]] inline bool operator==(const PositionBoards& other) const = default;

    private:
        [[nodiscard]] inline Piece& slot(Square sq) {
            return m_mailbox[sq.idx()];
        }

        BitboardSet m_bbs{};
        std::array<Piece, Squares::kCount> m_mailbox{};
    };
} // namespace stormphrax
