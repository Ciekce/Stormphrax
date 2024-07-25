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

#include "types.h"

#include <cmath>
#include <utility>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <array>

#include "util/bitfield.h"
#include "util/cemath.h"

namespace stormphrax
{
	namespace internal::core
	{
		namespace piece
		{
			constexpr u8 BlackPawn = 0;
			constexpr u8 WhitePawn = 1;
			constexpr u8 BlackKnight = 2;
			constexpr u8 WhiteKnight = 3;
			constexpr u8 BlackBishop = 4;
			constexpr u8 WhiteBishop = 5;
			constexpr u8 BlackRook = 6;
			constexpr u8 WhiteRook = 7;
			constexpr u8 BlackQueen = 8;
			constexpr u8 WhiteQueen = 9;
			constexpr u8 BlackKing = 10;
			constexpr u8 WhiteKing = 11;
			constexpr u8 None = 12;
		}

		namespace piece_type
		{
			constexpr u8 Pawn = 0;
			constexpr u8 Knight = 1;
			constexpr u8 Bishop = 2;
			constexpr u8 Rook = 3;
			constexpr u8 Queen = 4;
			constexpr u8 King = 5;
			constexpr u8 None = 6;
		}

		namespace color
		{
			constexpr u8 Black = 0;
			constexpr u8 White = 1;
			constexpr u8 None = 2;
		}
	}

	class PieceType;
	class Color;

	class Piece
	{
	public:
		constexpr Piece()
			: m_id{0} {}

		constexpr Piece(const Piece &) = default;
		constexpr Piece(Piece &&) = default;

		[[nodiscard]] constexpr auto raw() const
		{
			return m_id;
		}

		[[nodiscard]] constexpr auto idx() const
		{
			return static_cast<usize>(m_id);
		}

		[[nodiscard]] constexpr auto type() const -> PieceType;
		[[nodiscard]] constexpr auto typeOrNone() const -> PieceType;

		[[nodiscard]] constexpr auto color() const -> Color;
		[[nodiscard]] constexpr auto colorOrNone() const -> Color;

		[[nodiscard]] constexpr auto flipColor() const
		{
			assert(m_id != internal::core::piece::None);
			return Piece{static_cast<u8>(m_id ^ 1)};
		}

		[[nodiscard]] constexpr auto copyColor(PieceType target) const -> Piece;

		[[nodiscard]] constexpr auto toChar() const
		{
			constexpr auto Map = std::array {
				'p', 'P', 'n', 'N', 'b', 'B', 'r', 'R', 'q', 'Q', 'k', 'K', ' '
			};

			return Map[idx()];
		}

		[[nodiscard]] static constexpr auto fromRaw(u8 id)
		{
			return Piece{id};
		}

		[[nodiscard]] static constexpr auto fromChar(char c) -> Piece;

		constexpr auto operator==(const Piece &) const -> bool = default;

		constexpr auto operator=(const Piece &) -> Piece & = default;
		constexpr auto operator=(Piece &&) -> Piece & = default;

	private:
		explicit constexpr Piece(u8 id)
			: m_id{id} {}

		u8 m_id{};
	};

	class PieceType
	{
	public:
		constexpr PieceType()
			: m_id{0} {}

		constexpr PieceType(const PieceType &) = default;
		constexpr PieceType(PieceType &&) = default;

		[[nodiscard]] constexpr auto raw() const
		{
			return m_id;
		}

		[[nodiscard]] constexpr auto idx() const
		{
			return static_cast<usize>(m_id);
		}

		[[nodiscard]] constexpr auto withColor(Color c) const -> Piece;

		[[nodiscard]] constexpr auto toChar() const
		{
			constexpr auto Map = std::array {
				'p', 'n', 'b', 'r', 'q', 'k', ' '
			};

			return Map[idx()];
		}

		[[nodiscard]] static constexpr auto fromRaw(u8 id)
		{
			return PieceType{id};
		}

		[[nodiscard]] static constexpr auto fromChar(char c) -> PieceType;

		constexpr auto operator==(const PieceType &) const -> bool = default;

		constexpr auto operator=(const PieceType &) -> PieceType & = default;
		constexpr auto operator=(PieceType &&) -> PieceType & = default;

	private:
		explicit constexpr PieceType(u8 id)
			: m_id{id} {}

		u8 m_id{};
	};

	class Color
	{
	public:
		constexpr Color()
			: m_id{0} {}

		explicit constexpr Color(u8 id)
			: m_id{id} {}

		constexpr Color(const Color &) = default;
		constexpr Color(Color &&) = default;

		[[nodiscard]] constexpr auto raw() const
		{
			return m_id;
		}

		[[nodiscard]] constexpr auto idx() const
		{
			return static_cast<usize>(m_id);
		}

		[[nodiscard]] constexpr auto opponent() const
		{
			assert(m_id != internal::core::color::None);
			return Color{static_cast<u8>(m_id ^ 1)};
		}

		[[nodiscard]] static constexpr auto fromRaw(u8 id)
		{
			return Color{id};
		}

		constexpr auto operator==(const Color &) const -> bool = default;

		constexpr auto operator=(const Color &) -> Color & = default;
		constexpr auto operator=(Color &&) -> Color & = default;

	private:
		u8 m_id{};
	};

	namespace pieces
	{
		constexpr auto BlackPawn = Piece::fromRaw(internal::core::piece::BlackPawn);
		constexpr auto WhitePawn = Piece::fromRaw(internal::core::piece::WhitePawn);
		constexpr auto BlackKnight = Piece::fromRaw(internal::core::piece::BlackKnight);
		constexpr auto WhiteKnight = Piece::fromRaw(internal::core::piece::WhiteKnight);
		constexpr auto BlackBishop = Piece::fromRaw(internal::core::piece::BlackBishop);
		constexpr auto WhiteBishop = Piece::fromRaw(internal::core::piece::WhiteBishop);
		constexpr auto BlackRook = Piece::fromRaw(internal::core::piece::BlackRook);
		constexpr auto WhiteRook = Piece::fromRaw(internal::core::piece::WhiteRook);
		constexpr auto BlackQueen = Piece::fromRaw(internal::core::piece::BlackQueen);
		constexpr auto WhiteQueen = Piece::fromRaw(internal::core::piece::WhiteQueen);
		constexpr auto BlackKing = Piece::fromRaw(internal::core::piece::BlackKing);
		constexpr auto WhiteKing = Piece::fromRaw(internal::core::piece::WhiteKing);
		constexpr auto None = Piece::fromRaw(internal::core::piece::None);
	}

	namespace piece_types
	{
		constexpr auto Pawn = PieceType::fromRaw(internal::core::piece_type::Pawn);
		constexpr auto Knight = PieceType::fromRaw(internal::core::piece_type::Knight);
		constexpr auto Bishop = PieceType::fromRaw(internal::core::piece_type::Bishop);
		constexpr auto Rook = PieceType::fromRaw(internal::core::piece_type::Rook);
		constexpr auto Queen = PieceType::fromRaw(internal::core::piece_type::Queen);
		constexpr auto King = PieceType::fromRaw(internal::core::piece_type::King);
		constexpr auto None = PieceType::fromRaw(internal::core::piece_type::None);
	}

	namespace colors
	{
		constexpr auto Black = Color::fromRaw(internal::core::color::Black);
		constexpr auto White = Color::fromRaw(internal::core::color::White);
		constexpr auto None = Color::fromRaw(internal::core::color::None);
	}

	constexpr auto Piece::type() const -> PieceType
	{
		return PieceType::fromRaw(m_id >> 1);
	}

	constexpr auto Piece::typeOrNone() const -> PieceType
	{
		if (*this == pieces::None)
			return piece_types::None;
		return PieceType::fromRaw(m_id >> 1);
	}

	constexpr auto Piece::color() const -> Color
	{
		return Color{static_cast<u8>(m_id & 1)};
	}

	constexpr auto Piece::colorOrNone() const -> Color
	{
		if (*this == pieces::None)
			return colors::None;
		return Color{static_cast<u8>(m_id & 1)};
	}

	constexpr auto Piece::copyColor(PieceType target) const -> Piece
	{
		assert(*this != piece_types::None);
		return target.withColor(color());
	}

	constexpr auto Piece::fromChar(char c) -> Piece
	{
		switch (c)
		{
			case 'p': return pieces::  BlackPawn;
			case 'P': return pieces::  WhitePawn;
			case 'n': return pieces::BlackKnight;
			case 'N': return pieces::WhiteKnight;
			case 'b': return pieces::BlackBishop;
			case 'B': return pieces::WhiteBishop;
			case 'r': return pieces::  BlackRook;
			case 'R': return pieces::  WhiteRook;
			case 'q': return pieces:: BlackQueen;
			case 'Q': return pieces:: WhiteQueen;
			case 'k': return pieces::  BlackKing;
			case 'K': return pieces::  WhiteKing;
			default : return pieces::       None;
		}
	}

	constexpr auto PieceType::withColor(Color c) const -> Piece
	{
		assert(*this != piece_types::None);
		assert(c != colors::None);

		return Piece::fromRaw((m_id << 1) | c.raw());
	}

	constexpr auto PieceType::fromChar(char c) -> PieceType
	{
		switch (c)
		{
			case 'p': return piece_types::  Pawn;
			case 'n': return piece_types::Knight;
			case 'b': return piece_types::Bishop;
			case 'r': return piece_types::  Rook;
			case 'q': return piece_types:: Queen;
			case 'k': return piece_types::  King;
			default : return piece_types::  None;
		}
	}

	// =========================================

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
		assert(rank < 8);
		assert(file < 8);

		return static_cast<Square>((rank << 3) | file);
	}

	[[nodiscard]] constexpr auto squareRank(Square square)
	{
		assert(square != Square::None);
		return static_cast<i32>(square) >> 3;
	}

	[[nodiscard]] constexpr auto squareFile(Square square)
	{
		assert(square != Square::None);
		return static_cast<i32>(square) & 0x7;
	}

	[[nodiscard]] constexpr auto flipSquareRank(Square square)
	{
		assert(square != Square::None);
		return static_cast<Square>(static_cast<i32>(square) ^ 0b111000);
	}

	[[nodiscard]] constexpr auto flipSquareFile(Square square)
	{
		assert(square != Square::None);
		return static_cast<Square>(static_cast<i32>(square) ^ 0b000111);
	}

	[[nodiscard]] constexpr auto squareBit(Square square)
	{
		assert(square != Square::None);
		return U64(1) << static_cast<i32>(square);
	}

	[[nodiscard]] constexpr auto squareBitChecked(Square square)
	{
		if (square == Square::None)
			return U64(0);

		return U64(1) << static_cast<i32>(square);
	}

	constexpr auto relativeRank(Color c, i32 rank)
	{
		assert(rank >= 0 && rank < 8);
		return c == colors::Black ? 7 - rank : rank;
	}

	struct KingPair
	{
		std::array<Square, 2> kings{};

		[[nodiscard]] inline auto black() const
		{
			return kings[0];
		}

		[[nodiscard]] inline auto white() const
		{
			return kings[1];
		}

		[[nodiscard]] inline auto black() -> auto &
		{
			return kings[0];
		}

		[[nodiscard]] inline auto white() -> auto &
		{
			return kings[1];
		}

		[[nodiscard]] inline auto color(Color c) const
		{
			assert(c != colors::None);
			return kings[c.idx()];
		}

		[[nodiscard]] inline auto color(Color c) -> auto &
		{
			assert(c != colors::None);
			return kings[c.idx()];
		}

		[[nodiscard]] inline auto operator==(const KingPair &other) const -> bool = default;

		[[nodiscard]] inline auto isValid()
		{
			return black() != Square::None
				&& white() != Square::None
				&& black() != white();
		}
	};

	struct CastlingRooks
	{
		struct RookPair
		{
			Square kingside{Square::None};
			Square queenside{Square::None};

			inline auto clear()
			{
				kingside = Square::None;
				queenside = Square::None;
			}

			inline auto unset(Square square)
			{
				assert(square != Square::None);

				if (square == kingside)
					kingside = Square::None;
				else if (square == queenside)
					queenside = Square::None;
			}

			[[nodiscard]] inline auto operator==(const RookPair &) const -> bool = default;
		};

		std::array<RookPair, 2> rooks;

		[[nodiscard]] inline auto black() const -> const auto &
		{
			return rooks[0];
		}

		[[nodiscard]] inline auto white() const -> const auto &
		{
			return rooks[1];
		}

		[[nodiscard]] inline auto black() -> auto &
		{
			return rooks[0];
		}

		[[nodiscard]] inline auto white() -> auto &
		{
			return rooks[1];
		}

		[[nodiscard]] inline auto color(Color c) const -> const auto &
		{
			assert(c != colors::None);
			return rooks[c.idx()];
		}

		[[nodiscard]] inline auto color(Color c) -> auto &
		{
			assert(c != colors::None);
			return rooks[c.idx()];
		}

		[[nodiscard]] inline auto operator==(const CastlingRooks &) const -> bool = default;
	};

	using Score = i32;

	constexpr auto ScoreInf = 32767;
	constexpr auto ScoreMate = 32766;
	constexpr auto ScoreTbWin = 30000;
	constexpr auto ScoreWin = 25000;

	constexpr auto ScoreNone = -ScoreInf;

	constexpr i32 MaxDepth = 255;

	constexpr auto ScoreMaxMate = ScoreMate - MaxDepth;
}
