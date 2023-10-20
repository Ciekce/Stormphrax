/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

namespace stormphrax::limit
{
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

		const auto limit = std::max(0.001, remaining - overhead);

		if (toGo == 0)
			toGo = 20;

		const auto baseTime = limit / static_cast<f64>(toGo) + increment * 0.75;

		m_maxTime  = limit / 2.0;
		m_softTime = std::min(baseTime * 0.6, m_maxTime);
	}

	auto TimeManager::update(const search::SearchData &data, Move bestMove, usize totalNodes) -> void
	{
		assert(bestMove != NullMove);
		assert(totalNodes > 0);

		const auto bestMoveFraction = static_cast<f64>(m_moveNodeCounts[bestMove.srcIdx()][bestMove.dstIdx()])
			/ static_cast<f64>(totalNodes);
		const auto moveNodeScale = (1.5 - bestMoveFraction) * 1.35;

		m_scale = moveNodeScale;
	}

	auto TimeManager::updateMoveNodes(Move move, usize nodes) -> void
	{
		assert(move != NullMove);
		m_moveNodeCounts[move.srcIdx()][move.dstIdx()] += nodes;
	}

	auto TimeManager::stop(const search::SearchData &data, bool allowSoftTimeout) -> bool
	{
		if (data.depth < 5
			|| data.nodes == 0
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
