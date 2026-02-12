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

#include "limit.h"

#include "tunable.h"

namespace stormphrax::limit {
    namespace {
        constexpr usize kTimeCheckInterval = 1024;
    } // namespace

    using namespace stormphrax::tunable;

    TimeManager::TimeManager(const TimeLimits& limits, u32 moveOverheadMs) {
        const auto moveOverhead = static_cast<f64>(moveOverheadMs) / 1000.0;

        const auto limit = std::max(0.001, limits.remaining - moveOverhead);

        const auto movesToGo = limits.movesToGo.value_or(defaultMovesToGo());
        const auto baseTime = limit / static_cast<f64>(movesToGo) + limits.increment * incrementScale();

        m_maxTime = limit * hardTimeScale();
        m_optTime = std::min(baseTime * softTimeScale(), m_maxTime);
    }

    void TimeManager::update(i32 depth, usize totalNodes, const search::RootMove& pvMove) {
        const auto bestMove = pvMove.pv.moves[0];
        const auto score = pvMove.score;

        if (bestMove == m_prevBestMove) {
            ++m_stability;
        } else {
            m_stability = 1;
            m_prevBestMove = bestMove;
        }

        auto scale = 1.0;

        const auto bestMoveNodeFraction = static_cast<f64>(pvMove.nodes) / static_cast<f64>(totalNodes);
        scale *= std::max(nodeTmBase() - bestMoveNodeFraction * nodeTmScale(), nodeTmScaleMin());

        if (depth >= 6) {
            const auto stability = static_cast<f64>(m_stability);
            scale *= std::min(
                bmStabilityTmMax(),
                bmStabilityTmMin()
                    + bmStabilityTmScale() * std::pow(stability + bmStabilityTmOffset(), bmStabilityTmPower())
            );
        }

        if (m_avgScore) {
            const auto avgScore = *m_avgScore;

            const auto scoreChange = static_cast<f64>(score - avgScore) / scoreTrendTmScoreScale();
            const auto invScale = scoreChange * scoreTrendTmScale() / (std::abs(scoreChange) + scoreTrendTmStretch())
                                * (scoreChange > 0 ? scoreTrendTmPositiveScale() : scoreTrendTmNegativeScale());

            scale *= std::clamp(1.0 - invScale, scoreTrendTmMin(), scoreTrendTmMax());

            m_avgScore = util::ilerp<8>(avgScore, score, 1);
        } else {
            m_avgScore = score;
        }

        m_scale = std::max(scale, timeScaleMin());
    }

    bool TimeManager::stopSoft(f64 time) const {
        return time >= m_optTime * m_scale;
    }

    bool TimeManager::stopHard(f64 time) const {
        return time >= m_maxTime;
    }

    void TimeManager::stopEarly() {
        // Clamp max search time to 500ms with one legal move or in TB draws
        // Worth no elo, exists for TCEC viewer experience
        // Note that instamoving with one legal move without searching at all produces weird scores
        println("info string Search signalled early stop, capping search time to 500ms");
        m_maxTime = std::min(m_maxTime, 0.5);
    }

    SearchLimiter::SearchLimiter(util::Instant startTime) :
            m_startTime{startTime} {}

    bool SearchLimiter::setHardNodes(usize nodes) {
        if (m_hardNodes) {
            return false;
        }

        m_hardNodes = nodes;
        return true;
    }

    bool SearchLimiter::setSoftNodes(usize nodes) {
        if (m_softNodes) {
            return false;
        }

        m_softNodes = nodes;
        return true;
    }

    bool SearchLimiter::setMoveTime(f64 time) {
        if (m_moveTime) {
            return false;
        }

        m_moveTime = time;
        return true;
    }

    bool SearchLimiter::setTournamentTime(const TimeLimits& limits, u32 moveOverheadMs) {
        if (m_timeManager) {
            return false;
        }

        m_timeManager = TimeManager{limits, moveOverheadMs};
        return true;
    }

    void SearchLimiter::update(i32 depth, usize totalNodes, const search::RootMove& pvMove) {
        if (m_timeManager) {
            m_timeManager->update(depth, totalNodes, pvMove);
        }
    }

    bool SearchLimiter::stopSoft(usize nodes) const {
        if (m_softNodes && nodes >= *m_softNodes) {
            return true;
        }

        if (m_moveTime || m_timeManager) {
            const auto time = m_startTime.elapsed();

            if (m_moveTime && time >= *m_moveTime) {
                return true;
            }

            if (m_timeManager && m_timeManager->stopSoft(time)) {
                return true;
            }
        }

        return false;
    }

    bool SearchLimiter::stopHard(usize nodes) const {
        if (m_hardNodes && nodes >= *m_hardNodes) {
            return true;
        }

        if (nodes > 0 && nodes % kTimeCheckInterval == 0 && (m_moveTime || m_timeManager)) {
            const auto time = m_startTime.elapsed();

            if (m_moveTime && time >= *m_moveTime) {
                return true;
            }

            if (m_timeManager && m_timeManager->stopHard(time)) {
                return true;
            }
        }

        return false;
    }

    void SearchLimiter::stopEarly() {
        if (m_timeManager) {
            m_timeManager->stopEarly();
        }
    }
} // namespace stormphrax::limit
