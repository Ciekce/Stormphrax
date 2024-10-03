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

#include <atomic>

#include "limit.h"
#include "../opts.h"

namespace stormphrax::limit
{
	class InfiniteLimiter final : public ISearchLimiter
	{
	public:
		InfiniteLimiter() = default;
		~InfiniteLimiter() final = default;

		[[nodiscard]] inline auto stop(const search::SearchData &data, bool allowSoftTimeout) -> bool final
		{
			return false;
		}

		[[nodiscard]] inline auto stopped() const -> bool final
		{
			return false;
		}
	};

	constexpr auto SoftNodeHardLimitMultiplierRange = util::Range<i32>{1,  5000};

	class NodeLimiter final : public ISearchLimiter
	{
	public:
		explicit NodeLimiter(usize maxNodes) : m_maxNodes{maxNodes} {}

		~NodeLimiter() final = default;

		[[nodiscard]] inline auto stop(const search::SearchData &data, bool allowSoftTimeout) -> bool final
		{
			// if softLimit is enabled:
			//   - soft limit: m_maxNodes
			//   - hard limit: m_maxNodes * softNodeHardLimitMultiplier
			// otherwise:
			//   - no soft limit
			//   - hard limit: m_maxNodes

			const auto hardLimit = m_maxNodes
				* (g_opts.softNodes ? g_opts.softNodeHardLimitMultiplier : 1);

			if (data.nodes >= hardLimit || (g_opts.softNodes && allowSoftTimeout && data.nodes >= m_maxNodes))
			{
				m_stopped.store(true, std::memory_order_release);
				return true;
			}

			return false;
		}

		[[nodiscard]] inline auto stopped() const -> bool final
		{
			return m_stopped.load(std::memory_order_acquire);
		}

	private:
		usize m_maxNodes; // hard limit when softNodes is disabled, soft limit when enabled
		std::atomic_bool m_stopped{false};
	};
}
