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

#pragma once

#include "../types.h"

#include <array>
#include <atomic>

#include "limit.h"
#include "../util/timer.h"
#include "../util/range.h"

namespace stormphrax::limit
{
	constexpr i32 DefaultMoveOverhead = 10;
	constexpr util::Range<i32> MoveOverheadRange{0, 50000};

	class MoveTimeLimiter final : public ISearchLimiter
	{
	public:
		explicit MoveTimeLimiter(i64 time, i64 overhead = 0);
		~MoveTimeLimiter() final = default;

		[[nodiscard]] auto stop(const search::SearchData &data, bool allowSoftTimeout) -> bool final;

		[[nodiscard]] auto stopped() const -> bool final;

	private:
		f64 m_maxTime;
		std::atomic_bool m_stopped{false};
	};

	class TimeManager final : public ISearchLimiter
	{
	public:
		TimeManager(f64 start, f64 remaining, f64 increment, i32 toGo, f64 overhead);
		~TimeManager() final = default;

		auto update(const search::SearchData &data, Move bestMove, usize totalNodes) -> void final;
		auto updateMoveNodes(Move move, usize nodes) -> void final;

		[[nodiscard]] auto stop(const search::SearchData &data, bool allowSoftTimeout) -> bool final;

		[[nodiscard]] auto stopped() const -> bool final;

	private:
		f64 m_startTime;

		f64 m_softTime{};
		f64 m_maxTime{};

		f64 m_scale{1.0};

		bool m_firstMove{true};

		std::array<std::array<usize, 64>, 64> m_moveNodeCounts{};

		std::atomic_bool m_stopped{false};
	};
}
