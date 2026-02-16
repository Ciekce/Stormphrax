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

#ifdef SP_USE_LIBNUMA
    #include "numa.h"

    #include "numa.h"

    #include <pthread.h>

namespace stormphrax::numa {
    bool init() {
        if (numa_available() < 0) {
            return false;
        }

        threadMapping();

        println("{} NUMA nodes", nodeCount());

        return true;
    }

    void bindThread(u32 threadId) {
        const auto node = getNode(threadId);
        const auto handle = pthread_self();
        const auto* cpuSet = &threadMapping()[node];
        pthread_setaffinity_np(handle, sizeof(cpu_set_t), cpuSet);
    }

    i32 nodeCount() {
        return static_cast<i32>(threadMapping().size());
    }

    std::span<const cpu_set_t> threadMapping() {
        static const auto s_mapping = [] {
            const auto maxNode = numa_max_node();

            std::vector<cpu_set_t> masks{};
            masks.reserve(maxNode + 1);

            for (i32 node = 0; node <= maxNode; ++node) {
                auto* cpumask = numa_allocate_cpumask();

                if (numa_node_to_cpus(node, cpumask) != 0) {
                    eprintln("failed to get CPU mask for NUMA node {}", node);
                    std::terminate();
                }

                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);

                for (u32 cpu = 0; cpu < cpumask->size; ++cpu) {
                    if (numa_bitmask_isbitset(cpumask, cpu)) {
                        CPU_SET(cpu, &cpuset);
                    }
                }

                numa_free_cpumask(cpumask);
                masks.push_back(cpuset);
            }

            return masks;
        }();

        return s_mapping;
    }

    i32 getNode(u32 threadId) {
        return static_cast<i32>(threadId % nodeCount());
    }
} // namespace stormphrax::numa
#endif
