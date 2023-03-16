/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#include "time.h"

#include <algorithm>
#include <iostream>

namespace polaris::limit
{
	MoveTimeLimiter::MoveTimeLimiter(i64 time, i64 overhead)
		: m_maxTime{util::g_timer.time() + static_cast<f64>(std::max(I64(1), time - overhead)) / 1000.0} {}

	bool MoveTimeLimiter::stop(const search::SearchData &data) const
	{
		return data.depth > 2
			&& data.nodes > 0
			&& (data.nodes % 1024) == 0
			&& util::g_timer.time() >= m_maxTime;
	}

	TimeManager::TimeManager(f64 start, f64 remaining, f64 increment, i32 toGo, f64 overhead)
		: m_startTime{start}
	{
	//	std::cout << "info string remaining " << remaining << " increment " << increment
	//		<< " toGo " << toGo << " overhead " << overhead << std::endl;

		const auto limit = std::max(0.001, remaining - overhead);

		if (toGo == 0)
			toGo = 35;

		if (toGo < 15)
			toGo = 15;

		m_time = limit / static_cast<f64>(toGo) + increment;

		if (m_time > limit)
			m_time = limit;

	//	std::cout << "info string allocated " << (m_time * 1000.0)
	//		<< " ms, max " << (m_maxTime * 1000.0) << " ms" << std::endl;
	}

	void TimeManager::update(const search::SearchData &data, bool stableBestMove)
	{
		if (data.depth < 4)
			return;

		//TODO allocate more time for unstable best moves
	}

	bool TimeManager::stop(const search::SearchData &data) const
	{
		if (data.nodes == 0 || (data.nodes % 1024) != 0)
			return false;

		if (data.depth < 5)
			return false;

		const auto elapsed = util::g_timer.time() - m_startTime;
		return elapsed > m_time;
	}
}