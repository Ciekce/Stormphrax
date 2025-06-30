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

#pragma once

#include "types.h"

#include <algorithm>
#include <cstring>

#include "core.h"
#include "position/position.h"
#include "search_fwd.h"
#include "tunable.h"
#include "util/cemath.h"
#include "util/multi_array.h"

namespace stormphrax {
    class CorrectionHistoryTable {
    public:
        inline void clear() {
            std::memset(&m_pawnTable, 0, sizeof(m_pawnTable));
            std::memset(&m_blackNonPawnTable, 0, sizeof(m_blackNonPawnTable));
            std::memset(&m_whiteNonPawnTable, 0, sizeof(m_whiteNonPawnTable));
            std::memset(&m_majorTable, 0, sizeof(m_majorTable));
            std::memset(&m_minorTable, 0, sizeof(m_minorTable));
            std::memset(&m_contTable, 0, sizeof(m_contTable));
        }

        inline void update(
            const Position& pos,
            std::span<search::PlayedMove> moves,
            i32 ply,
            i32 depth,
            Score searchScore,
            Score staticEval
        ) {
            const auto bonus = std::clamp((searchScore - staticEval) * depth / 8, -kMaxBonus, kMaxBonus);

            const auto stm = static_cast<i32>(pos.stm());

            m_pawnTable[stm][pos.pawnKey() % kEntries].update(bonus);
            m_blackNonPawnTable[stm][pos.blackNonPawnKey() % kEntries].update(bonus);
            m_whiteNonPawnTable[stm][pos.whiteNonPawnKey() % kEntries].update(bonus);
            m_majorTable[stm][pos.majorKey() % kEntries].update(bonus);
            m_minorTable[stm][pos.minorKey() % kEntries].update(bonus);

            if (ply >= 2) {
                const auto [moving2, dst2] = moves[ply - 2];
                const auto [moving1, dst1] = moves[ply - 1];

                if (moving2 != Piece::kNone && moving1 != Piece::kNone) {
                    m_contTable[stm][static_cast<i32>(pieceType(moving2))][static_cast<i32>(dst2)]
                               [static_cast<i32>(pieceType(moving1))][static_cast<i32>(dst1)]
                                   .update(bonus);
                }
            }
        }

        [[nodiscard]] inline Score correct(
            const Position& pos,
            std::span<search::PlayedMove> moves,
            i32 ply,
            Score score
        ) const {
            using namespace tunable;

            const auto stm = static_cast<i32>(pos.stm());

            const auto [blackNpWeight, whiteNpWeight] =
                pos.stm() == Color::kBlack ? std::pair{stmNonPawnCorrhistWeight(), nstmNonPawnCorrhistWeight()}
                                           : std::pair{nstmNonPawnCorrhistWeight(), stmNonPawnCorrhistWeight()};

            i32 correction{};

            correction += pawnCorrhistWeight() * m_pawnTable[stm][pos.pawnKey() % kEntries];
            correction += blackNpWeight * m_blackNonPawnTable[stm][pos.blackNonPawnKey() % kEntries];
            correction += whiteNpWeight * m_whiteNonPawnTable[stm][pos.whiteNonPawnKey() % kEntries];
            correction += majorCorrhistWeight() * m_majorTable[stm][pos.majorKey() % kEntries];
            correction += 128 * m_minorTable[stm][pos.minorKey() % kEntries];

            if (ply >= 2) {
                const auto [moving2, dst2] = moves[ply - 2];
                const auto [moving1, dst1] = moves[ply - 1];

                if (moving2 != Piece::kNone && moving1 != Piece::kNone) {
                    correction += contCorrhistWeight()
                                * m_contTable[stm][static_cast<i32>(pieceType(moving2))][static_cast<i32>(dst2)]
                                             [static_cast<i32>(pieceType(moving1))][static_cast<i32>(dst1)];
                }
            }

            score += correction / 2048;

            return std::clamp(score, -kScoreWin + 1, kScoreWin - 1);
        }

    private:
        static constexpr usize kEntries = 16384;

        static constexpr i32 kLimit = 1024;
        static constexpr i32 kMaxBonus = kLimit / 4;

        struct Entry {
            i16 value{};

            inline void update(i32 bonus) {
                value += bonus - value * std::abs(bonus) / kLimit;
            }

            [[nodiscard]] inline operator i32() const {
                return value;
            }
        };

        util::MultiArray<Entry, 2, kEntries> m_pawnTable{};
        util::MultiArray<Entry, 2, kEntries> m_blackNonPawnTable{};
        util::MultiArray<Entry, 2, kEntries> m_whiteNonPawnTable{};
        util::MultiArray<Entry, 2, kEntries> m_majorTable{};
        util::MultiArray<Entry, 2, kEntries> m_minorTable{};
        util::MultiArray<Entry, 2, 6, 64, 6, 64> m_contTable{};
    };
} // namespace stormphrax
