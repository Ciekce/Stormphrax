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

#include "cuckoo.h"

#include <algorithm>
#include <cassert>

#include "hash.h"
#include "attacks/attacks.h"

namespace stormphrax::cuckoo
{
	// https://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
	// Implementation based on Stockfish's

	std::array<u64, 8192> keys{};
	std::array<Move, 8192> moves{};

	void init()
	{
		u32 count = 0;

		// skip pawns
		for (u32 p = static_cast<u32>(Piece::BlackKnight);
			p < static_cast<u32>(Piece::None);
			++p)
		{
			const auto piece = static_cast<Piece>(p);

			for (u32 s0 = 0; s0 < 64; ++s0)
			{
				const auto square0 = static_cast<Square>(s0);

				for (u32 s1 = s0 + 1; s1 < 64; ++s1)
				{
					const auto square1 = static_cast<Square>(s1);

					if (!attacks::getNonPawnPieceAttacks(pieceType(piece), square0)[square1])
						continue;

					auto move = Move::standard(square0, square1);
					auto key = hash::pieceSquare(piece, square0)
						^ hash::pieceSquare(piece, square1)
						^ hash::color();

					u32 slot = h1(key);

					while (true)
					{
						std::swap(keys[slot], key);
						std::swap(moves[slot], move);

						if (move == NullMove)
							break;

						slot = slot == h1(key) ? h2(key) : h1(key);
					}

					++count;
				}
			}
		}

		assert(count == 3668);
	}
}
