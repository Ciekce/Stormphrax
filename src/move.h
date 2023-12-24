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

#include "types.h"

#include <cstdint>
#include <array>

#include "core.h"
#include "util/static_vector.h"
#include "opts.h"
#include "position/boards.h"

namespace stormphrax
{
	enum class MoveType : u8
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

		[[nodiscard]] constexpr auto srcIdx() const { return m_move >> 10; }
		[[nodiscard]] constexpr auto src() const { return static_cast<Square>(srcIdx()); }

		[[nodiscard]] constexpr auto srcRank() const { return  m_move >> 13; }
		[[nodiscard]] constexpr auto srcFile() const { return (m_move >> 10) & 0x7; }

		[[nodiscard]] constexpr auto dstIdx() const { return (m_move >> 4) & 0x3F; }
		[[nodiscard]] constexpr auto dst() const { return static_cast<Square>(dstIdx()); }

		[[nodiscard]] constexpr auto dstRank() const { return (m_move >> 7) & 0x7; }
		[[nodiscard]] constexpr auto dstFile() const { return (m_move >> 4) & 0x7; }

		[[nodiscard]] constexpr auto targetIdx() const { return (m_move >> 2) & 0x3; }
		[[nodiscard]] constexpr auto target() const { return static_cast<PieceType>(targetIdx() + 1); }

		[[nodiscard]] constexpr auto type() const { return static_cast<MoveType>(m_move & 0x3); }

		[[nodiscard]] constexpr auto isNull() const { return /*src() == dst()*/ m_move == 0; }

		[[nodiscard]] constexpr auto data() const { return m_move; }

		// returns the king's actual destination square for
		// castling moves, otherwise just the move's destination
		// used to avoid inflating the history of the generally
		// bad moves of putting the king in a corner when castling
		[[nodiscard]] constexpr auto historyDst() const
		{
			if (type() == MoveType::Castling && !g_opts.chess960)
				return toSquare(srcRank(), srcFile() < dstFile() ? 6 : 2);
			else return dst();
		}

		[[nodiscard]] explicit constexpr operator bool() const { return !isNull(); }

		constexpr auto operator==(Move other) const { return m_move == other.m_move; }

		[[nodiscard]] static constexpr auto standard(Square src, Square dst)
		{
			return Move{static_cast<u16>(
				(static_cast<u16>(src) << 10)
				| (static_cast<u16>(dst) << 4)
				| static_cast<u16>(MoveType::Standard)
			)};
		}

		[[nodiscard]] static constexpr auto promotion(Square src, Square dst, PieceType target)
		{
			return Move{static_cast<u16>(
				(static_cast<u16>(src) << 10)
				| (static_cast<u16>(dst) << 4)
				| ((static_cast<u16>(target) - 1) << 2)
				| static_cast<u16>(MoveType::Promotion)
			)};
		}

		[[nodiscard]] static constexpr auto castling(Square src, Square dst)
		{
			return Move{static_cast<u16>(
				(static_cast<u16>(src) << 10)
				| (static_cast<u16>(dst) << 4)
				| static_cast<u16>(MoveType::Castling)
			)};
		}

		[[nodiscard]] static constexpr auto enPassant(Square src, Square dst)
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

	struct ExtendedMove
	{
		Piece moving{Piece::None};
		Square src{Square::None};
		Square dst{Square::None};
		Square kingDst{Square::None};
		MoveType type{MoveType::Standard};
		Piece captured{Piece::None};
		PieceType promo{PieceType::None};
		[[maybe_unused]] u8 padding{};

		[[nodiscard]] inline auto valid() const
		{
			return moving != Piece::None
				&& src != Square::None
				&& dst != Square::None
				&& src != dst
				&& (captured == Piece::None ||
					(pieceType(captured) != PieceType::King
						&& pieceColor(moving) != pieceColor(captured)))
				&& (type != MoveType::EnPassant || pieceType(captured) == PieceType::Pawn)
				&& (type == MoveType::Castling || dst == kingDst)
				&& (type != MoveType::Castling || captured == Piece::None) //TODO consider king dst
				&& ((type == MoveType::Promotion
					&& (promo == PieceType::Knight
						|| promo == PieceType::Bishop
						|| promo == PieceType::Rook
						|| promo == PieceType::Queen))
					|| (type != MoveType::Promotion && promo == PieceType::Knight /* default */));
		}

		[[nodiscard]] static inline auto fromMove(const PositionBoards &boards, Move move)
		{
			if (move == NullMove)
				return ExtendedMove{};

			const auto moving = boards.pieceAt(move.src());

			const auto kingDst = move.type() == MoveType::Castling
				? toSquare(move.srcRank(), move.srcFile() < move.dstFile() ? 6 : 2)
				: move.dst();

			const auto captured = [&]
			{
				if (move.type() == MoveType::Castling)
					return Piece::None;
				else if (move.type() == MoveType::EnPassant)
					return colorPiece(PieceType::Pawn, oppColor(pieceColor(moving)));
				else return boards.pieceAt(move.dst());
			}();

			return ExtendedMove {
				.moving = moving,
				.src = move.src(),
				.dst = move.dst(),
				.kingDst = kingDst,
				.type = move.type(),
				.captured = captured,
				.promo = move.target()
			};
		}
	};

	static_assert(sizeof(ExtendedMove) == 8);

	// assumed upper bound for number of possible moves is 218
	constexpr usize DefaultMoveListCapacity = 256;

	using MoveList = StaticVector<Move, DefaultMoveListCapacity>;
}
