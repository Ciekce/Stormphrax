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

#include <cstdint>
#include <array>

#include "core.h"
#include "util/static_vector.h"
#include "opts.h"

namespace polaris
{
	enum class MoveType
	{
		Standard = 0,
		Promotion,
		Castling,
		EnPassant
	};

	class Move
	{
	public:
		constexpr Move() = default;
		constexpr ~Move() = default;

		[[nodiscard]] constexpr auto src() const { return static_cast<Square>(m_move >> 10); }

		[[nodiscard]] constexpr i32 srcRank() const { return  m_move >> 13; }
		[[nodiscard]] constexpr i32 srcFile() const { return (m_move >> 10) & 0x7; }

		[[nodiscard]] constexpr auto dst() const { return static_cast<Square>((m_move >> 4) & 0x3F); }

		[[nodiscard]] constexpr i32 dstRank() const { return (m_move >> 7) & 0x7; }
		[[nodiscard]] constexpr i32 dstFile() const { return (m_move >> 4) & 0x7; }

		[[nodiscard]] constexpr auto target() const { return static_cast<BasePiece>(((m_move >> 2) & 0x3) + 1); }
		[[nodiscard]] constexpr auto targetIdx() const { return (m_move >> 2) & 0x3; }

		[[nodiscard]] constexpr auto type() const { return static_cast<MoveType>(m_move & 0x3); }

		[[nodiscard]] constexpr bool isNull() const { return /*src() == dst()*/ m_move == 0; }

		[[nodiscard]] constexpr auto data() const { return m_move; }

		[[nodiscard]] explicit constexpr operator bool() const { return !isNull(); }

		constexpr auto operator==(Move other) const { return m_move == other.m_move; }

		[[nodiscard]] static constexpr Move standard(Square src, Square dst)
		{
			return Move{static_cast<u16>(
				(static_cast<u16>(src) << 10)
				| (static_cast<u16>(dst) << 4)
				| static_cast<u16>(MoveType::Standard)
			)};
		}

		[[nodiscard]] static constexpr Move promotion(Square src, Square dst, BasePiece target)
		{
			return Move{static_cast<u16>(
				(static_cast<u16>(src) << 10)
				| (static_cast<u16>(dst) << 4)
				| ((static_cast<u16>(target) - 1) << 2)
				| static_cast<u16>(MoveType::Promotion)
			)};
		}

		[[nodiscard]] static constexpr Move castling(Square src, Square dst)
		{
			return Move{static_cast<u16>(
				(static_cast<u16>(src) << 10)
				| (static_cast<u16>(dst) << 4)
				| static_cast<u16>(MoveType::Castling)
			)};
		}

		[[nodiscard]] static constexpr Move enPassant(Square src, Square dst)
		{
			return Move{static_cast<u16>(
				(static_cast<u16>(src) << 10)
				| (static_cast<u16>(dst) << 4)
				| static_cast<u16>(MoveType::EnPassant)
			)};
		}

	private:
		explicit constexpr Move(u16 move) : m_move{move} {}

		u16 m_move{};
	};

	constexpr Move NullMove{};

	// returns the king's actual destination square for
	// castling moves, otherwise just the move's destination
	// used to avoid inflating the history of the generally
	// bad moves of putting the king in a corner when castling
	constexpr Square moveActualDst(Move move)
	{
		if (move.type() == MoveType::Castling && !g_opts.chess960)
			return toSquare(move.srcRank(), move.srcFile() < move.dstFile() ? 6 : 2);
		else return move.dst();
	}

	// assumed upper bound for number of possible moves is 218
	constexpr usize DefaultMoveListCapacity = 256;

	using MoveList = StaticVector<Move, DefaultMoveListCapacity>;
}
