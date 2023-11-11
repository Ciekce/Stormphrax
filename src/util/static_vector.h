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

#include <array>
#include <cstddef>
#include <algorithm>
#include <cassert>

namespace stormphrax
{
	template <typename T, usize Capacity>
	class StaticVector
	{
	public:
		StaticVector() = default;
		~StaticVector() = default;

		StaticVector(const StaticVector<T, Capacity> &other)
		{
			*this = other;
		}

		inline auto push(const T &elem)
		{
			assert(m_size < Capacity);
			m_data[m_size++] = elem;
		}

		inline auto push(T &&elem)
		{
			assert(m_size < Capacity);
			m_data[m_size++] = std::move(elem);
		}

		inline auto clear() { m_size = 0; }

		inline auto fill(const T &v) { m_data.fill(v); }

		[[nodiscard]] inline auto size() const { return m_size; }

		[[nodiscard]] inline auto empty() const { return m_size == 0; }

		[[nodiscard]] inline auto operator[](usize i) const -> const auto &
		{
			assert(i < m_size);
			return m_data[i];
		}

		[[nodiscard]] inline auto begin() { return m_data.begin(); }
		[[nodiscard]] inline auto end() { return m_data.begin() + static_cast<std::ptrdiff_t>(m_size); }

		[[nodiscard]] inline auto operator[](usize i) -> auto &
		{
			assert(i < m_size);
			return m_data[i];
		}

		[[nodiscard]] inline auto begin() const { return m_data.begin(); }
		[[nodiscard]] inline auto end() const { return m_data.begin() + static_cast<std::ptrdiff_t>(m_size); }

		inline auto resize(usize size)
		{
			assert(size <= Capacity);
			m_size = size;
		}

		inline auto operator=(const StaticVector<T, Capacity> &other) -> auto &
		{
			std::copy(other.begin(), other.end(), begin());
			m_size = other.m_size;
			return *this;
		}

	private:
		std::array<T, Capacity> m_data{};
		usize m_size{0};
	};
}
