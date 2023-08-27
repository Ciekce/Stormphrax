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

#pragma once

#include "../types.h"

#include <limits>
#include <atomic>
#include <cassert>

namespace stormphrax::util
{
	// This is a stripped-down reimplementation of MSVC's <barrier>,
	// because SP has to build on machines that do not yet have the header
	class Barrier
	{
	public:
		explicit Barrier(i64 expected)
		{
			reset(expected);
		}

		~Barrier() = default;

		inline auto reset(i64 expected) -> void
		{
			assert(expected <= Max);

			const auto value = expected << ValueShift;

			m_total.store(value);
			m_current.store(value);
		}

		auto arriveAndWait()
		{
			auto current = m_current.fetch_sub(ValueStep, std::memory_order::acq_rel) - ValueStep;

			if ((current & ValueMask) == 0)
			{
				const auto remCount = m_total.load();
				const auto newPhaseCount = remCount | ((current + 1) & ArrivalTokenMask);

				m_current.store(newPhaseCount, std::memory_order::release);
				m_current.notify_all();

				return;
			}

			const auto arrival = current & ArrivalTokenMask;

			while (true)
			{
				m_current.wait(current, std::memory_order::relaxed);
				current = m_current.load(std::memory_order::acquire);

				if ((current & ArrivalTokenMask) != arrival)
					return;
			}
		}

	private:
		static inline constexpr i64 ArrivalTokenMask = 1;
		static inline constexpr i64 ValueMask = ~ArrivalTokenMask;
		static inline constexpr i64 ValueShift = 1;
		static inline constexpr i64 ValueStep = U64(1) << ValueShift;
		static inline constexpr i64 Max = std::numeric_limits<i64>::max() >> ValueShift;

		std::atomic<i64> m_total{};
		std::atomic<i64> m_current{};
	};
}
