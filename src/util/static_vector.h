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

#include <array>
#include <cstddef>
#include <algorithm>

#include <iostream>

namespace polaris
{
	class Move;

	template <typename T, size_t Capacity>
	class StaticVector
	{
	public:
		StaticVector() = default;
		~StaticVector() = default;

		inline void push(const T &elem) { m_data[m_size++] = elem; }
		inline void push(T &&elem) { m_data[m_size++] = std::move(elem); }

		inline void clear() { m_size = 0; }

		inline void fill(const T &v)
		{
			// horrible dirty hack
#ifdef NDEBUG
			if constexpr (std::is_same_v<T, Move>)
			{
				auto *data = reinterpret_cast<u16 *>(m_data.data());
				std::fill(data, data + Capacity, v.data());
				return;
			}
#endif

			m_data.fill(v);
		}

		[[nodiscard]] inline auto size() const { return m_size; }

		[[nodiscard]] inline auto empty() const { return m_size == 0; }

		[[nodiscard]] inline auto operator[](size_t i) const { return m_data[i]; }

		[[nodiscard]] inline auto begin() { return m_data.begin(); }
		[[nodiscard]] inline auto end() { return m_data.begin() + static_cast<std::ptrdiff_t>(m_size); }

		[[nodiscard]] inline auto &operator[](size_t i) { return m_data[i]; }

		[[nodiscard]] inline auto begin() const { return m_data.begin(); }
		[[nodiscard]] inline auto end() const { return m_data.begin() + static_cast<std::ptrdiff_t>(m_size); }

	private:
		std::array<T, Capacity> m_data{};
		size_t m_size{0};
	};
}
