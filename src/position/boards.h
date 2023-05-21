/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../types.h"

#include <array>
#include <iostream>

#include "../bitboard.h"

namespace polaris
{
	class PositionBoards
	{
	public:
		PositionBoards() = default;
		~PositionBoards() = default;

		[[nodiscard]] inline Bitboard &forPiece(BasePiece piece)
		{
			return m_boards[static_cast<i32>(piece)];
		}

		[[nodiscard]] inline Bitboard forPiece(BasePiece piece) const
		{
			return m_boards[static_cast<i32>(piece)];
		}

		[[nodiscard]] inline Bitboard forPiece(BasePiece piece, Color c) const
		{
			return m_boards[static_cast<i32>(piece)] & (c == Color::Black ? m_blackPop : m_whitePop);
		}

		[[nodiscard]] inline auto &forColor(Color color)
		{
			return color == Color::Black ? m_blackPop : m_whitePop;
		}

		[[nodiscard]] inline auto forColor(Color color) const
		{
			return color == Color::Black ? m_blackPop : m_whitePop;
		}

		[[nodiscard]] inline auto blackOccupancy() const { return m_blackPop; }
		[[nodiscard]] inline auto whiteOccupancy() const { return m_whitePop; }

		template <Color C>
		[[nodiscard]] inline auto occupancy() const
		{
			if constexpr (C == Color::Black)
				return m_blackPop;
			else return m_whitePop;
		}

		[[nodiscard]] inline auto occupancy() const { return m_whitePop | m_blackPop; }

		[[nodiscard]] inline auto pawns() const { return forPiece(BasePiece::Pawn); }
		[[nodiscard]] inline auto knights() const { return forPiece(BasePiece::Knight); }
		[[nodiscard]] inline auto bishops() const { return forPiece(BasePiece::Bishop); }
		[[nodiscard]] inline auto rooks() const { return forPiece(BasePiece::Rook); }
		[[nodiscard]] inline auto queens() const { return forPiece(BasePiece::Queen); }
		[[nodiscard]] inline auto kings() const { return forPiece(BasePiece::King); }

		[[nodiscard]] inline auto blackPawns() const { return pawns() & m_blackPop; }
		[[nodiscard]] inline auto whitePawns() const { return pawns() & m_whitePop; }

		[[nodiscard]] inline auto blackKnights() const { return knights() & m_blackPop; }
		[[nodiscard]] inline auto whiteKnights() const { return knights() & m_whitePop; }

		[[nodiscard]] inline auto blackBishops() const { return bishops() & m_blackPop; }
		[[nodiscard]] inline auto whiteBishops() const { return bishops() & m_whitePop; }

		[[nodiscard]] inline auto blackRooks() const { return rooks() & m_blackPop; }
		[[nodiscard]] inline auto whiteRooks() const { return rooks() & m_whitePop; }

		[[nodiscard]] inline auto blackQueens() const { return queens() & m_blackPop; }
		[[nodiscard]] inline auto whiteQueens() const { return queens() & m_whitePop; }

		[[nodiscard]] inline auto blackKings() const { return kings() & m_blackPop; }
		[[nodiscard]] inline auto whiteKings() const { return kings() & m_whitePop; }

		[[nodiscard]] inline auto minors() const
		{
			return knights() | bishops();
		}

		[[nodiscard]] inline auto blackMinors() const
		{
			return minors() & m_blackPop;
		}

		[[nodiscard]] inline auto whiteMinors() const
		{
			return minors() & m_whitePop;
		}

		[[nodiscard]] inline auto majors() const
		{
			return rooks() | queens();
		}

		[[nodiscard]] inline auto blackMajors() const
		{
			return majors() & m_blackPop;
		}

		[[nodiscard]] inline auto whiteMajors() const
		{
			return majors() & m_whitePop;
		}

		[[nodiscard]] inline auto nonPk() const
		{
			return minors() | majors();
		}

		[[nodiscard]] inline auto blackNonPk() const
		{
			return nonPk() & m_blackPop;
		}

		[[nodiscard]] inline auto whiteNonPk() const
		{
			return nonPk() & m_whitePop;
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
			return forPiece(BasePiece::Pawn, color);
		}

		[[nodiscard]] inline auto knights(Color color) const
		{
			return forPiece(BasePiece::Knight, color);
		}

		[[nodiscard]] inline auto bishops(Color color) const
		{
			return forPiece(BasePiece::Bishop, color);
		}

		[[nodiscard]] inline auto rooks(Color color) const
		{
			return forPiece(BasePiece::Rook, color);
		}

		[[nodiscard]] inline auto queens(Color color) const
		{
			return forPiece(BasePiece::Queen, color);
		}

		[[nodiscard]] inline auto kings(Color color) const
		{
			return forPiece(BasePiece::King, color);
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

		[[nodiscard]] inline auto pieceAt(Square square) const
		{
			const auto bit = Bitboard::fromSquare(square);

			Color color;

			if (!(m_blackPop & bit).empty())
				color = Color::Black;
			else if (!(m_whitePop & bit).empty())
				color = Color::White;
			else return Piece::None;

			for (const auto piece : {
				BasePiece::Pawn,
				BasePiece::Knight,
				BasePiece::Bishop,
				BasePiece::Rook,
				BasePiece::Queen,
				BasePiece::King
			})
			{
				if (!(forPiece(piece) & bit).empty())
					return colorPiece(piece, color);
			}

			std::cerr << "bit set in " << (color == Color::Black ? "black" : "white")
				<< " occupancy bitboard but no piece found" << std::endl;

			assert(false);
			__builtin_unreachable();
		}

		[[nodiscard]] inline auto pieceAt(u32 rank, u32 file) const { return pieceAt(toSquare(rank, file)); }

		inline void setPiece(Square square, Piece piece)
		{
			const auto mask = Bitboard::fromSquare(square);

			forPiece(basePiece(piece)) ^= mask;
			forColor(pieceColor(piece)) ^= mask;
		}

		inline void movePiece(Square src, Square dst, Piece piece)
		{
			const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);

			forPiece(basePiece(piece)) ^= mask;
			forColor(pieceColor(piece)) ^= mask;
		}

		inline void moveAndChangePiece(Square src, Square dst, Piece moving, BasePiece target)
		{
			forPiece(basePiece(moving))[src] = false;
			forPiece(target)[dst] = true;

			const auto mask = Bitboard::fromSquare(src) | Bitboard::fromSquare(dst);
			forColor(pieceColor(moving)) ^= mask;
		}

		inline void removePiece(Square square, Piece piece)
		{
			forPiece(basePiece(piece))[square] = false;
			forColor(pieceColor(piece))[square] = false;
		}

		[[nodiscard]] inline bool operator==(const PositionBoards &other) const = default;

	private:
		std::array<Bitboard, 6> m_boards{};

		Bitboard m_blackPop{};
		Bitboard m_whitePop{};
	};
}
