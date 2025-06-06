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

#include <algorithm>

namespace stormphrax::util
{
	template<typename T>
	class Range
	{
	public:
		constexpr Range(T min, T max)
			: m_min{min}, m_max{max} {}

		[[nodiscard]] constexpr auto contains(T v) const
		{
			return m_min <= v && v <= m_max;
		}

		[[nodiscard]] constexpr auto clamp(T v) const
		{
			return std::clamp(v, m_min, m_max);
		}

		[[nodiscard]] constexpr auto min() const { return m_min; }
		[[nodiscard]] constexpr auto max() const { return m_max; }

	private:
		T m_min;
		T m_max;
	};
}
