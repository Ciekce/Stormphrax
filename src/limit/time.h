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

#pragma once

#include "../types.h"

#include "limit.h"
#include "../util/timer.h"
#include "../util/range.h"

namespace polaris::limit
{
	constexpr i32 DefaultMoveOverhead = 100;
	constexpr util::Range<i32> MoveOverheadRange{0, 50000};

	class MoveTimeLimiter final : public ISearchLimiter
	{
	public:
		explicit MoveTimeLimiter(i64 time, i64 overhead = 0);
		~MoveTimeLimiter() final = default;

		[[nodiscard]] bool stop(const search::SearchData &data) const final;

	private:
		f64 m_maxTime;
	};

	class TimeManager final : public ISearchLimiter
	{
	public:
		TimeManager(f64 start, f64 remaining, f64 increment, i32 toGo, f64 overhead);
		~TimeManager() final = default;

		void update(const search::SearchData &data, bool stableBestMove) final;

		[[nodiscard]] bool stop(const search::SearchData &data) const final;

	private:
		f64 m_startTime;

		f64 m_time{};
	};
}
