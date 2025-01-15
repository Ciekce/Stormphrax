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

#include "time.h"

#include <algorithm>

#include "../tunable.h"

namespace stormphrax::limit
{
	using namespace stormphrax::tunable;

	using util::Instant;

	MoveTimeLimiter::MoveTimeLimiter(i64 time, i64 overhead)
		: m_endTime{Instant::now() + static_cast<f64>(std::max<i64>(1, time - overhead)) / 1000.0} {}

	auto MoveTimeLimiter::stop(const search::SearchData &data, bool allowSoftTimeout) -> bool
	{
		if (data.rootDepth > 2
			&& data.nodes > 0
			&& (data.nodes % 1024) == 0
			&& Instant::now() >= m_endTime)
		{
			m_stopped.store(true, std::memory_order_release);
			return true;
		}

		return false;
	}

	auto MoveTimeLimiter::stopped() const -> bool
	{
		return m_stopped.load(std::memory_order_acquire);
	}

	TimeManager::TimeManager(Instant start, f64 remaining, f64 increment, i32 toGo, f64 overhead, u32 moveNumber)
		: m_startTime{start}
	{
		assert(toGo >= 0);

		const auto limit = std::max(0.001, remaining - overhead);

		if (toGo == 0)
			toGo = defaultMovesToGo();

		const auto baseTime = limit / static_cast<f64>(toGo) + increment * incrementScale();

		m_maxTime  = limit * hardTimeScale();
		m_softTime = std::min(baseTime * softTimeScale(), m_maxTime);

		assert(moveNumber >= 1);
		m_scale = 0.95 + 1.0 / static_cast<f64>(moveNumber);
	}

	auto TimeManager::update(const search::SearchData &data, Score score, Move bestMove, usize totalNodes) -> void
	{
		assert(bestMove != NullMove);
		assert(totalNodes > 0);

		if (bestMove == m_prevBestMove)
			++m_stability;
		else
		{
			m_stability = 1;
			m_prevBestMove = bestMove;
		}

		auto scale = 1.0;

		const auto bestMoveNodeFraction = static_cast<f64>(m_moveNodeCounts[bestMove.srcIdx()][bestMove.dstIdx()])
			/ static_cast<f64>(totalNodes);
		scale *= std::max(nodeTmBase() - bestMoveNodeFraction * nodeTmScale(), nodeTmScaleMin());

		if (data.rootDepth >= 6)
		{
			const auto stability = static_cast<f64>(m_stability);
			scale *= std::min(
				bmStabilityTmMax(),
				bmStabilityTmMin() + bmStabilityTmScale()
					* std::pow(stability + bmStabilityTmOffset(), bmStabilityTmPower())
			);
		}

		if (m_avgScore)
		{
			const auto avgScore = *m_avgScore;

			const auto scoreChange = static_cast<f64>(score - avgScore) / scoreTrendTmScoreScale();
			const auto invScale = scoreChange * scoreTrendTmScale() / (std::abs(scoreChange) + scoreTrendTmStretch())
				* (scoreChange > 0 ? scoreTrendTmPositiveScale() : scoreTrendTmNegativeScale());

			scale *= std::clamp(1.0 - invScale, scoreTrendTmMin(), scoreTrendTmMax());

			m_avgScore = util::ilerp<8>(avgScore, score, 1);
		}
		else m_avgScore = score;

		m_scale = std::max(scale, timeScaleMin());
	}

	auto TimeManager::updateMoveNodes(Move move, usize nodes) -> void
	{
		assert(move != NullMove);
		m_moveNodeCounts[move.srcIdx()][move.dstIdx()] += nodes;
	}

	auto TimeManager::stop(const search::SearchData &data, bool allowSoftTimeout) -> bool
	{
		if (data.nodes == 0
			|| (!allowSoftTimeout && (data.nodes % 1024) != 0))
			return false;

		const auto elapsed = m_startTime.elapsed();

		if (elapsed > m_maxTime || (allowSoftTimeout && elapsed > m_softTime * m_scale))
		{
			m_stopped.store(true, std::memory_order_release);
			return true;
		}

		return false;
	}

	auto TimeManager::stopped() const -> bool
	{
		return m_stopped.load(std::memory_order_acquire);
	}
}
