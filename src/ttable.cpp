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

#include "ttable.h"

#include <bit>
#include <cstring>
#include <iostream>
#include <thread>

#include "tunable.h"
#include "util/align.h"
#include "util/cemath.h"

namespace stormphrax {
    using namespace stormphrax::tunable;

    namespace {
        // for a long time, these were backwards
        // cheers toanth
        inline auto scoreToTt(Score score, i32 ply) {
            if (score < -ScoreWin) {
                return score - ply;
            } else if (score > ScoreWin) {
                return score + ply;
            }
            return score;
        }

        inline auto scoreFromTt(Score score, i32 ply) {
            if (score < -ScoreWin) {
                return score + ply;
            } else if (score > ScoreWin) {
                return score - ply;
            }
            return score;
        }

        inline auto packEntryKey(u64 key) {
            return static_cast<u16>(key);
        }
    } // namespace

    TTable::TTable(usize size) {
        resize(size);
    }

    TTable::~TTable() {
        if (m_table) {
            util::alignedFree(m_table);
        }
    }

    auto TTable::resize(usize size) -> void {
        size *= 1024 * 1024;

        const auto capacity = size / sizeof(Cluster);

        // don't bother reallocating if we're already at the right size
        if (m_tableSize != capacity) {
            if (m_table) {
                util::alignedFree(m_table);
            }

            m_table = nullptr;
            m_tableSize = capacity;
        }

        m_pendingInit = true;
    }

    auto TTable::finalize() -> bool {
        if (!m_pendingInit) {
            return false;
        }

        m_pendingInit = false;
        m_table = util::alignedAlloc<Cluster>(StorageAlignment, m_tableSize);

        if (!m_table) {
            std::cout << "info string Failed to reallocate TT - out of memory?" << std::endl;
            std::terminate();
        }

        clear();

        return true;
    }

    auto TTable::probe(ProbedTTableEntry &dst, u64 key, i32 ply) const -> bool {
        assert(!m_pendingInit);

        const auto packedKey = packEntryKey(key);

        const auto &cluster = m_table[index(key)];
        for (const auto entry: cluster.entries) {
            if (packedKey == entry.key) {
                dst.score = scoreFromTt(static_cast<Score>(entry.score), ply);
                dst.staticEval = static_cast<Score>(entry.staticEval);
                dst.depth = entry.depth;
                dst.move = entry.move;
                dst.wasPv = entry.pv();
                dst.flag = entry.flag();

                return true;
            }
        }

        return false;
    }

    auto TTable::put(
        u64 key,
        Score score,
        Score staticEval,
        Move move,
        i32 depth,
        i32 ply,
        TtFlag flag,
        bool pv
    ) -> void {
        assert(!m_pendingInit);

        assert(depth >= 0);
        assert(depth <= MaxDepth);

        assert(staticEval == ScoreNone || staticEval > -ScoreWin);
        assert(staticEval == ScoreNone || staticEval < ScoreWin);

        const auto newKey = packEntryKey(key);

        const auto entryValue = [this](const auto &entry) {
            const i32 relativeAge = (Entry::AgeCycle + m_age - entry.age()) & Entry::AgeMask;
            return entry.depth - relativeAge * 2;
        };

        auto &cluster = m_table[index(key)];

        Entry *entryPtr = nullptr;
        auto minValue = std::numeric_limits<i32>::max();

        for (auto &candidate: cluster.entries) {
            // always take an empty entry, or one from the same position
            if (candidate.key == newKey || candidate.flag() == TtFlag::None) {
                entryPtr = &candidate;
                break;
            }

            // otherwise, take the lowest-weighted entry by depth and age
            const auto value = entryValue(candidate);

            if (value < minValue) {
                entryPtr = &candidate;
                minValue = value;
            }
        }

        assert(entryPtr != nullptr);

        auto entry = *entryPtr;

        // Roughly the SF replacement scheme
        if (!(flag == TtFlag::Exact || newKey != entry.key || entry.age() != m_age
              || depth + ttReplacementDepthOffset() + pv * ttReplacementPvOffset() > entry.depth))
        {
            return;
        }

        if (move || entry.key != newKey) {
            entry.move = move;
        }

        entry.key = newKey;
        entry.score = static_cast<i16>(scoreToTt(score, ply));
        entry.staticEval = static_cast<i16>(staticEval);
        entry.depth = depth;
        entry.setAgePvFlag(m_age, pv, flag);

        *entryPtr = entry;
    }

    auto TTable::clear() -> void {
        assert(!m_pendingInit);

        const auto threadCount = g_opts.threads;

        std::vector<std::thread> threads{};
        threads.reserve(threadCount);

        const auto chunkSize = util::ceilDiv<usize>(m_tableSize, threadCount);

        for (u32 i = 0; i < threadCount; ++i) {
            threads.emplace_back([this, chunkSize, i] {
                const auto start = chunkSize * i;
                const auto end = std::min(start + chunkSize, m_tableSize);

                const auto count = end - start;

                std::memset(&m_table[start], 0, count * sizeof(Cluster));
            });
        }

        m_age = 0;

        for (auto &thread: threads) {
            thread.join();
        }
    }

    auto TTable::full() const -> u32 {
        assert(!m_pendingInit);

        u32 filledEntries{};

        for (u64 i = 0; i < 1000; ++i) {
            const auto cluster = m_table[i];
            for (const auto &entry: cluster.entries) {
                if (entry.flag() != TtFlag::None && entry.age() == m_age) {
                    ++filledEntries;
                }
            }
        }

        return filledEntries / Cluster::EntriesPerCluster;
    }
} // namespace stormphrax
