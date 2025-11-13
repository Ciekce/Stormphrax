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

#include "../types.h"

#include "../search_fwd.h"

namespace stormphrax::limit {
    class ISearchLimiter {
    public:
        virtual ~ISearchLimiter() = default;

        virtual void update(
            [[maybe_unused]] const search::SearchData& data,
            [[maybe_unused]] Score score,
            [[maybe_unused]] Move bestMove,
            [[maybe_unused]] usize totalNodes
        ) {}
        virtual void updateMoveNodes([[maybe_unused]] Move move, [[maybe_unused]] usize nodes) {}

        virtual void stopEarly() {}

        [[nodiscard]] virtual bool stop(const search::SearchData& data, bool allowSoftTimeout) = 0;

        [[nodiscard]] virtual bool stopped() const = 0;
    };
} // namespace stormphrax::limit
