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

#ifdef NDEBUG
		[[nodiscard]] constexpr auto src() const { return static_cast<Square>(m_move >> 10); }

		[[nodiscard]] constexpr i32 srcRank() const { return  m_move >> 13; }
		[[nodiscard]] constexpr i32 srcFile() const { return (m_move >> 10) & 0x7; }

		[[nodiscard]] constexpr auto dst() const { return static_cast<Square>((m_move >> 4) & 0x3F); }

		[[nodiscard]] constexpr i32 dstRank() const { return (m_move >> 7) & 0x7; }
		[[nodiscard]] constexpr i32 dstFile() const { return (m_move >> 4) & 0x7; }

		[[nodiscard]] constexpr auto target() const { return static_cast<BasePiece>(((m_move >> 2) & 0x3) + 1); }

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
#else
	public:
		[[nodiscard]] constexpr auto src() const { return m_src; }

		[[nodiscard]] constexpr i32 srcRank() const { return squareRank(m_src); }
		[[nodiscard]] constexpr i32 srcFile() const { return squareFile(m_src); }

		[[nodiscard]] constexpr auto dst() const { return m_dst; }

		[[nodiscard]] constexpr i32 dstRank() const { return squareRank(m_dst); }
		[[nodiscard]] constexpr i32 dstFile() const { return squareFile(m_dst); }

		[[nodiscard]] constexpr auto target() const { return m_target; }

		[[nodiscard]] constexpr auto type() const { return m_type; }

		[[nodiscard]] constexpr bool isNull() const { return src() == dst(); }

		[[nodiscard]] explicit constexpr operator bool() const { return !isNull(); }

		constexpr auto operator==(Move other) const
		{
			return m_src == other.m_src
				&& m_dst == other.m_dst
				&& m_target == other.m_target
				&& m_type == other.m_type;
		}

		[[nodiscard]] static constexpr Move standard(Square src, Square dst)
		{
			return Move{src, dst, BasePiece::None, MoveType::Standard};
		}

		[[nodiscard]] static constexpr Move promotion(Square src, Square dst, BasePiece target)
		{
			return Move{src, dst, target, MoveType::Promotion};
		}

		[[nodiscard]] static constexpr Move castling(Square src, Square dst)
		{
			return Move{src, dst, BasePiece::None, MoveType::Castling};
		}

		[[nodiscard]] static constexpr Move enPassant(Square src, Square dst)
		{
			return Move{src, dst, BasePiece::None, MoveType::EnPassant};
		}

		[[nodiscard]] static constexpr Move null() { return Move{}; }

	private:
		constexpr Move(Square src, Square dst, BasePiece target, MoveType type)
			: m_src{src}, m_dst{dst}, m_target{target}, m_type{type} {}

		Square m_src{Square::None};
		Square m_dst{Square::None};
		BasePiece m_target{BasePiece::None};
		MoveType m_type{MoveType::Standard};
#endif
	};

	constexpr Move NullMove{};

	// assumed upper bound for number of possible moves is 218
	constexpr size_t DefaultMoveListCapacity = 256;

	using MoveList = StaticVector<Move, DefaultMoveListCapacity>;
}
