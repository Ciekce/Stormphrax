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

#include <array>

#include "core.h"
#include "position.h"
#include "attacks/attacks.h"

namespace polaris::see
{
	namespace values
	{
		constexpr Score Pawn = 100;
		constexpr Score Knight = 300;
		constexpr Score Bishop = 325;
		constexpr Score Rook = 600;
		constexpr Score Queen = 1100;
		constexpr Score King = 99999;
	}

	constexpr auto Values = std::array {
		values::Pawn,
		values::Pawn,
		values::Knight,
		values::Knight,
		values::Bishop,
		values::Bishop,
		values::Rook,
		values::Rook,
		values::Queen,
		values::Queen,
		values::King,
		values::King,
		static_cast<Score>(0)
	};

	constexpr Score value(Piece piece)
	{
		return Values[static_cast<i32>(piece)];
	}

	constexpr Score value(BasePiece piece)
	{
		return Values[static_cast<i32>(piece) * 2];
	}

	inline Score gain(const Position &pos, Move move)
	{
		const auto type = move.type();

		if (type == MoveType::Castling)
			return 0;
		else if (type == MoveType::EnPassant)
			return values::Pawn;

		auto score = value(pos.pieceAt(move.dst()));

		if (type == MoveType::Promotion)
			score += value(move.target()) - values::Pawn;

		return score;
	}

	[[nodiscard]] inline BasePiece popLeastValuable(const Position &pos, Bitboard &occ, Bitboard attackers, Color color)
	{
		auto board = attackers & pos.pawns(color);
		if (!board.empty())
		{
			occ ^= board.lowestBit();
			return BasePiece::Pawn;
		}

		board = attackers & pos.knights(color);
		if (!board.empty())
		{
			occ ^= board.lowestBit();
			return BasePiece::Knight;
		}

		board = attackers & pos.bishops(color);
		if (!board.empty())
		{
			occ ^= board.lowestBit();
			return BasePiece::Bishop;
		}

		board = attackers & pos.rooks(color);
		if (!board.empty())
		{
			occ ^= board.lowestBit();
			return BasePiece::Rook;
		}

		board = attackers & pos.queens(color);
		if (!board.empty())
		{
			occ ^= board.lowestBit();
			return BasePiece::Queen;
		}

		board = attackers & pos.kings(color);
		if (!board.empty())
		{
			occ ^= board.lowestBit();
			return BasePiece::King;
		}

		return BasePiece::None;
	}

	// basically ported from ethereal and weiss (their implementation is the same)
	inline bool see(const Position &pos, Move move)
	{
		const auto color = pos.toMove();

		auto next = move.type() == MoveType::Promotion
			? move.target()
			: basePiece(pos.pieceAt(move.src()));

		auto score = gain(pos, move) - value(next);

		if (score >= 0)
			return true;

		const auto square = move.dst();

		auto occupancy = pos.occupancy()
			^ squareBit(move.src())
			^ squareBit(square);

		const auto queens = pos.blackQueens() | pos.whiteQueens();

		const auto bishops = queens | pos.blackBishops() | pos.whiteBishops();
		const auto rooks = queens | pos.blackRooks() | pos.whiteRooks();

		auto attackers = pos.allAttackersTo(square, occupancy);

		auto us = oppColor(color);

		while (true)
		{
			const auto ourAttackers = attackers & pos.occupancy(us);

			if (ourAttackers.empty())
				break;

			next = popLeastValuable(pos, occupancy, ourAttackers, us);

			if (next == BasePiece::Pawn
				|| next == BasePiece::Bishop
				|| next == BasePiece::Queen)
				attackers |= attacks::getBishopAttacks(square, occupancy) & bishops;

			if (next == BasePiece::Rook
				|| next == BasePiece::Queen)
				attackers |= attacks::getRookAttacks(square, occupancy) & rooks;

			attackers &= occupancy;

			score = -score - 1 - value(next);
			us = oppColor(us);

			if (score >= 0)
			{
				// our only attacker is our king, but the opponent still has defenders
				if (next == BasePiece::King
					&& !(attackers & pos.occupancy(us)).empty())
					us = oppColor(us);
				break;
			}
		}

		return color != us;
	}
}
