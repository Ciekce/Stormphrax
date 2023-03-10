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

#include <array>

#include "types.h"
#include "core.h"
#include "util/bits.h"

namespace polaris
{
	namespace offsets
	{
		constexpr i32 Up = 8;
		constexpr i32 Down = -8;
		constexpr i32 Left = -1;
		constexpr i32 Right = 1;

		constexpr i32 UpLeft = Up + Left;
		constexpr i32 UpRight = Up + Right;
		constexpr i32 DownLeft = Down + Left;
		constexpr i32 DownRight = Down + Right;

		template <Color C>
		constexpr auto up()
		{
			if constexpr(C == Color::Black)
				return Down;
			else return Up;
		}

		template <Color C>
		constexpr auto upLeft()
		{
			if constexpr(C == Color::Black)
				return DownLeft;
			else return UpLeft;
		}

		template <Color C>
		constexpr auto upRight()
		{
			if constexpr(C == Color::Black)
				return DownRight;
			else return UpRight;
		}

		template <Color C>
		constexpr auto down()
		{
			if constexpr(C == Color::Black)
				return Up;
			else return Down;
		}

		template <Color C>
		constexpr auto downLeft()
		{
			if constexpr(C == Color::Black)
				return UpLeft;
			else return DownLeft;
		}

		template <Color C>
		constexpr auto downRight()
		{
			if constexpr(C == Color::Black)
				return UpRight;
			else return DownRight;
		}
	}

	namespace shifts
	{
		constexpr i32 Vertical = 8;
		constexpr i32 Horizontal = 1;

		// '\'
		constexpr i32 DiagonalLR = Vertical - Horizontal;
		// '/'
		constexpr i32 DiagonalRL = Vertical + Horizontal;

		constexpr i32 Diagonal12LR = Vertical + Vertical - Horizontal;
		constexpr i32 Diagonal12RL = Vertical + Vertical + Horizontal;
		constexpr i32 Diagonal21LR = Vertical - Horizontal - Horizontal;
		constexpr i32 Diagonal21RL = Vertical + Horizontal + Horizontal;
	}

	class BitboardSlot
	{
	public:
		[[nodiscard]] explicit constexpr operator bool() const { return (m_board & m_mask) != 0; }

		constexpr BitboardSlot &operator=(bool rhs)
		{
			if (rhs)
				m_board |= m_mask;
			else m_board &= ~m_mask;

			return *this;
		}

	private:
		constexpr BitboardSlot(u64 &board, i32 n)
			: m_board{board},
			  m_mask{u64{1} << n} {}

		u64 &m_board;
		u64 m_mask;

		friend class Bitboard;
	};

	class Bitboard
	{
	public:
		static constexpr auto Rank1{U64(0x00000000000000FF)};
		static constexpr auto Rank2{U64(0x000000000000FF00)};
		static constexpr auto Rank3{U64(0x0000000000FF0000)};
		static constexpr auto Rank4{U64(0x00000000FF000000)};
		static constexpr auto Rank5{U64(0x000000FF00000000)};
		static constexpr auto Rank6{U64(0x0000FF0000000000)};
		static constexpr auto Rank7{U64(0x00FF000000000000)};
		static constexpr auto Rank8{U64(0xFF00000000000000)};

		static constexpr auto FileA{U64(0x0101010101010101)};
		static constexpr auto FileB{U64(0x0202020202020202)};
		static constexpr auto FileC{U64(0x0404040404040404)};
		static constexpr auto FileD{U64(0x0808080808080808)};
		static constexpr auto FileE{U64(0x1010101010101010)};
		static constexpr auto FileF{U64(0x2020202020202020)};
		static constexpr auto FileG{U64(0x4040404040404040)};
		static constexpr auto FileH{U64(0x8080808080808080)};

		static constexpr auto  DarkSquares{U64(0xAA55AA55AA55AA55)};
		static constexpr auto LightSquares{U64(0x55AA55AA55AA55AA)};

		static constexpr auto CenterSquares{U64(0x0000001818000000)};

		static constexpr auto All{UINT16_C(0xFFFFFFFFFFFFFFFF)};

		constexpr Bitboard(u64 board = 0) : m_board{board} {}

		[[nodiscard]] constexpr operator auto() const { return m_board; }

		[[nodiscard]] constexpr Bitboard operator&(Bitboard rhs) const { return m_board & rhs; }
		[[nodiscard]] constexpr Bitboard operator|(Bitboard rhs) const { return m_board | rhs; }
		[[nodiscard]] constexpr Bitboard operator^(Bitboard rhs) const { return m_board ^ rhs; }

		constexpr Bitboard &operator&=(Bitboard rhs) { m_board &= rhs; return *this; }
		constexpr Bitboard &operator|=(Bitboard rhs) { m_board |= rhs; return *this; }
		constexpr Bitboard &operator^=(Bitboard rhs) { m_board ^= rhs; return *this; }

		[[nodiscard]] constexpr Bitboard operator&(u64 rhs) const { return m_board & rhs; }
		[[nodiscard]] constexpr Bitboard operator|(u64 rhs) const { return m_board | rhs; }
		[[nodiscard]] constexpr Bitboard operator^(u64 rhs) const { return m_board ^ rhs; }

		constexpr Bitboard &operator&=(u64 rhs) { m_board &= rhs; return *this; }
		constexpr Bitboard &operator|=(u64 rhs) { m_board |= rhs; return *this; }
		constexpr Bitboard &operator^=(u64 rhs) { m_board ^= rhs; return *this; }

		[[nodiscard]] constexpr Bitboard operator&(i32 rhs) const { return m_board & static_cast<u64>(rhs); }
		[[nodiscard]] constexpr Bitboard operator|(i32 rhs) const { return m_board | static_cast<u64>(rhs); }
		[[nodiscard]] constexpr Bitboard operator^(i32 rhs) const { return m_board ^ static_cast<u64>(rhs); }

		constexpr Bitboard &operator&=(i32 rhs) { m_board &= static_cast<u64>(rhs); return *this; }
		constexpr Bitboard &operator|=(i32 rhs) { m_board |= static_cast<u64>(rhs); return *this; }
		constexpr Bitboard &operator^=(i32 rhs) { m_board ^= static_cast<u64>(rhs); return *this; }

		[[nodiscard]] constexpr Bitboard operator~() const { return ~m_board; }

		[[nodiscard]] constexpr Bitboard operator<<(i32 rhs) const { return m_board << rhs; }
		[[nodiscard]] constexpr Bitboard operator>>(i32 rhs) const { return m_board >> rhs; }

		constexpr Bitboard &operator<<=(i32 rhs) { m_board <<= rhs; return *this; }
		constexpr Bitboard &operator>>=(i32 rhs) { m_board >>= rhs; return *this; }

		[[nodiscard]] constexpr bool operator[](Square s) const { return m_board & (U64(1) << static_cast<i32>(s)); }
		[[nodiscard]] constexpr BitboardSlot operator[](Square s) { return {m_board, static_cast<i32>(s)}; }

		[[nodiscard]] constexpr auto popcount() const { return util::popcnt(m_board); }

		[[nodiscard]] constexpr bool empty() const { return m_board == 0; }
		[[nodiscard]] constexpr bool multiple() const { return util::resetLsb(m_board) != 0; }

		[[nodiscard]] constexpr Square lowestSquare() const
		{
			return static_cast<Square>(util::ctz(m_board));
		}

		[[nodiscard]] constexpr Bitboard lowestBit() const
		{
			return util::lsb(m_board);
		}

		[[nodiscard]] constexpr Square popLowestSquare()
		{
			const auto square = lowestSquare();
			m_board = util::resetLsb(m_board);
			return square;
		}

		[[nodiscard]] constexpr Bitboard popLowestBit()
		{
			const auto bit = lowestBit();
			m_board = util::resetLsb(m_board);
			return bit;
		}

		constexpr void clear()
		{
			m_board = 0;
		}

		[[nodiscard]] constexpr Bitboard shiftUp() const
		{
			return m_board << shifts::Vertical;
		}

		[[nodiscard]] constexpr Bitboard shiftDown() const
		{
			return m_board >> shifts::Vertical;
		}

		[[nodiscard]] constexpr Bitboard shiftLeft() const
		{
			return (m_board >> shifts::Horizontal) & ~FileH;
		}

		[[nodiscard]] constexpr Bitboard shiftLeftUnchecked() const
		{
			return m_board >> shifts::Horizontal;
		}

		[[nodiscard]] constexpr Bitboard shiftRight() const
		{
			return (m_board << shifts::Horizontal) & ~FileA;
		}

		[[nodiscard]] constexpr Bitboard shiftRightUnchecked() const
		{
			return m_board << shifts::Horizontal;
		}

		[[nodiscard]] constexpr Bitboard shiftUpLeft() const
		{
			return (m_board << shifts::DiagonalLR) & ~FileH;
		}

		[[nodiscard]] constexpr Bitboard shiftUpRight() const
		{
			return (m_board << shifts::DiagonalRL) & ~FileA;
		}

		[[nodiscard]] constexpr Bitboard shiftDownLeft() const
		{
			return (m_board >> shifts::DiagonalRL) & ~FileH;
		}

		[[nodiscard]] constexpr Bitboard shiftDownRight() const
		{
			return (m_board >> shifts::DiagonalLR) & ~FileA;
		}

		[[nodiscard]] constexpr Bitboard shiftUpUpLeft() const
		{
			return (m_board << shifts::Diagonal12LR) & ~FileH;
		}

		[[nodiscard]] constexpr Bitboard shiftUpUpRight() const
		{
			return (m_board << shifts::Diagonal12RL) & ~FileA;
		}

		[[nodiscard]] constexpr Bitboard shiftUpLeftLeft() const
		{
			return (m_board << shifts::Diagonal21LR) & ~(FileG | FileH);
		}

		[[nodiscard]] constexpr Bitboard shiftUpRightRight() const
		{
			return (m_board << shifts::Diagonal21RL) & ~(FileA | FileB);
		}

		[[nodiscard]] constexpr Bitboard shiftDownLeftLeft() const
		{
			return (m_board >> shifts::Diagonal21RL) & ~(FileG | FileH);
		}

		[[nodiscard]] constexpr Bitboard shiftDownRightRight() const
		{
			return (m_board >> shifts::Diagonal21LR) & ~(FileA | FileB);
		}

		[[nodiscard]] constexpr Bitboard shiftDownDownLeft() const
		{
			return (m_board >> shifts::Diagonal12RL) & ~FileH;
		}

		[[nodiscard]] constexpr Bitboard shiftDownDownRight() const
		{
			return (m_board >> shifts::Diagonal12LR) & ~FileA;
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard shiftUpRelative() const
		{
			if constexpr(C == Color::Black)
				return shiftDown();
			else return shiftUp();
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard shiftUpLeftRelative() const
		{
			if constexpr(C == Color::Black)
				return shiftDownLeft();
			else return shiftUpLeft();
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard shiftUpRightRelative() const
		{
			if constexpr(C == Color::Black)
				return shiftDownRight();
			else return shiftUpRight();
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard shiftDownRelative() const
		{
			if constexpr(C == Color::Black)
				return shiftUp();
			else return shiftDown();
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard shiftDownLeftRelative() const
		{
			if constexpr(C == Color::Black)
				return shiftUpLeft();
			else return shiftDownLeft();
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard shiftDownRightRelative() const
		{
			if constexpr(C == Color::Black)
				return shiftUpRight();
			else return shiftDownRight();
		}

		[[nodiscard]] constexpr Bitboard fillUp() const
		{
			auto b = m_board;
			b |= b << 8;
			b |= b << 16;
			b |= b << 32;
			return b;
		}

		[[nodiscard]] constexpr Bitboard fillDown() const
		{
			auto b = m_board;
			b |= b >> 8;
			b |= b >> 16;
			b |= b >> 32;
			return b;
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard fillUpRelative() const
		{
			if constexpr(C == Color::Black)
				return fillDown();
			else return fillUp();
		}

		template <Color C>
		[[nodiscard]] constexpr Bitboard fillDownRelative() const
		{
			if constexpr(C == Color::Black)
				return fillUp();
			else return fillDown();
		}

		[[nodiscard]] constexpr Bitboard fillFile() const
		{
			return fillUp() | fillDown();
		}

		[[nodiscard]] constexpr bool operator==(const Bitboard &other) const
		{
			return m_board == other.m_board;
		}

		[[nodiscard]] constexpr bool operator==(u64 other) const
		{
			return m_board == other;
		}

		[[nodiscard]] constexpr static Bitboard fromSquare(Square square)
		{
			return squareBit(square);
		}

		[[nodiscard]] constexpr static Bitboard fromSquareChecked(Square square)
		{
			return squareBitChecked(square);
		}

	private:
		u64 m_board;
	};

	namespace boards
	{
		constexpr Bitboard Rank1{Bitboard::Rank1};
		constexpr Bitboard Rank2{Bitboard::Rank2};
		constexpr Bitboard Rank3{Bitboard::Rank3};
		constexpr Bitboard Rank4{Bitboard::Rank4};
		constexpr Bitboard Rank5{Bitboard::Rank5};
		constexpr Bitboard Rank6{Bitboard::Rank6};
		constexpr Bitboard Rank7{Bitboard::Rank7};
		constexpr Bitboard Rank8{Bitboard::Rank8};

		constexpr Bitboard FileA{Bitboard::FileA};
		constexpr Bitboard FileB{Bitboard::FileB};
		constexpr Bitboard FileC{Bitboard::FileC};
		constexpr Bitboard FileD{Bitboard::FileD};
		constexpr Bitboard FileE{Bitboard::FileE};
		constexpr Bitboard FileF{Bitboard::FileF};
		constexpr Bitboard FileG{Bitboard::FileG};
		constexpr Bitboard FileH{Bitboard::FileH};

		constexpr std::array Ranks{Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8};
		constexpr std::array Files{FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH};

		constexpr Bitboard  DarkSquares{Bitboard:: DarkSquares};
		constexpr Bitboard LightSquares{Bitboard::LightSquares};

		constexpr Bitboard CenterSquares{Bitboard::CenterSquares};

		constexpr Bitboard All{Bitboard::All};

		template <Color C>
		constexpr auto promotionRank()
		{
			if constexpr(C == Color::Black)
				return Rank1;
			else return Rank8;
		}

		constexpr auto promotionRank(Color c)
		{
			return c == Color::Black ? Rank1 : Rank8;
		}

		template <Color C>
		constexpr auto rank(i32 idx)
		{
			return Ranks[relativeRank<C>(idx)];
		}
	}
}
