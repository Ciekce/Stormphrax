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

	class NodeLimiter final : public ISearchLimiter
	{
	public:
		explicit NodeLimiter(usize maxNodes) : m_maxNodes{maxNodes} {}

		~NodeLimiter() final = default;

		[[nodiscard]] inline auto stop(const search::SearchData &data, bool allowSoftTimeout) -> bool final
		{
			if (data.nodes >= m_maxNodes)
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
		usize m_maxNodes;
		std::atomic_bool m_stopped{false};
	};
}
