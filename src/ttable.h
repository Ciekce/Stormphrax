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

#include <atomic>
#include <bit>
#include <cstring>
#include <vector>

#include "arch.h"
#include "core.h"
#include "move.h"
#include "util/range.h"

namespace stormphrax {
    constexpr usize kDefaultTtSizeMib = 64;
    constexpr util::Range<usize> kTtSizeMibRange{1, 67108864};

    enum class TtFlag : u8 { kNone = 0, kUpperBound, kLowerBound, kExact };

    struct ProbedTTableEntry {
        Score score;
        Score staticEval;
        i32 depth;
        Move move;
        bool wasPv;
        TtFlag flag;
    };

    class TTable {
    public:
        explicit TTable(usize mib = kDefaultTtSizeMib);
        ~TTable();

        void resize(usize mib);
        bool finalize();

        bool probe(ProbedTTableEntry& dst, u64 key, i32 ply) const;
        void put(u64 key, Score score, Score staticEval, Move move, i32 depth, i32 ply, TtFlag flag, bool pv);

        inline void age() {
            m_age = (m_age + 1) % (1 << Entry::kAgeBits);
        }

        void clear();

        [[nodiscard]] u32 full() const;

        inline void prefetch(u64 key) {
            __builtin_prefetch(&m_clusters[index(key)]);
        }

    private:
        struct Entry {
            static constexpr u32 kAgeBits = 5;

            static constexpr u32 kAgeCycle = 1 << kAgeBits;
            static constexpr u32 kAgeMask = kAgeCycle - 1;

            u16 key;
            i16 score;
            i16 staticEval;
            Move move;
            u8 depth;
            u8 agePvFlag;

            [[nodiscard]] inline u32 age() const {
                return static_cast<u32>(agePvFlag >> 3);
            }

            [[nodiscard]] inline bool pv() const {
                return (static_cast<u32>(agePvFlag >> 2) & 1) != 0;
            }

            [[nodiscard]] inline TtFlag flag() const {
                return static_cast<TtFlag>(agePvFlag & 0x3);
            }

            inline void setAgePvFlag(u32 age, bool pv, TtFlag flag) {
                assert(age < (1 << kAgeBits));
                agePvFlag = (age << 3) | (static_cast<u32>(pv) << 2) | static_cast<u32>(flag);
            }
        };

        static_assert(sizeof(Entry) == 10);

        static constexpr usize kClusterAlignment = 32;
        static constexpr auto kStorageAlignment = std::max(kCacheLineSize, kClusterAlignment);

        struct alignas(32) Cluster {
            static constexpr usize kEntriesPerCluster = 3;

            std::array<Entry, kEntriesPerCluster> entries{};

            // round up to nearest power of 2 bytes
            [[maybe_unused]] std::array<
                u8,
                std::bit_ceil(sizeof(Entry) * kEntriesPerCluster) - sizeof(Entry) * kEntriesPerCluster> padding{};
        };

        [[nodiscard]] inline u64 index(u64 key) const {
            // this emits a single mul on both x64 and arm64
            return static_cast<u64>((static_cast<u128>(key) * static_cast<u128>(m_clusterCount)) >> 64);
        }

        // Only accessed from UCI thread
        bool m_pendingInit{};

        Cluster* m_clusters{};
        usize m_clusterCount{};

        u32 m_age{};
    };
} // namespace stormphrax
