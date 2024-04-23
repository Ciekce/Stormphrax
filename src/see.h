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

#include <array>

#include "core.h"
#include "position/position.h"
#include "attacks/attacks.h"

namespace stormphrax::see
{
	namespace values
	{
		constexpr Score Pawn = 100;
		constexpr Score Knight = 450;
		constexpr Score Bishop = 450;
		constexpr Score Rook = 650;
		constexpr Score Queen = 1250;
		constexpr Score King = 0;
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

	constexpr auto value(Piece piece)
	{
		return Values[static_cast<i32>(piece)];
	}

	constexpr auto value(PieceType piece)
	{
		return Values[static_cast<i32>(piece) * 2];
	}

	inline auto gain(const PositionBoards &boards, Move move)
	{
		const auto type = move.type();

		if (type == MoveType::Castling)
			return 0;
		else if (type == MoveType::EnPassant)
			return values::Pawn;

		auto score = value(boards.pieceAt(move.dst()));

		if (type == MoveType::Promotion)
			score += value(move.promo()) - values::Pawn;

		return score;
	}

	[[nodiscard]] inline auto popLeastValuable(const BitboardSet &bbs,
		Bitboard &occ, Bitboard attackers, Color color)
	{
		for (i32 i = 0; i < 6; ++i)
		{
			const auto piece = static_cast<PieceType>(i);
			auto board = attackers & bbs.forPiece(piece, color);

			if (!board.empty())
			{
				occ ^= board.lowestBit();
				return piece;
			}
		}

		return PieceType::None;
	}

	// basically ported from ethereal and weiss (their implementation is the same)
	inline auto see(const Position &pos, Move move, Score threshold)
	{
		const auto &boards = pos.boards();
		const auto &bbs = boards.bbs();

		const auto color = pos.toMove();

		auto score = gain(boards, move) - threshold;

		if (score < 0)
			return false;

		auto next = move.type() == MoveType::Promotion
			? move.promo()
			: pieceType(boards.pieceAt(move.src()));

		score -= value(next);

		if (score >= 0)
			return true;

		const auto square = move.dst();

		auto occupancy = bbs.occupancy()
			^ squareBit(move.src())
			^ squareBit(square);

		const auto queens = bbs.queens();

		const auto bishops = queens | bbs.bishops();
		const auto rooks = queens | bbs.rooks();

		auto attackers = pos.allAttackersTo(square, occupancy);

		auto us = oppColor(color);

		while (true)
		{
			const auto ourAttackers = attackers & bbs.forColor(us);

			if (ourAttackers.empty())
				break;

			next = popLeastValuable(bbs, occupancy, ourAttackers, us);

			if (next == PieceType::Pawn
				|| next == PieceType::Bishop
				|| next == PieceType::Queen)
				attackers |= attacks::getBishopAttacks(square, occupancy) & bishops;

			if (next == PieceType::Rook
				|| next == PieceType::Queen)
				attackers |= attacks::getRookAttacks(square, occupancy) & rooks;

			attackers &= occupancy;

			score = -score - 1 - value(next);
			us = oppColor(us);

			if (score >= 0)
			{
				// our only attacker is our king, but the opponent still has defenders
				if (next == PieceType::King
					&& !(attackers & bbs.forColor(us)).empty())
					us = oppColor(us);
				break;
			}
		}

		return color != us;
	}
}
