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

#include "timer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace polaris::util
{
	Timer::Timer()
	{
		u64 freq{};
		QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER *>(&freq));

		m_frequency = static_cast<f64>(freq);

		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_initTime));
	}

	auto Timer::time() const -> f64
	{
		u64 time{};
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&time));

		return static_cast<f64>(time - m_initTime) / m_frequency;
	}

	auto Timer::roughTimeMs() -> i64
	{
		return static_cast<i64>(GetTickCount64());
	}
}
#else // assume posix
#include <time.h>

namespace polaris::util
{
	Timer::Timer()
	{
		struct timespec time;
		clock_gettime(CLOCK_MONOTONIC, &time);

		m_initTime = static_cast<f64>(time.tv_sec) + static_cast<f64>(time.tv_nsec) / 1000000000.0;
	}

	auto Timer::time() const -> f64
	{
		struct timespec time{};
		clock_gettime(CLOCK_MONOTONIC, &time);

		return (static_cast<f64>(time.tv_sec) + static_cast<f64>(time.tv_nsec) / 1000000000.0) - m_initTime;
	}
}
#endif
