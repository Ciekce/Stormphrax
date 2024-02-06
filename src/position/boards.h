/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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
	class PositionBoards
	{
	public:
		PositionBoards() = default;
		~PositionBoards() = default;

		[[nodiscard]] inline auto forColor(Color color) -> auto &
		{
			return m_colors[static_cast<i32>(color)];
		}

		[[nodiscard]] inline auto forColor(Color color) const
		{
			return m_colors[static_cast<i32>(color)];
		}

		[[nodiscard]] inline auto forPiece(PieceType piece) -> Bitboard &
		{
			return m_boards[static_cast<i32>(piece)];
		}

		[[nodiscard]] inline auto forPiece(PieceType piece) const
		{
			return m_boards[static_cast<i32>(piece)];
		}

		[[nodiscard]] inline auto forPiece(PieceType piece, Color c) const
		{
			return m_boards[static_cast<i32>(piece)] & forColor(c);
		}

		[[nodiscard]] inline auto forPiece(Piece piece) const
		{
			return forPiece(pieceType(piece), pieceColor(piece));
		}

		[[nodiscard]] inline auto blackOccupancy() const { return m_colors[0]; }
		[[nodiscard]] inline auto whiteOccupancy() const { return m_colors[1]; }

		template <Color C>
		[[nodiscard]] inline auto occupancy() const
		{
			return m_colors[static_cast<i32>(C)];
		}

		[[nodiscard]] inline auto occupancy(Color c) const
		{
			return m_colors[static_cast<i32>(c)];
		}

		[[nodiscard]] inline auto occupancy() const { return m_colors[0] | m_colors[1]; }

		[[nodiscard]] inline auto pawns() const { return forPiece(PieceType::Pawn); }
		[[nodiscard]] inline auto knights() const { return forPiece(PieceType::Knight); }
		[[nodiscard]] inline auto bishops() const { return forPiece(PieceType::Bishop); }
		[[nodiscard]] inline auto rooks() const { return forPiece(PieceType::Rook); }
		[[nodiscard]] inline auto queens() const { return forPiece(PieceType::Queen); }
		[[nodiscard]] inline auto kings() const { return forPiece(PieceType::King); }

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

		template <Color C>
		[[nodiscard]] inline auto pawns() const
		{
			if constexpr (C == Color::Black)
				return blackPawns();
			else return whitePawns();
		}

		template <Color C>
		[[nodiscard]] inline auto knights() const
		{
			if constexpr (C == Color::Black)
				return blackKnights();
			else return whiteKnights();
		}

		template <Color C>
		[[nodiscard]] inline auto bishops() const
		{
			if constexpr (C == Color::Black)
				return blackBishops();
			else return whiteBishops();
		}

		template <Color C>
		[[nodiscard]] inline auto rooks() const
		{
			if constexpr (C == Color::Black)
				return blackRooks();
			else return whiteRooks();
		}

		template <Color C>
		[[nodiscard]] inline auto queens() const
		{
			if constexpr (C == Color::Black)
				return blackQueens();
			else return whiteQueens();
		}

		template <Color C>
		[[nodiscard]] inline auto kings() const
		{
			if constexpr (C == Color::Black)
				return blackKings();
			else return whiteKings();
		}

		template <Color C>
		[[nodiscard]] inline auto minors() const
		{
			if constexpr (C == Color::Black)
				return blackMinors();
			else return whiteMinors();
		}

		template <Color C>
		[[nodiscard]] inline auto majors() const
		{
			if constexpr (C == Color::Black)
				return blackMajors();
			else return whiteMajors();
		}

		template <Color C>
		[[nodiscard]] inline auto nonPk() const
		{
			if constexpr (C == Color::Black)
				return blackNonPk();
			else return whiteNonPk();
		}

		[[nodiscard]] inline auto pawns(Color color) const
		{
			return forPiece(PieceType::Pawn, color);
		}

		[[nodiscard]] inline auto knights(Color color) const
		{
			return forPiece(PieceType::Knight, color);
		}

		[[nodiscard]] inline auto bishops(Color color) const
		{
			return forPiece(PieceType::Bishop, color);
		}

		[[nodiscard]] inline auto rooks(Color color) const
		{
			return forPiece(PieceType::Rook, color);
		}

		[[nodiscard]] inline auto queens(Color color) const
		{
			return forPiece(PieceType::Queen, color);
		}

		[[nodiscard]] inline auto kings(Color color) const
		{
			return forPiece(PieceType::King, color);
		}

		[[nodiscard]] inline auto minors(Color color) const
		{
			return color == Color::Black ? blackMinors() : whiteMinors();
		}

		[[nodiscard]] inline auto majors(Color color) const
		{
			return color == Color::Black ? blackMajors() : whiteMajors();
		}

		[[nodiscard]] inline auto nonPk(Color color) const
		{
			return color == Color::Black ? blackNonPk() : whiteNonPk();
		}

		[[nodiscard]] inline auto pieceTypeAt(Square square) const
		{
			const auto bit = Bitboard::fromSquare(square);

			for (const auto piece : {
				PieceType::Pawn,
				PieceType::Knight,
				PieceType::Bishop,
				PieceType::Rook,
				PieceType::Queen,
				PieceType::King
			})
			{
				if (!(forPiece(piece) & bit).empty())
					return piece;
			}

			return PieceType::None;
		}

		[[nodiscard]] inline auto pieceAt(Square square) const
		{
			const auto bit = Bitboard::fromSquare(square);

			Color color;

			if (!(blackOccupancy() & bit).empty())
				color = Color::Black;
			else if (!(whiteOccupancy() & bit).empty())
				color = Color::White;
			else return Piece::None;

			for (const auto piece : {
				PieceType::Pawn,
				PieceType::Knight,
				PieceType::Bishop,
				PieceType::Rook,
				PieceType::Queen,
				PieceType::King
			})
			{
				if (!(forPiece(piece) & bit).empty())
					return colorPiece(piece, color);
			}

			std::cerr << "bit set in " << (color == Color::Black ? "black" : "white")
				<< " occupancy bitboard but no piece found" << std::endl;

			assert(false);
			std::terminate();
		}

		[[nodiscard]] inline auto pieceAt(u32 rank, u32 file) const { return pieceAt(toSquare(rank, file)); }

		inline auto setPiece(Square square, Piece piece)
		{
			assert(square != Square::None);
			assert(piece != Piece::None);

			assert(pieceAt(square) == Piece::None);

			const auto mask = Bitboard::fromSquare(square);

			forPiece(pieceType(piece)) ^= mask;
			forColor(pieceColor(piece)) ^= mask;
		}

		inline auto movePiece(Square src, Square dst, Piece piece)
		{
			assert(src != Square::None);
			assert(dst != Square::None);
			assert(src != dst);

			assert(pieceAt(src) == piece);

			const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);

			forPiece(pieceType(piece)) ^= mask;
			forColor(pieceColor(piece)) ^= mask;
		}

		inline auto moveAndChangePiece(Square src, Square dst, Piece moving, PieceType promo)
		{
			assert(src != Square::None);
			assert(dst != Square::None);
			assert(src != dst);

			assert(moving != Piece::None);
			assert(promo != PieceType::None);

			assert(pieceAt(src) == moving);

			forPiece(pieceType(moving))[src] = false;
			forPiece(promo)[dst] = true;

			const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);
			forColor(pieceColor(moving)) ^= mask;
		}

		inline auto removePiece(Square square, Piece piece)
		{
			assert(square != Square::None);
			assert(piece != Piece::None);

			assert(pieceAt(square) == piece);

			forPiece(pieceType(piece))[square] = false;
			forColor(pieceColor(piece))[square] = false;
		}

		[[nodiscard]] inline auto operator==(const PositionBoards &other) const -> bool = default;

	private:
		std::array<Bitboard, 2> m_colors{};
		std::array<Bitboard, 6> m_boards{};
	};
}
