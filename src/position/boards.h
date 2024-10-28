/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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
#include <iostream>

#include "../bitboard.h"

namespace stormphrax
{
	class BitboardSet
	{
	public:
		BitboardSet() = default;
		~BitboardSet() = default;

		[[nodiscard]] inline auto forColor(Color color) -> auto &
		{
			return m_colors[color.idx()];
		}

		[[nodiscard]] inline auto forColor(Color color) const
		{
			return m_colors[color.idx()];
		}

		[[nodiscard]] inline auto forPiece(PieceType piece) -> Bitboard &
		{
			return m_pieces[piece.idx()];
		}

		[[nodiscard]] inline auto forPiece(PieceType piece) const
		{
			return m_pieces[piece.idx()];
		}

		[[nodiscard]] inline auto forPiece(PieceType piece, Color c) const
		{
			return m_pieces[piece.idx()] & forColor(c);
		}

		[[nodiscard]] inline auto forPiece(Piece piece) const
		{
			return forPiece(piece.type(), piece.color());
		}

		[[nodiscard]] inline auto blackOccupancy() const { return m_colors[0]; }
		[[nodiscard]] inline auto whiteOccupancy() const { return m_colors[1]; }

		[[nodiscard]] inline auto occupancy(Color c) const
		{
			return m_colors[c.idx()];
		}

		[[nodiscard]] inline auto occupancy() const { return m_colors[0] | m_colors[1]; }

		[[nodiscard]] inline auto pawns() const { return forPiece(piece_types::Pawn); }
		[[nodiscard]] inline auto knights() const { return forPiece(piece_types::Knight); }
		[[nodiscard]] inline auto bishops() const { return forPiece(piece_types::Bishop); }
		[[nodiscard]] inline auto rooks() const { return forPiece(piece_types::Rook); }
		[[nodiscard]] inline auto queens() const { return forPiece(piece_types::Queen); }
		[[nodiscard]] inline auto kings() const { return forPiece(piece_types::King); }

		[[nodiscard]] inline auto blackPawns() const { return pawns() & blackOccupancy(); }
		[[nodiscard]] inline auto whitePawns() const { return pawns() & whiteOccupancy(); }

		[[nodiscard]] inline auto blackKnights() const { return knights() & blackOccupancy(); }
		[[nodiscard]] inline auto whiteKnights() const { return knights() & whiteOccupancy(); }

		[[nodiscard]] inline auto blackBishops() const { return bishops() & blackOccupancy(); }
		[[nodiscard]] inline auto whiteBishops() const { return bishops() & whiteOccupancy(); }

		[[nodiscard]] inline auto blackRooks() const { return rooks() & blackOccupancy(); }
		[[nodiscard]] inline auto whiteRooks() const { return rooks() & whiteOccupancy(); }

		[[nodiscard]] inline auto blackQueens() const { return queens() & blackOccupancy(); }
		[[nodiscard]] inline auto whiteQueens() const { return queens() & whiteOccupancy(); }

		[[nodiscard]] inline auto blackKings() const { return kings() & blackOccupancy(); }
		[[nodiscard]] inline auto whiteKings() const { return kings() & whiteOccupancy(); }

		[[nodiscard]] inline auto minors() const
		{
			return knights() | bishops();
		}

		[[nodiscard]] inline auto blackMinors() const
		{
			return minors() & blackOccupancy();
		}

		[[nodiscard]] inline auto whiteMinors() const
		{
			return minors() & whiteOccupancy();
		}

		[[nodiscard]] inline auto majors() const
		{
			return rooks() | queens();
		}

		[[nodiscard]] inline auto blackMajors() const
		{
			return majors() & blackOccupancy();
		}

		[[nodiscard]] inline auto whiteMajors() const
		{
			return majors() & whiteOccupancy();
		}

		[[nodiscard]] inline auto nonPk() const
		{
			return occupancy() ^ pawns() ^ kings();
		}

		[[nodiscard]] inline auto blackNonPk() const
		{
			return blackOccupancy() ^ (pawns() | kings()) & blackOccupancy();
		}

		[[nodiscard]] inline auto whiteNonPk() const
		{
			return whiteOccupancy() ^ (pawns() | kings()) & whiteOccupancy();
		}

		[[nodiscard]] inline auto pawns(Color color) const
		{
			return forPiece(piece_types::Pawn, color);
		}

		[[nodiscard]] inline auto knights(Color color) const
		{
			return forPiece(piece_types::Knight, color);
		}

		[[nodiscard]] inline auto bishops(Color color) const
		{
			return forPiece(piece_types::Bishop, color);
		}

		[[nodiscard]] inline auto rooks(Color color) const
		{
			return forPiece(piece_types::Rook, color);
		}

		[[nodiscard]] inline auto queens(Color color) const
		{
			return forPiece(piece_types::Queen, color);
		}

		[[nodiscard]] inline auto kings(Color color) const
		{
			return forPiece(piece_types::King, color);
		}

		[[nodiscard]] inline auto minors(Color color) const
		{
			return color == colors::Black ? blackMinors() : whiteMinors();
		}

		[[nodiscard]] inline auto majors(Color color) const
		{
			return color == colors::Black ? blackMajors() : whiteMajors();
		}

		[[nodiscard]] inline auto nonPk(Color color) const
		{
			return color == colors::Black ? blackNonPk() : whiteNonPk();
		}

		[[nodiscard]] inline auto operator==(const BitboardSet &other) const -> bool = default;

	private:
		std::array<Bitboard, 2> m_colors{};
		std::array<Bitboard, 6> m_pieces{};
	};

	class PositionBoards
	{
	public:
		PositionBoards()
		{
			m_mailbox.fill(pieces::None);
		}

		~PositionBoards() = default;

		[[nodiscard]] inline auto bbs() const -> const auto &
		{
			return m_bbs;
		}

		[[nodiscard]] inline auto bbs() -> auto &
		{
			return m_bbs;
		}

		[[nodiscard]] inline auto pieceTypeAt(Square square) const
		{
			assert(square != squares::None);

			const auto piece = m_mailbox[square.idx()];
			return piece == pieces::None ? piece_types::None : piece.type();
		}

		[[nodiscard]] inline auto pieceAt(Square square) const
		{
			assert(square != squares::None);
			return m_mailbox[square.idx()];
		}

		[[nodiscard]] inline auto pieceAt(u32 rank, u32 file) const
		{
			return pieceAt(Square::fromRankFile(rank, file));
		}

		inline auto setPiece(Square square, Piece piece)
		{
			assert(square != squares::None);
			assert(piece != pieces::None);

			assert(pieceAt(square) == pieces::None);

			slot(square) = piece;

			const auto mask = Bitboard::fromSquare(square);

			m_bbs.forPiece(piece.type()) ^= mask;
			m_bbs.forColor(piece.color()) ^= mask;
		}

		inline auto movePiece(Square src, Square dst, Piece piece)
		{
			assert(src != squares::None);
			assert(dst != squares::None);

			if (slot(src) == piece) [[likely]]
				slot(src) = pieces::None;
			slot(dst) = piece;

			const auto mask = Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst);

			m_bbs.forPiece(piece.type()) ^= mask;
			m_bbs.forColor(piece.color()) ^= mask;
		}

		inline auto moveAndChangePiece(Square src, Square dst, Piece moving, PieceType promo)
		{
			assert(src != squares::None);
			assert(dst != squares::None);
			assert(src != dst);

			assert(moving != pieces::None);
			assert(promo != piece_types::None);

			assert(pieceAt(src) == moving);
			assert(slot(src) == moving);

			slot(src) = pieces::None;
			slot(dst) = moving.copyColorTo(promo);

			m_bbs.forPiece(moving.type())[src] = false;
			m_bbs.forPiece(promo)[dst] = true;

			const auto mask = Bitboard::fromSquare(src) ^ Bitboard::fromSquare(dst);
			m_bbs.forColor(moving.color()) ^= mask;
		}

		inline auto removePiece(Square square, Piece piece)
		{
			assert(square != squares::None);
			assert(piece != pieces::None);

			assert(pieceAt(square) == piece);

			slot(square) = pieces::None;

			m_bbs.forPiece(piece.type())[square] = false;
			m_bbs.forColor(piece.color())[square] = false;
		}

		inline auto regenFromBbs()
		{
			m_mailbox.fill(pieces::None);

			for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx)
			{
				const auto piece = Piece::fromRaw(pieceIdx);

				auto board = m_bbs.forPiece(piece);
				while (!board.empty())
				{
					const auto sq = board.popLowestSquare();
					assert(slot(sq) == pieces::None);
					slot(sq) = piece;
				}
			}
		}

		[[nodiscard]] inline auto operator==(const PositionBoards &other) const -> bool = default;

	private:
		[[nodiscard]] inline auto slot(Square square) -> Piece &
		{
			return m_mailbox[square.idx()];
		}

		BitboardSet m_bbs{};
		std::array<Piece, 64> m_mailbox{};
	};
}
