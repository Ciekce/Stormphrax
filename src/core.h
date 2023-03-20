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

#include "types.h"

#include <cmath>
#include <utility>

#include "util/bitfield.h"
#include "util/cemath.h"

namespace polaris
{
	enum class Piece : u8
	{
		BlackPawn = 0,
		WhitePawn,
		BlackKnight,
		WhiteKnight,
		BlackBishop,
		WhiteBishop,
		BlackRook,
		WhiteRook,
		BlackQueen,
		WhiteQueen,
		BlackKing,
		WhiteKing,
		None
	};

	enum class BasePiece
	{
		Pawn = 0,
		Knight,
		Bishop,
		Rook,
		Queen,
		King,
		None
	};

	enum class Color : i8
	{
		None = 0,
		Black = -1,
		White = 1
	};

	[[nodiscard]] constexpr auto oppColor(Color color)
	{
		return static_cast<Color>(-static_cast<i32>(color));
	}

	[[nodiscard]] constexpr auto colorPiece(BasePiece piece, Color color)
	{
#ifndef NDEBUG
		if (piece == BasePiece::None || color == Color::None)
			return Piece::None;
#endif

		return static_cast<Piece>((static_cast<i32>(piece) << 1) + (color == Color::White ? 1 : 0));
	}

	[[nodiscard]] constexpr auto basePiece(Piece piece)
	{
#ifndef NDEBUG
		if (piece == Piece::None)
			return BasePiece::None;
#endif

		return static_cast<BasePiece>(static_cast<i32>(piece) >> 1);
	}

	[[nodiscard]] constexpr auto pieceColor(Piece piece)
	{
#ifndef NDEBUG
		if (piece == Piece::None)
			return Color::None;
#endif

		return (static_cast<i32>(piece) % 2 == 0) ? Color::Black : Color::White;
	}

	[[nodiscard]] constexpr auto flipPieceColor(Piece piece)
	{
#ifndef NDEBUG
		if (piece == Piece::None)
			return Piece::None;
#endif

		return static_cast<Piece>(static_cast<i32>(piece) ^ 0x1);
	}

	[[nodiscard]] constexpr auto pieceFromChar(char c)
	{
		switch (c)
		{
		case 'p': return Piece::  BlackPawn;
		case 'P': return Piece::  WhitePawn;
		case 'n': return Piece::BlackKnight;
		case 'N': return Piece::WhiteKnight;
		case 'b': return Piece::BlackBishop;
		case 'B': return Piece::WhiteBishop;
		case 'r': return Piece::  BlackRook;
		case 'R': return Piece::  WhiteRook;
		case 'q': return Piece:: BlackQueen;
		case 'Q': return Piece:: WhiteQueen;
		case 'k': return Piece::  BlackKing;
		case 'K': return Piece::  WhiteKing;
		default : return Piece::       None;
		}
	}

	[[nodiscard]] constexpr auto pieceToChar(Piece piece)
	{
		switch (piece)
		{
		case Piece::       None: return ' ';
		case Piece::  BlackPawn: return 'p';
		case Piece::  WhitePawn: return 'P';
		case Piece::BlackKnight: return 'n';
		case Piece::WhiteKnight: return 'N';
		case Piece::BlackBishop: return 'b';
		case Piece::WhiteBishop: return 'B';
		case Piece::  BlackRook: return 'r';
		case Piece::  WhiteRook: return 'R';
		case Piece:: BlackQueen: return 'q';
		case Piece:: WhiteQueen: return 'Q';
		case Piece::  BlackKing: return 'k';
		case Piece::  WhiteKing: return 'K';
		default: return ' ';
		}
	}

	[[nodiscard]] constexpr auto basePieceFromChar(char c)
	{
		switch (c)
		{
		case 'p': return BasePiece::  Pawn;
		case 'n': return BasePiece::Knight;
		case 'b': return BasePiece::Bishop;
		case 'r': return BasePiece::  Rook;
		case 'q': return BasePiece:: Queen;
		case 'k': return BasePiece::  King;
		default : return BasePiece::  None;
		}
	}

	[[nodiscard]] constexpr auto basePieceToChar(BasePiece piece)
	{
		switch (piece)
		{
		case BasePiece::  None: return ' ';
		case BasePiece::  Pawn: return 'p';
		case BasePiece::Knight: return 'n';
		case BasePiece::Bishop: return 'b';
		case BasePiece::  Rook: return 'r';
		case BasePiece:: Queen: return 'q';
		case BasePiece::  King: return 'k';
		default: return ' ';
		}
	}

	// upside down
	enum class Square : u8
	{
		A1, B1, C1, D1, E1, F1, G1, H1,
		A2, B2, C2, D2, E2, F2, G2, H2,
		A3, B3, C3, D3, E3, F3, G3, H3,
		A4, B4, C4, D4, E4, F4, G4, H4,
		A5, B5, C5, D5, E5, F5, G5, H5,
		A6, B6, C6, D6, E6, F6, G6, H6,
		A7, B7, C7, D7, E7, F7, G7, H7,
		A8, B8, C8, D8, E8, F8, G8, H8,
		None
	};

	[[nodiscard]] constexpr auto toSquare(u32 rank, u32 file)
	{
		return static_cast<Square>((rank << 3) | file);
	}

	[[nodiscard]] constexpr auto squareRank(Square square)
	{
		return static_cast<i32>(square) >> 3;
	}

	[[nodiscard]] constexpr auto squareFile(Square square)
	{
		return static_cast<i32>(square) & 0x7;
	}

	[[nodiscard]] constexpr auto squareRank(i32 square)
	{
		return square >> 3;
	}

	[[nodiscard]] constexpr auto squareFile(i32 square)
	{
		return square & 0x7;
	}

	[[nodiscard]] constexpr auto squareBit(Square square)
	{
		return U64(1) << static_cast<i32>(square);
	}

	[[nodiscard]] constexpr auto squareBitChecked(Square square)
	{
		if (square == Square::None)
			return U64(0);

		return U64(1) << static_cast<i32>(square);
	}

	[[nodiscard]] constexpr i32 chebyshev(Square s1, Square s2)
	{
		const auto x1 = squareFile(s1);
		const auto x2 = squareFile(s2);

		const auto y1 = squareRank(s1);
		const auto y2 = squareRank(s2);

		return std::max(util::abs(x2 - x1), util::abs(y2 - y1));
	}

	template <Color C>
	constexpr i32 relativeRank(i32 rank)
	{
		if constexpr (C == Color::Black)
			return 7 - rank;
		else return rank;
	}

	ENUM_FLAGS(u8, PositionFlags)
	{
		None = 0x00,
		BlackKingside = 0x01,
		BlackQueenside = 0x02,
		WhiteKingside = 0x04,
		WhiteQueenside = 0x08,
		BlackCastling = BlackKingside | BlackQueenside,
		WhiteCastling = WhiteKingside | WhiteQueenside,
		AllCastling = BlackCastling | WhiteCastling,
		BlackToMove = 0x10
	};

	using Score = i32;

	constexpr auto colorScore(Color color)
	{
		return static_cast<Score>(color);
	}

	struct TaperedScore
	{
		Score midgame;
		Score endgame;

		inline TaperedScore operator+(const TaperedScore &other) const
		{
			return {midgame + other.midgame,
				endgame + other.endgame};
		}

		inline TaperedScore operator+(Score other) const
		{
			return {midgame + other,
				endgame + other};
		}

		inline TaperedScore &operator+=(const TaperedScore &other)
		{
			midgame += other.midgame;
			endgame += other.endgame;

			return *this;
		}

		inline TaperedScore &operator+=(Score other)
		{
			midgame += other;
			endgame += other;

			return *this;
		}

		inline TaperedScore operator-(const TaperedScore &other) const
		{
			return {midgame - other.midgame,
				endgame - other.endgame};
		}

		inline TaperedScore operator-(Score other) const
		{
			return {midgame - other,
				endgame - other};
		}

		inline TaperedScore &operator-=(const TaperedScore &other)
		{
			midgame -= other.midgame;
			endgame -= other.endgame;

			return *this;
		}

		inline TaperedScore &operator-=(Score other)
		{
			midgame -= other;
			endgame -= other;

			return *this;
		}

		inline TaperedScore operator*(Score other) const
		{
			return {midgame * other,
				endgame * other};
		}

		inline TaperedScore &operator*=(Score other)
		{
			midgame *= other;
			endgame *= other;

			return *this;
		}

		inline TaperedScore operator-() const
		{
			return {-midgame, -endgame};
		}

		[[nodiscard]] inline bool operator==(const TaperedScore &other) const
		{
			return midgame == other.midgame && endgame == other.endgame;
		}

		[[nodiscard]] TaperedScore colored(Color color) const
		{
			return color == Color::White ? *this : -*this;
		}
	};

	inline TaperedScore operator+(Score lhs, const TaperedScore &rhs)
	{
		return {rhs.midgame + lhs, rhs.endgame + lhs};
	}

	inline TaperedScore operator-(Score lhs, const TaperedScore &rhs)
	{
		return {rhs.midgame - lhs, rhs.endgame - lhs};
	}

	inline TaperedScore operator*(Score lhs, const TaperedScore &rhs)
	{
		return {rhs.midgame * lhs, rhs.endgame * lhs};
	}

	constexpr auto ScoreMax = std::numeric_limits<Score>::max();
	constexpr auto ScoreMate = ScoreMax / 2;
}
