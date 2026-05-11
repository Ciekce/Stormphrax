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
#include "position/position.h"
#include "util/multi_array.h"

namespace stormphrax {
    class CorrectionHistoryTable {
    public:
        void clear();

        void update(
            const Position& pos,
            std::span<const u64> keyHistory,
            i32 depth,
            Score searchScore,
            Score staticEval
        );

        [[nodiscard]] Score correct(const Position& pos, std::span<const u64> keyHistory, Score score) const;

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

        struct SidedTables {
            std::array<Entry, kEntries> pawn{};
            std::array<Entry, kEntries> blackNonPawn{};
            std::array<Entry, kEntries> whiteNonPawn{};
            std::array<Entry, kEntries> major{};
        };

        std::array<SidedTables, Colors::kCount> m_tables{};
        std::array<Entry, kContEntries> m_cont{};
    };
} // namespace stormphrax
