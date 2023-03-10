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

namespace polaris::util
{
	class Timer
	{
	public:
		Timer();
		~Timer() = default;

		[[nodiscard]] f64 time() const;

		[[nodiscard]] static i64 roughTimeMs();

	private:
#ifdef _WIN32
		u64 m_initTime{};
		f64 m_frequency;
#else
		f64 m_initTime;
#endif
	};

	inline const Timer g_timer{};
}
