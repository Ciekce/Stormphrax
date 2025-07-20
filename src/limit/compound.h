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

#include <memory>
#include <vector>

#include "limit.h"

namespace stormphrax::limit {
    class CompoundLimiter final : public ISearchLimiter {
    public:
        ~CompoundLimiter() final = default;

        template <typename T, typename... Args>
        inline void addLimiter(Args&&... args) {
            m_limiters.push_back(std::make_unique<T>(std::forward<Args>(args)...));
        }

        inline void update(const search::SearchData& data, Score score, Move bestMove, usize totalNodes) final {
            for (const auto& limiter : m_limiters) {
                limiter->update(data, score, bestMove, totalNodes);
            }
        }

        inline void updateMoveNodes(Move move, usize nodes) final {
            for (const auto& limiter : m_limiters) {
                limiter->updateMoveNodes(move, nodes);
            }
        }

        inline void stopEarly() final {
            for (const auto& limiter : m_limiters) {
                limiter->stopEarly();
            }
        }

        [[nodiscard]] inline bool stop(const search::SearchData& data, bool allowSoftTimeout) final {
            return std::ranges::any_of(m_limiters, [&](const auto& limiter) {
                return limiter->stop(data, allowSoftTimeout);
            });
        }

        [[nodiscard]] inline bool stopped() const final {
            return std::ranges::any_of(m_limiters, [&](const auto& limiter) { return limiter->stopped(); });
        }

    private:
        std::vector<std::unique_ptr<ISearchLimiter>> m_limiters{};
    };
} // namespace stormphrax::limit
