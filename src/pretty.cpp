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

#include "pretty.h"

namespace stormphrax
{
	auto printBitboard(std::ostream &out, Bitboard board) -> void
	{
		for (i32 rank = 7; rank >= 0; --rank)
		{
			out << " +---+---+---+---+---+---+---+---+\n";

			for (usize file = 0; file < 8; ++file)
			{
				out << " | " << (board[Square::fromRankFile(rank, file)] ? '1' : ' ');
			}

			out << " | " << (rank + 1) << "\n";
		}

		out << " +---+---+---+---+---+---+---+---+\n";
		out << "   a   b   c   d   e   f   g   h\n\n";
	}

	auto printBitboardCompact(std::ostream &out, Bitboard board) -> void
	{
		for (i32 rank = 7; rank >= 0; --rank)
		{
			for (usize file = 0; file < 8; ++file)
			{
				if (file > 0)
					out << ' ';

				out << (board[Square::fromRankFile(rank, file)] ? '1' : '.');
			}

			out << "\n";
		}
	}

	auto printBoard(std::ostream &out, const Position &position) -> void
	{
		const auto &boards = position.boards();

		for (i32 rank = 7; rank >= 0; --rank)
		{
			out << " +---+---+---+---+---+---+---+---+\n";

			for (usize file = 0; file < 8; ++file)
			{
				out << " | " << boards.pieceAt(rank, file).toChar();
			}

			out << " | " << (rank + 1) << "\n";
		}

		out << " +---+---+---+---+---+---+---+---+\n";
		out << "   a   b   c   d   e   f   g   h\n\n";

		out << (position.toMove() == colors::White ? "White" : "Black") << " to move\n";
	}

	auto printScore(std::ostream &out, Score score) -> void
	{
		if (score == 0)
		{
			out << "0.00";
			return;
		}

		out << (score < 0 ? '-' : '+');

		if (score < 0)
			score = -score;

		out << (score / 100) << '.';

		const auto d = score % 100;

		if (d < 10)
			out << '0';

		out << d;
	}
}
