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

#pragma once

#include "../types.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>

namespace stormphrax::util {
    class Barrier {
    public:
        explicit Barrier(i64 expected) {
            reset(expected);
        }

        auto reset(i64 expected) -> void {
            assert(expected > 0);
            assert(m_current.load() == m_total.load());

            m_total.store(expected, std::memory_order::seq_cst);
            m_current.store(expected, std::memory_order::seq_cst);
        }

        auto arriveAndWait() {
            std::unique_lock lock{m_waitMutex};

            const auto current = --m_current;

            if (current > 0) {
                const auto phase = m_phase.load(std::memory_order::relaxed);
                m_waitSignal.wait(lock, [this, phase] {
                    return (phase - m_phase.load(std::memory_order::acquire)) < 0;
                });
            } else {
                const auto total = m_total.load(std::memory_order::acquire);
                m_current.store(total, std::memory_order::release);

                ++m_phase;

                m_waitSignal.notify_all();
            }
        }

    private:
        std::atomic<i64> m_total{};
        std::atomic<i64> m_current{};
        std::atomic<i64> m_phase{};

        std::mutex m_waitMutex{};
        std::condition_variable m_waitSignal{};
    };
} // namespace stormphrax::util
