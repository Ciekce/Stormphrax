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
    namespace {
        [[nodiscard]] constexpr u32 nextPowerOf2(u32 x) {
            --x;

            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;

            return x + 1;
        }
    } // namespace

    Score CorrhistAccessor::correct(const Position& pos, std::span<const u64> keyHistory, Score score) const {
        return corrhist->correct(numaId, pos, keyHistory, score);
    }

    void CorrhistAccessor::update(
        const Position& pos,
        std::span<const u64> keyHistory,
        i32 depth,
        Score searchScore,
        Score staticEval
    ) {
        corrhist->update(numaId, pos, keyHistory, depth, searchScore, staticEval);
    }

    CorrectionHistoryTable::CorrectionHistoryTable(u32 count) :
            m_count{nextPowerOf2(count)},
            m_tables{numa::NumaUniqueAllocation<EntrySet>{2 * kEntries * m_count}},
            m_cont{numa::NumaUniqueAllocation<Entry>{kContEntries * m_count}},
            m_mask{kEntries * m_count - 1},
            m_contMask{kContEntries * m_count - 1} {}

    void CorrectionHistoryTable::clear() {
        for (i32 numaNode = 0; numaNode < numa::nodeCount(); ++numaNode) {
            auto* tables = m_tables.get(numaNode);
            auto* cont = m_cont.get(numaNode);

            std::memset(tables, 0, sizeof(*tables));
            std::memset(cont, 0, sizeof(*cont));
        }
    }

    void CorrectionHistoryTable::update(
        u32 numaId,
        const Position& pos,
        std::span<const u64> keyHistory,
        i32 depth,
        Score searchScore,
        Score staticEval
    ) {
        auto* tables = m_tables.get(numaId) + (m_count * kEntries) * pos.stm().idx();
        auto* cont = m_cont.get(numaId);

        const auto bonus = std::clamp((searchScore - staticEval) * depth / 8, -kMaxBonus, kMaxBonus);

        const auto updateCont = [&](const u64 offset) {
            if (keyHistory.size() >= offset) {
                cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) & m_contMask].update(bonus);
            }
        };

        tables[pos.pawnKey() & m_mask].pawn.update(bonus);
        tables[pos.blackNonPawnKey() % kEntries].blackNonPawn.update(bonus);
        tables[pos.whiteNonPawnKey() % kEntries].whiteNonPawn.update(bonus);
        tables[pos.majorKey() % kEntries].major.update(bonus);

        updateCont(1);
        updateCont(2);
        updateCont(4);
    }

    Score CorrectionHistoryTable::correct(
        u32 numaId,
        const Position& pos,
        std::span<const u64> keyHistory,
        Score score
    ) const {
        using namespace tunable;

        const auto* tables = m_tables.get(numaId) + (m_count * kEntries) * pos.stm().idx();
        const auto* cont = m_cont.get(numaId);

        const auto contAdjustment = [&](const u64 offset, i32 weight) {
            if (keyHistory.size() >= offset) {
                return weight * cont[(pos.key() ^ keyHistory[keyHistory.size() - offset]) & m_contMask];
            } else {
                return 0;
            }
        };

        i32 correction{};

        correction += pawnCorrhistWeight() * tables[pos.pawnKey() % kEntries].pawn;
        correction += nonPawnCorrhistWeight() * tables[pos.blackNonPawnKey() % kEntries].blackNonPawn;
        correction += nonPawnCorrhistWeight() * tables[pos.whiteNonPawnKey() % kEntries].whiteNonPawn;
        correction += majorCorrhistWeight() * tables[pos.majorKey() % kEntries].major;

        correction += contAdjustment(1, contCorrhist1Weight());
        correction += contAdjustment(2, contCorrhist2Weight());
        correction += contAdjustment(4, contCorrhist4Weight());

        score += correction / 2048;

        return std::clamp(score, -kScoreWin + 1, kScoreWin - 1);
    }
} // namespace stormphrax
