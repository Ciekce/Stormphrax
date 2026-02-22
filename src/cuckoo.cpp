/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

#include "attacks/attacks.h"
#include "keys.h"

namespace stormphrax::cuckoo {
    // https://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
    // Implementation based on Stockfish's

    std::array<u64, 8192> keys{};
    std::array<Move, 8192> moves{};

    void init() {
        [[maybe_unused]] u32 count = 0;

        // skip pawns
        for (u32 p = Pieces::kBlackKnight.raw(); p < Pieces::kNone.raw(); ++p) {
            const auto piece = Piece::fromRaw(p);

            for (u32 s0 = 0; s0 < Squares::kCount; ++s0) {
                const auto sq0 = Square::fromRaw(s0);

                for (u32 s1 = s0 + 1; s1 < Squares::kCount; ++s1) {
                    const auto sq1 = Square::fromRaw(s1);

                    if (!attacks::getAttacks(piece, sq0)[sq1]) {
                        continue;
                    }

                    auto move = Move::standard(sq0, sq1);
                    auto key = keys::pieceSquare(piece, sq0) ^ keys::pieceSquare(piece, sq1) ^ keys::color();

                    u32 slot = h1(key);

                    while (true) {
                        std::swap(keys[slot], key);
                        std::swap(moves[slot], move);

                        if (move == kNullMove) {
                            break;
                        }

                        slot = slot == h1(key) ? h2(key) : h1(key);
                    }

                    ++count;
                }
            }
        }

        assert(count == 3668);
    }
} // namespace stormphrax::cuckoo
