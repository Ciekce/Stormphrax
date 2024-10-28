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
		namespace color
		{
			enum : u8
			{
				Black = 0,
				White,
				None,
			};
		}

		namespace piece_type
		{
			enum : u8
			{
				Pawn = 0,
				Knight,
				Bishop,
				Rook,
				Queen,
				King,
				None,
			};
		}

		namespace piece
		{
			enum : u8
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
				None,
			};
		}

		namespace square
		{
			enum : u8
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
		}
	}

	class Color
	{
	public:
		constexpr Color()
			: m_id{0} {}

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
			assert(id <= internal::core::color::None);
			return Color{id};
		}

		constexpr auto operator==(const Color &) const -> bool = default;

		constexpr auto operator=(const Color &) -> Color & = default;
		constexpr auto operator=(Color &&) -> Color & = default;

	private:
		explicit constexpr Color(u8 id)
			: m_id{id} {}

		u8 m_id{};
	};

	namespace colors
	{
		constexpr auto Black = Color::fromRaw(internal::core::color::Black);
		constexpr auto White = Color::fromRaw(internal::core::color::White);
		constexpr auto None = Color::fromRaw(internal::core::color::None);
	}

	class Piece;

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

		[[nodiscard]] constexpr auto isMajor() const
		{
			assert(m_id != internal::core::piece_type::None);
			return m_id == internal::core::piece_type::Rook
				|| m_id == internal::core::piece_type::Queen;
		}

		[[nodiscard]] constexpr auto isMinor() const
		{
			assert(m_id != internal::core::piece_type::None);
			return m_id == internal::core::piece_type::Knight
				|| m_id == internal::core::piece_type::Bishop;
		}

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

		[[nodiscard]] constexpr auto type() const -> PieceType
		{
			return PieceType::fromRaw(m_id >> 1);
		}

		[[nodiscard]] constexpr auto typeOrNone() const -> PieceType
		{
			if (m_id == internal::core::piece::None)
				return piece_types::None;
			return PieceType::fromRaw(m_id >> 1);
		}

		[[nodiscard]] constexpr auto color() const -> Color
		{
			return Color::fromRaw(m_id & 1);
		}

		[[nodiscard]] constexpr auto colorOrNone() const -> Color
		{
			if (m_id == internal::core::piece::None)
				return colors::None;
			return Color::fromRaw(m_id & 1);
		}

		[[nodiscard]] constexpr auto flipColor() const
		{
			assert(m_id != internal::core::piece::None);
			return Piece{static_cast<u8>(m_id ^ 1)};
		}

		[[nodiscard]] constexpr auto copyColorTo(PieceType target) const -> Piece
		{
			assert(m_id != internal::core::piece::None);
			return target.withColor(color());
		}

		[[nodiscard]] constexpr auto isMajor() const
		{
			assert(m_id != internal::core::piece::None);
			return type().isMajor();
		}

		[[nodiscard]] constexpr auto isMinor() const
		{
			assert(m_id != internal::core::piece::None);
			return type().isMinor();
		}

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

	class Square
	{
	public:
		constexpr Square()
			: m_id{0} {}

		constexpr Square(const Square &) = default;
		constexpr Square(Square &&) = default;

		[[nodiscard]] constexpr auto raw() const
		{
			return m_id;
		}

		[[nodiscard]] constexpr auto idx() const
		{
			return static_cast<usize>(m_id);
		}

		[[nodiscard]] constexpr auto rank() const
		{
			assert(m_id != internal::core::square::None);
			return static_cast<u32>(m_id) >> 3;
		}

		[[nodiscard]] constexpr auto file() const
		{
			assert(m_id != internal::core::square::None);
			return static_cast<u32>(m_id) & 0x7;
		}

		[[nodiscard]] constexpr auto flipRank() const
		{
			assert(m_id != internal::core::square::None);
			return fromRaw(m_id ^ 0b111000);
		}

		[[nodiscard]] constexpr auto flipFile() const
		{
			assert(m_id != internal::core::square::None);
			return fromRaw(m_id ^ 0b000111);
		}

		[[nodiscard]] constexpr auto withRank(u32 rank) const
		{
			assert(m_id != internal::core::square::None);
			return fromRankFile(rank, file());
		}

		[[nodiscard]] constexpr auto withFile(u32 file) const
		{
			assert(m_id != internal::core::square::None);
			return fromRankFile(rank(), file);
		}

		[[nodiscard]] constexpr auto bit() const
		{
			assert(m_id != internal::core::square::None);
			return u64{1} << m_id;
		}

		[[nodiscard]] constexpr auto bitOrZero() const
		{
			if (m_id == internal::core::square::None)
				return u64{0};
			return bit();
		}

		[[nodiscard]] static constexpr auto fromRaw(u8 id) -> Square
		{
			return Square{id};
		}

		[[nodiscard]] static constexpr auto fromRankFile(u32 rank, u32 file) -> Square
		{
			assert(rank < 8);
			assert(file < 8);

			return fromRaw((rank << 3) | file);
		}

		constexpr auto operator==(const Square &) const -> bool = default;

		constexpr auto operator=(const Square &) -> Square & = default;
		constexpr auto operator=(Square &&) -> Square & = default;

	private:
		explicit constexpr Square(u8 id)
			: m_id{id} {}

		u8 m_id{};
	};

	namespace squares
	{
		constexpr auto A1 = Square::fromRaw(internal::core::square::A1);
		constexpr auto B1 = Square::fromRaw(internal::core::square::B1);
		constexpr auto C1 = Square::fromRaw(internal::core::square::C1);
		constexpr auto D1 = Square::fromRaw(internal::core::square::D1);
		constexpr auto E1 = Square::fromRaw(internal::core::square::E1);
		constexpr auto F1 = Square::fromRaw(internal::core::square::F1);
		constexpr auto G1 = Square::fromRaw(internal::core::square::G1);
		constexpr auto H1 = Square::fromRaw(internal::core::square::H1);
		constexpr auto A2 = Square::fromRaw(internal::core::square::A2);
		constexpr auto B2 = Square::fromRaw(internal::core::square::B2);
		constexpr auto C2 = Square::fromRaw(internal::core::square::C2);
		constexpr auto D2 = Square::fromRaw(internal::core::square::D2);
		constexpr auto E2 = Square::fromRaw(internal::core::square::E2);
		constexpr auto F2 = Square::fromRaw(internal::core::square::F2);
		constexpr auto G2 = Square::fromRaw(internal::core::square::G2);
		constexpr auto H2 = Square::fromRaw(internal::core::square::H2);
		constexpr auto A3 = Square::fromRaw(internal::core::square::A3);
		constexpr auto B3 = Square::fromRaw(internal::core::square::B3);
		constexpr auto C3 = Square::fromRaw(internal::core::square::C3);
		constexpr auto D3 = Square::fromRaw(internal::core::square::D3);
		constexpr auto E3 = Square::fromRaw(internal::core::square::E3);
		constexpr auto F3 = Square::fromRaw(internal::core::square::F3);
		constexpr auto G3 = Square::fromRaw(internal::core::square::G3);
		constexpr auto H3 = Square::fromRaw(internal::core::square::H3);
		constexpr auto A4 = Square::fromRaw(internal::core::square::A4);
		constexpr auto B4 = Square::fromRaw(internal::core::square::B4);
		constexpr auto C4 = Square::fromRaw(internal::core::square::C4);
		constexpr auto D4 = Square::fromRaw(internal::core::square::D4);
		constexpr auto E4 = Square::fromRaw(internal::core::square::E4);
		constexpr auto F4 = Square::fromRaw(internal::core::square::F4);
		constexpr auto G4 = Square::fromRaw(internal::core::square::G4);
		constexpr auto H4 = Square::fromRaw(internal::core::square::H4);
		constexpr auto A5 = Square::fromRaw(internal::core::square::A5);
		constexpr auto B5 = Square::fromRaw(internal::core::square::B5);
		constexpr auto C5 = Square::fromRaw(internal::core::square::C5);
		constexpr auto D5 = Square::fromRaw(internal::core::square::D5);
		constexpr auto E5 = Square::fromRaw(internal::core::square::E5);
		constexpr auto F5 = Square::fromRaw(internal::core::square::F5);
		constexpr auto G5 = Square::fromRaw(internal::core::square::G5);
		constexpr auto H5 = Square::fromRaw(internal::core::square::H5);
		constexpr auto A6 = Square::fromRaw(internal::core::square::A6);
		constexpr auto B6 = Square::fromRaw(internal::core::square::B6);
		constexpr auto C6 = Square::fromRaw(internal::core::square::C6);
		constexpr auto D6 = Square::fromRaw(internal::core::square::D6);
		constexpr auto E6 = Square::fromRaw(internal::core::square::E6);
		constexpr auto F6 = Square::fromRaw(internal::core::square::F6);
		constexpr auto G6 = Square::fromRaw(internal::core::square::G6);
		constexpr auto H6 = Square::fromRaw(internal::core::square::H6);
		constexpr auto A7 = Square::fromRaw(internal::core::square::A7);
		constexpr auto B7 = Square::fromRaw(internal::core::square::B7);
		constexpr auto C7 = Square::fromRaw(internal::core::square::C7);
		constexpr auto D7 = Square::fromRaw(internal::core::square::D7);
		constexpr auto E7 = Square::fromRaw(internal::core::square::E7);
		constexpr auto F7 = Square::fromRaw(internal::core::square::F7);
		constexpr auto G7 = Square::fromRaw(internal::core::square::G7);
		constexpr auto H7 = Square::fromRaw(internal::core::square::H7);
		constexpr auto A8 = Square::fromRaw(internal::core::square::A8);
		constexpr auto B8 = Square::fromRaw(internal::core::square::B8);
		constexpr auto C8 = Square::fromRaw(internal::core::square::C8);
		constexpr auto D8 = Square::fromRaw(internal::core::square::D8);
		constexpr auto E8 = Square::fromRaw(internal::core::square::E8);
		constexpr auto F8 = Square::fromRaw(internal::core::square::F8);
		constexpr auto G8 = Square::fromRaw(internal::core::square::G8);
		constexpr auto H8 = Square::fromRaw(internal::core::square::H8);
		constexpr auto None = Square::fromRaw(internal::core::square::None);
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
			return black() != squares::None
				&& white() != squares::None
				&& black() != white();
		}
	};

	struct CastlingRooks
	{
		struct RookPair
		{
			Square kingside{squares::None};
			Square queenside{squares::None};

			inline auto clear()
			{
				kingside = squares::None;
				queenside = squares::None;
			}

			inline auto unset(Square square)
			{
				assert(square != squares::None);

				if (square == kingside)
					kingside = squares::None;
				else if (square == queenside)
					queenside = squares::None;
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
