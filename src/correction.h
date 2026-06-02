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

#pragma once

#include "types.h"

#include <atomic>

#include "core.h"
#include "position.h"
#include "util/numa/numa.h"

namespace stormphrax {
    class CorrectionHistoryTable;

    struct CorrhistAccessor {
        CorrectionHistoryTable* corrhist{nullptr};
        u32 numaId{0};

        void update(
            const Position& pos,
            std::span<const u64> keyHistory,
            i32 depth,
            Score searchScore,
            Score staticEval
        );

        [[nodiscard]] Score correct(const Position& pos, std::span<const u64> keyHistory, Score score) const;
    };

    class CorrectionHistoryTable {
    public:
        explicit CorrectionHistoryTable(u32 count);

        void clear();

        void update(
            u32 numaId,
            const Position& pos,
            std::span<const u64> keyHistory,
            i32 depth,
            Score searchScore,
            Score staticEval
        );

        [[nodiscard]] Score correct(
            u32 numaId,
            const Position& pos,
            std::span<const u64> keyHistory,
            Score score
        ) const;

        [[nodiscard]] u32 count() const {
            return m_count;
        }

        [[nodiscard]] CorrhistAccessor getAccessor(u32 numaId) {
            return CorrhistAccessor{
                .corrhist = this,
                .numaId = numaId,
            };
        }

    private:
        static constexpr usize kEntries = 16384;
        static constexpr usize kContEntries = 32768;

        static constexpr i32 kLimit = 1024;
        static constexpr i32 kMaxBonus = kLimit / 4;

        struct Entry {
            std::atomic<i16> value{};

            inline void update(i32 bonus) {
                auto v = value.load(std::memory_order::relaxed);
                v += bonus - v * std::abs(bonus) / kLimit;
                value.store(v, std::memory_order::relaxed);
            }

            [[nodiscard]] inline operator i32() const {
                return value.load(std::memory_order::relaxed);
            }
        };

        struct EntrySet {
            Entry pawn{};
            Entry blackNonPawn{};
            Entry whiteNonPawn{};
            Entry major{};
        };

        u32 m_count;

        numa::NumaUniqueAllocation<EntrySet> m_tables{};
        numa::NumaUniqueAllocation<Entry> m_cont{};

        usize m_mask;
        usize m_contMask;
    };
} // namespace stormphrax
