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

	enum class PieceType
	{
		Pawn = 0,
		Knight,
		Bishop,
		Rook,
		Queen,
		King,
		None
	};

	[[nodiscard]] constexpr auto colorPiece(PieceType piece, Color color)
	{
		assert(piece != PieceType::None);
		assert(color != colors::None);

		return static_cast<Piece>((static_cast<i32>(piece) << 1) + color.idx());
	}

	[[nodiscard]] constexpr auto pieceType(Piece piece)
	{
		assert(piece != Piece::None);
		return static_cast<PieceType>(static_cast<i32>(piece) >> 1);
	}

	[[nodiscard]] constexpr auto pieceTypeOrNone(Piece piece)
	{
		if (piece == Piece::None)
			return PieceType::None;
		return static_cast<PieceType>(static_cast<i32>(piece) >> 1);
	}

	[[nodiscard]] constexpr auto pieceColor(Piece piece)
	{
		assert(piece != Piece::None);
		return Color::fromRaw(static_cast<i32>(piece) & 1);
	}

	[[nodiscard]] constexpr auto flipPieceColor(Piece piece)
	{
		assert(piece != Piece::None);
		return static_cast<Piece>(static_cast<i32>(piece) ^ 0x1);
	}

	[[nodiscard]] constexpr auto copyPieceColor(Piece piece, PieceType target)
	{
		assert(piece != Piece::None);
		assert(target != PieceType::None);

		return colorPiece(target, pieceColor(piece));
	}

	[[nodiscard]] constexpr auto isMajor(PieceType piece)
	{
		assert(piece != PieceType::None);
		return piece == PieceType::Rook || piece == PieceType::Queen;
	}

	[[nodiscard]] constexpr auto isMajor(Piece piece)
	{
		assert(piece != Piece::None);
		return isMajor(pieceType(piece));
	}

	[[nodiscard]] constexpr auto isMinor(PieceType piece)
	{
		assert(piece != PieceType::None);
		return piece == PieceType::Knight || piece == PieceType::Bishop;
	}

	[[nodiscard]] constexpr auto isMinor(Piece piece)
	{
		assert(piece != Piece::None);
		return isMinor(pieceType(piece));
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

	[[nodiscard]] constexpr auto pieceTypeFromChar(char c)
	{
		switch (c)
		{
		case 'p': return PieceType::  Pawn;
		case 'n': return PieceType::Knight;
		case 'b': return PieceType::Bishop;
		case 'r': return PieceType::  Rook;
		case 'q': return PieceType:: Queen;
		case 'k': return PieceType::  King;
		default : return PieceType::  None;
		}
	}

	[[nodiscard]] constexpr auto pieceTypeToChar(PieceType piece)
	{
		switch (piece)
		{
		case PieceType::  None: return ' ';
		case PieceType::  Pawn: return 'p';
		case PieceType::Knight: return 'n';
		case PieceType::Bishop: return 'b';
		case PieceType::  Rook: return 'r';
		case PieceType:: Queen: return 'q';
		case PieceType::  King: return 'k';
		default: return ' ';
		}
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
