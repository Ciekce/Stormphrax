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

#include <optional>

#include "root_move.h"
#include "util/timer.h"

namespace stormphrax::limit {
    constexpr u32 kDefaultMoveOverheadMs = 10;
    constexpr util::Range<i32> kMoveOverheadRange{0, 50000};

    struct TimeLimits {
        f64 remaining{};
        f64 increment{};
        std::optional<i32> movesToGo{};
    };

    class TimeManager {
    public:
        TimeManager(const TimeLimits& limits, u32 moveOverheadMs);

        void update(i32 depth, usize totalNodes, const search::RootMove& pvMove);

        [[nodiscard]] bool stopSoft(f64 time) const;
        [[nodiscard]] bool stopHard(f64 time) const;

        void stopEarly();

    private:
        f64 m_optTime;
        f64 m_maxTime;

        f64 m_scale{1.0};

        Move m_prevBestMove{};
        u32 m_stability{};

        std::optional<Score> m_avgScore{};
    };

    class SearchLimiter {
    public:
        explicit SearchLimiter(util::Instant startTime);

        bool setHardNodes(usize nodes);
        bool setSoftNodes(usize nodes);

        bool setMoveTime(f64 time);

        bool setTournamentTime(const TimeLimits& limits, u32 moveOverheadMs);

        void update(i32 depth, usize totalNodes, const search::RootMove& pvMove);

        [[nodiscard]] bool stopSoft(usize nodes) const;
        [[nodiscard]] bool stopHard(usize nodes) const;

        void stopEarly();

    private:
        util::Instant m_startTime;

        std::optional<usize> m_hardNodes;
        std::optional<usize> m_softNodes;

        std::optional<f64> m_moveTime;

        std::optional<TimeManager> m_timeManager;
    };
} // namespace stormphrax::limit
