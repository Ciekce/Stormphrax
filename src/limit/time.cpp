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

#include "time.h"

#include <algorithm>
#include <cmath>

#include "../tunable.h"

namespace stormphrax::limit {
    using namespace stormphrax::tunable;

    using util::Instant;

    MoveTimeLimiter::MoveTimeLimiter(i64 time, i64 overhead) :
            m_endTime{Instant::now() + static_cast<f64>(std::max<i64>(1, time - overhead)) / 1000.0} {}

    bool MoveTimeLimiter::stop(const search::SearchData& data, [[maybe_unused]] bool allowSoftTimeout) {
        if (data.rootDepth > 2 && data.nodes > 0 && (data.nodes % 1024) == 0 && Instant::now() >= m_endTime) {
            m_stopped.store(true, std::memory_order_release);
            return true;
        }

        return false;
    }

    bool MoveTimeLimiter::stopped() const {
        return m_stopped.load(std::memory_order_acquire);
    }

    TimeManager::TimeManager(
        Instant start,
        f64 remaining,
        f64 increment,
        [[maybe_unused]] i32 toGo,
        f64 overhead,
        u32 gamePly,
        f64& originalTimeAdjust
    ) :
            m_startTime{start} {
        assert(toGo >= 0);

        const auto centiMtg = remaining < 1.0 ? static_cast<u32>(remaining * 5051.0) : 5051;
        const auto timeLeft =
            std::max(0.001, remaining + (increment * (centiMtg - 100) - overhead * (centiMtg + 200)) / 100.0);

        if (originalTimeAdjust < 0.0) {
            originalTimeAdjust = 0.3128 * std::log10(timeLeft * 1000.0) - 0.4354;
        }

        const auto logTime = std::log10(remaining);

        const auto optConstant = std::min(0.0032116 + 0.000321123 * logTime, 0.00508017);
        const auto maxConstant = std::max(3.3977 + 3.03950 * logTime, 2.94761);

        m_softTime = timeLeft
                   * std::min(
                         0.0121431 + std::pow(static_cast<f64>(gamePly) + 2.94693, 0.461073) * optConstant,
                         0.213035 * remaining / timeLeft
                   );

        m_maxTime = std::min(
                        0.825179 * remaining - overhead,
                        std::min(6.67704, maxConstant + static_cast<f64>(gamePly) / 11.9847) * m_softTime
                    )
                  - 0.01;
    }

    void TimeManager::update(f64 scale) {
        m_scale = scale;
    }

    void TimeManager::stopEarly() {
        // Clamp max search time to 500ms with one legal move or in TB draws
        // Worth no elo, exists for TCEC viewer experience
        // Note that instamoving with one legal move without searching at all produces weird scores
        println("info string Search signalled early stop, capping search time to 500ms");
        m_maxTime = std::min(m_maxTime, 0.5);
    }

    bool TimeManager::stop(const search::SearchData& data, bool allowSoftTimeout) {
        if (data.nodes == 0 || (!allowSoftTimeout && (data.nodes % 1024) != 0)) {
            return false;
        }

        const auto elapsed = m_startTime.elapsed();

        if (elapsed > m_maxTime || (allowSoftTimeout && elapsed > m_softTime * m_scale)) {
            m_stopped.store(true, std::memory_order_release);
            return true;
        }

        return false;
    }

    bool TimeManager::stopped() const {
        return m_stopped.load(std::memory_order_acquire);
    }
} // namespace stormphrax::limit
