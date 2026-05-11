/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "correction.h"

#include <algorithm>
#include <cstring>

#include "tunable.h"

namespace stormphrax {
    void CorrectionHistoryTable::clear() {
        std::memset(&m_tables, 0, sizeof(m_tables));
        std::memset(&m_cont, 0, sizeof(m_cont));
    }

    void CorrectionHistoryTable::update(
        const Position& pos,
        std::span<const u64> keyHistory,
        i32 depth,
        Score searchScore,
        Score staticEval
    ) {
        auto& tables = m_tables[pos.stm().idx()];

        const auto bonus = std::clamp((searchScore - staticEval) * depth / 8, -kMaxBonus, kMaxBonus);

        const auto updateCont = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kContEntries].update(bonus);
            }
        };

        tables.pawn[pos.pawnKey() % kEntries].update(bonus);
        tables.blackNonPawn[pos.blackNonPawnKey() % kEntries].update(bonus);
        tables.whiteNonPawn[pos.whiteNonPawnKey() % kEntries].update(bonus);
        tables.major[pos.majorKey() % kEntries].update(bonus);

        updateCont(1);
        updateCont(2);
        updateCont(4);
    }

    Score CorrectionHistoryTable::correct(const Position& pos, std::span<const u64> keyHistory, Score score) const {
        using namespace tunable;

        const auto& tables = m_tables[pos.stm().idx()];

        const auto contAdjustment = [&](const u64 offset, i32 weight) {
            if (keyHistory.size() >= offset) {
                return weight * m_cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kContEntries];
            } else {
                return 0;
            }
        };

        i32 correction{};

        correction += pawnCorrhistWeight() * tables.pawn[pos.pawnKey() % kEntries];
        correction += nonPawnCorrhistWeight() * tables.blackNonPawn[pos.blackNonPawnKey() % kEntries];
        correction += nonPawnCorrhistWeight() * tables.whiteNonPawn[pos.whiteNonPawnKey() % kEntries];
        correction += majorCorrhistWeight() * tables.major[pos.majorKey() % kEntries];

        correction += contAdjustment(1, contCorrhist1Weight());
        correction += contAdjustment(2, contCorrhist2Weight());
        correction += contAdjustment(4, contCorrhist4Weight());

        score += correction / 2048;

        return std::clamp(score, -kScoreWin + 1, kScoreWin - 1);
    }
} // namespace stormphrax
