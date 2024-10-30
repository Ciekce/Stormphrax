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
#include <cmath>
#include <numbers>

#include "../tunable.h"

namespace stormphrax::limit
{
	using namespace stormphrax::tunable;

	MoveTimeLimiter::MoveTimeLimiter(i64 time, i64 overhead)
		: m_maxTime{util::g_timer.time() + static_cast<f64>(std::max(I64(1), time - overhead)) / 1000.0} {}

	auto MoveTimeLimiter::stop(const search::SearchData &data, bool allowSoftTimeout) -> bool
	{
		if (data.depth > 2
			&& data.nodes > 0
			&& (data.nodes % 1024) == 0
			&& util::g_timer.time() >= m_maxTime)
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

	TimeManager::TimeManager(f64 start, f64 remaining, f64 increment, i32 toGo, f64 overhead)
		: m_startTime{start}
	{
		assert(toGo >= 0);

		const auto incScale = static_cast<f64>(incrementScale()) / 100.0;

		const auto softScale = static_cast<f64>(softTimeScale()) / 100.0;
		const auto hardScale = static_cast<f64>(hardTimeScale()) / 100.0;

		const auto limit = std::max(0.001, remaining - overhead);

		if (toGo == 0)
			toGo = defaultMovesToGo();

		const auto baseTime = limit / static_cast<f64>(toGo) + increment * incScale;

		m_maxTime  = limit * hardScale;
		m_softTime = std::min(baseTime * softScale, m_maxTime);
	}

	auto TimeManager::update(const search::SearchData &data, Score score, Move bestMove, usize totalNodes) -> void
	{
		assert(bestMove != NullMove);
		assert(totalNodes > 0);

		const auto nodeBase = static_cast<f64>(nodeTmBase()) / 100.0;
		const auto nodeScale = static_cast<f64>(nodeTmScale()) / 100.0;
		const auto nodeMin = static_cast<f64>(nodeTmScaleMin()) / 1000.0;

		const auto bmStabilityMin = static_cast<f64>(bmStabilityTmMin()) / 100.0;
		const auto bmStabilityMax = static_cast<f64>(bmStabilityTmMax()) / 100.0;
		const auto bmStabilityScale = static_cast<f64>(bmStabilityTmScale()) / 100.0;
		const auto bmStabilityOffset = static_cast<f64>(bmStabilityTmOffset()) / 100.0;
		const auto bmStabilityPower = static_cast<f64>(bmStabilityTmPower()) / 100.0;

		const auto stMin = static_cast<f64>(scoreTrendTmMin()) / 100.0;
		const auto stMax = static_cast<f64>(scoreTrendTmMax()) / 100.0;
		const auto stScoreScale = static_cast<f64>(scoreTrendTmScoreScale()) / 10.0;
		const auto stStretch = static_cast<f64>(scoreTrendTmStretch()) / 100.0;
		const auto stScale = static_cast<f64>(scoreTrendTmScale()) / 100.0;
		const auto stPositiveScale = static_cast<f64>(scoreTrendTmPositiveScale()) / 100.0;
		const auto stNegativeScale = static_cast<f64>(scoreTrendTmNegativeScale()) / 100.0;

		const auto minScale = static_cast<f64>(timeScaleMin()) / 1000.0;

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
		scale *= std::max(nodeBase - bestMoveNodeFraction * nodeScale, nodeMin);

		if (data.depth >= 6)
		{
			const auto stability = static_cast<f64>(m_stability);
			scale *= std::min(
				bmStabilityMax,
				bmStabilityMin + bmStabilityScale * std::pow(stability + bmStabilityOffset, bmStabilityPower)
			);
		}

		if (m_avgScore)
		{
			const auto avgScore = *m_avgScore;

			const auto scoreChange = static_cast<f64>(score - avgScore) / stScoreScale;
			const auto invScale = scoreChange * stScale / (std::abs(scoreChange) + stStretch)
				* (scoreChange > 0 ? stPositiveScale : stNegativeScale);

			scale *= std::clamp(1.0 - invScale, stMin, stMax);

			m_avgScore = util::ilerp<8>(avgScore, score, 1);
		}
		else m_avgScore = score;

		static constexpr f64 Stretch = 0.06;
		static constexpr f64 Scale = 0.067;

		const auto scaledScore = std::abs(score) / 260.0;
		const auto clamped = std::clamp(scaledScore, 1.0 - Stretch * std::numbers::pi, 1.0 + Stretch * std::numbers::pi);

		m_scale *= 1.0 + Scale / 2.2 + Scale * std::cos((1.0 - clamped) / Stretch);

		m_scale = std::max(scale, minScale);
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

		const auto elapsed = util::g_timer.time() - m_startTime;

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
