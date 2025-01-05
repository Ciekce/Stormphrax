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

#include "timer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else // posix
#include <time.h>
#endif

namespace stormphrax::util
{
	namespace
	{
		class Timer
		{
		public:
			Timer();
			~Timer() = default;

			[[nodiscard]] auto time() const -> f64;

		private:
#ifdef _WIN32
			u64 m_initTime{};
			f64 m_frequency;
#else
			f64 m_initTime;
#endif
		};

#ifdef _WIN32
		Timer::Timer()
		{
			LARGE_INTEGER freq{};
			QueryPerformanceFrequency(&freq);

			LARGE_INTEGER initTime{};
			QueryPerformanceCounter(&initTime);

			m_frequency = static_cast<f64>(freq.QuadPart);
			m_initTime = initTime.QuadPart;
		}

		auto Timer::time() const -> f64
		{
			LARGE_INTEGER time{};
			QueryPerformanceCounter(&time);

			return static_cast<f64>(time.QuadPart - m_initTime) / m_frequency;
		}
#else
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
#endif

		const Timer s_timer{};
	}

	auto Instant::elapsed() const -> f64
	{
		return s_timer.time() - m_time;
	}

	auto Instant::now() -> Instant
	{
		return Instant{s_timer.time()};
	}
}
