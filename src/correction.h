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
#include <atomic>
#include <cstring>

#include "core.h"
#include "position/position.h"
#include "tunable.h"
#include "util/cemath.h"
#include "util/multi_array.h"

namespace stormphrax {
    struct PlayedMove {
        Piece moving;
        Square dst;
    };

    class CorrectionHistoryTable {
    public:
        inline void clear() {
            std::memset(&m_pawnTable, 0, sizeof(m_pawnTable));
            std::memset(&m_blackNonPawnTable, 0, sizeof(m_blackNonPawnTable));
            std::memset(&m_whiteNonPawnTable, 0, sizeof(m_whiteNonPawnTable));
            std::memset(&m_majorTable, 0, sizeof(m_majorTable));
            std::memset(&m_contTable, 0, sizeof(m_contTable));
        }

        inline void update(
            const Position& pos,
            std::span<const u64> keyHistory,
            i32 depth,
            Score searchScore,
            Score staticEval
        ) {
            const auto bonus = std::clamp((searchScore - staticEval) * depth / 8, -kMaxBonus, kMaxBonus);

            const auto stm = pos.stm().idx();

            const auto updateCont = [&](const u64 offset) {
                if (keyHistory.size() >= offset) {
                    m_contTable[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries].update(bonus);
                }
            };

            m_pawnTable[stm][pos.pawnKey() % kEntries].update(bonus);
            m_blackNonPawnTable[stm][pos.blackNonPawnKey() % kEntries].update(bonus);
            m_whiteNonPawnTable[stm][pos.whiteNonPawnKey() % kEntries].update(bonus);
            m_majorTable[stm][pos.majorKey() % kEntries].update(bonus);

            updateCont(1);
            updateCont(2);
        }

        [[nodiscard]] inline Score correct(
            const Position& pos,
            std::span<const u64> keyHistory,
            Score score
        ) const {
            using namespace tunable;

            const auto stm = pos.stm().idx();

            const auto contAdjustment = [&](const u64 offset, i32 weight) {
                if (keyHistory.size() >= offset) {
                    return weight * m_contTable[(pos.key() ^ keyHistory[keyHistory.size() - offset]) % kEntries];
                } else {
                    return 0;
                }
            };

            const auto [blackNpWeight, whiteNpWeight] =
                pos.stm() == Colors::kBlack ? std::pair{stmNonPawnCorrhistWeight(), nstmNonPawnCorrhistWeight()}
                                            : std::pair{nstmNonPawnCorrhistWeight(), stmNonPawnCorrhistWeight()};

            i32 correction{};

            correction += pawnCorrhistWeight() * m_pawnTable[stm][pos.pawnKey() % kEntries];
            correction += blackNpWeight * m_blackNonPawnTable[stm][pos.blackNonPawnKey() % kEntries];
            correction += whiteNpWeight * m_whiteNonPawnTable[stm][pos.whiteNonPawnKey() % kEntries];
            correction += majorCorrhistWeight() * m_majorTable[stm][pos.majorKey() % kEntries];

            correction += contAdjustment(1, contCorrhist1Weight());
            correction += contAdjustment(2, contCorrhist2Weight());

            score += correction / 2048;

            return std::clamp(score, -kScoreWin + 1, kScoreWin - 1);
        }

    private:
        static constexpr usize kEntries = 16384;

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

        util::MultiArray<Entry, Colors::kCount, kEntries> m_pawnTable{};
        util::MultiArray<Entry, Colors::kCount, kEntries> m_blackNonPawnTable{};
        util::MultiArray<Entry, Colors::kCount, kEntries> m_whiteNonPawnTable{};
        util::MultiArray<Entry, Colors::kCount, kEntries> m_majorTable{};
        std::array<Entry, kEntries> m_contTable{};
    };
} // namespace stormphrax
