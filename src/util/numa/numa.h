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

#include "../../types.h"

#include <span>
#include <vector>

#ifdef SP_USE_LIBNUMA
    #include <numa.h>
    #include <sched.h>
#else
    #include "../align.h"
#endif

namespace stormphrax::numa {
    [[nodiscard]] bool init();

    void bindThread(u32 threadId);

    [[nodiscard]] i32 nodeCount();

    template <typename T>
    class NumaUniqueAllocation {
    public:
        explicit NumaUniqueAllocation(usize count = 1);
        ~NumaUniqueAllocation();

        [[nodiscard]] T* get(u32 threadId);

    private:
        usize m_count;
        std::vector<T*> m_data{};
    };

#ifdef SP_USE_LIBNUMA
    std::span<const cpu_set_t> threadMapping();
    [[nodiscard]] i32 getNode(u32 threadId);

    template <typename T>
    NumaUniqueAllocation<T>::NumaUniqueAllocation(usize count) :
            m_count{count} {
        const auto nodes = nodeCount();
        m_data.reserve(nodes);
        for (i32 node = 0; node < nodes; ++node) {
            auto* ptr = static_cast<T*>(numa_alloc_onnode(sizeof(T) * count, node));
            for (usize i = 0; i < count; ++i) {
                new (&ptr[i]) T;
            }
            m_data.push_back(ptr);
        }
    }

    template <typename T>
    NumaUniqueAllocation<T>::~NumaUniqueAllocation() {
        for (auto* ptr : m_data) {
            for (usize i = 0; i < m_count; ++i) {
                ptr[i].~T();
            }
            numa_free(ptr, sizeof(T) * m_count);
        }
        m_data.clear();
    }

    template <typename T>
    T* NumaUniqueAllocation<T>::get(u32 threadId) {
        const auto node = getNode(threadId);
        return m_data[node];
    }
#else
    template <typename T>
    NumaUniqueAllocation<T>::NumaUniqueAllocation(usize count) :
            m_count{count} {
        m_data.reserve(1);
        auto* ptr = util::alignedAlloc<T>(4096, count);
        for (usize i = 0; i < count; ++i) {
            new (&ptr[i]) T;
        }
        m_data.push_back(ptr);
    }

    template <typename T>
    NumaUniqueAllocation<T>::~NumaUniqueAllocation() {
        for (auto* ptr : m_data) {
            for (usize i = 0; i < m_count; ++i) {
                ptr[i].~T();
            }
            util::alignedFree(ptr);
        }
        m_count = 0;
        m_data.clear();
    }

    template <typename T>
    T* NumaUniqueAllocation<T>::get(u32 threadId) {
        SP_UNUSED(threadId);
        return m_data[0];
    }
#endif
} // namespace stormphrax::numa
