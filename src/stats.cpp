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

#include "stats.h"

#include <iostream>
#include <atomic>

#include "util/multi_array.h"

namespace stormphrax::stats
{
	namespace
	{
		constexpr usize Slots = 32;

		struct Range
		{
			std::atomic<i64> min{std::numeric_limits<i64>::max()};
			std::atomic<i64> max{std::numeric_limits<i64>::min()};
		};

		util::MultiArray<std::atomic<u64>, Slots, 2> s_conditionHits{};
		util::MultiArray<Range, Slots> s_ranges{};

		std::atomic_bool s_anyUsed{false};

		template <typename T>
		inline auto atomicMin(std::atomic<T> &v, T x)
		{
			auto curr = v.load();
			while (x < curr && v.compare_exchange_weak(curr, x)) {}
		}

		template <typename T>
		inline auto atomicMax(std::atomic<T> &v, T x)
		{
			auto curr = v.load();
			while (x > curr && v.compare_exchange_weak(curr, x)) {}
		}
	}

	auto conditionHit(bool condition, usize slot) -> void
	{
		if (slot >= Slots)
		{
			std::cerr << "tried to hit condition " << slot << " (max " << (Slots - 1) << ")" << std::endl;
			return;
		}

		++s_conditionHits[slot][condition];

		s_anyUsed = true;
	}

	auto range(i64 v, usize slot) -> void
	{
		if (slot >= Slots)
		{
			std::cerr << "tried to hit range " << slot << " (max " << (Slots - 1) << ")" << std::endl;
			return;
		}

		atomicMin(s_ranges[slot].min, v);
		atomicMax(s_ranges[slot].max, v);

		s_anyUsed = true;
	}

	auto print() -> void
	{
		if (!s_anyUsed.load())
			return;

		for (usize slot = 0; slot < Slots; ++slot)
		{
			const auto hits = s_conditionHits[slot][1].load();
			const auto misses = s_conditionHits[slot][0].load();

			if (hits == 0 && misses == 0)
				continue;

			const auto hitrate = static_cast<f64>(hits) / static_cast<f64>(hits + misses);

			std::cout << "condition " << slot << ":\n";
			std::cout << "    hits: " << hits << "\n";
			std::cout << "    misses: " << misses << "\n";
			std::cout << "    hitrate: " << (hitrate * 100) << "%" << std::endl;
		}

		for (usize slot = 0; slot < Slots; ++slot)
		{
			const auto min = s_ranges[slot].min.load();
			const auto max = s_ranges[slot].max.load();

			if (min == std::numeric_limits<i64>::max())
				continue;

			std::cout << "range " << slot << ":\n";
			std::cout << "    min: " << min << "\n";
			std::cout << "    max: " << max << std::endl;
		}
	}
}
