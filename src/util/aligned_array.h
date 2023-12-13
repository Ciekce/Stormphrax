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

namespace stormphrax::util
{
	template <usize Alignment, typename T, usize Count>
	class AlignedArray
	{
	public:
		[[nodiscard]] constexpr auto at(usize idx) -> auto &
		{
			return m_array.at(idx);
		}

		[[nodiscard]] constexpr auto at(usize idx) const -> const auto &
		{
			return m_array.at(idx);
		}

		[[nodiscard]] constexpr auto operator[](usize idx) -> auto &
		{
			return m_array[idx];
		}

		[[nodiscard]] constexpr auto operator[](usize idx) const -> const auto &
		{
			return m_array[idx];
		}

		[[nodiscard]] constexpr auto front() -> auto &
		{
			return m_array.front();
		}

		[[nodiscard]] constexpr auto front() const -> const auto &
		{
			return m_array.front();
		}

		[[nodiscard]] constexpr auto back() -> auto &
		{
			return m_array.back();
		}

		[[nodiscard]] constexpr auto back() const -> const auto &
		{
			return m_array.back();
		}

		[[nodiscard]] constexpr auto data()
		{
			return m_array.data();
		}

		[[nodiscard]] constexpr auto data() const
		{
			return m_array.data();
		}

		[[nodiscard]] constexpr auto begin()
		{
			return m_array.begin();
		}

		[[nodiscard]] constexpr auto begin() const
		{
			return m_array.begin();
		}

		[[nodiscard]] constexpr auto cbegin() const
		{
			return m_array.cbegin();
		}

		[[nodiscard]] constexpr auto end()
		{
			return m_array.end();
		}

		[[nodiscard]] constexpr auto end() const
		{
			return m_array.end();
		}

		[[nodiscard]] constexpr auto cend() const
		{
			return m_array.cend();
		}

		[[nodiscard]] constexpr auto rbegin()
		{
			return m_array.rbegin();
		}

		[[nodiscard]] constexpr auto rbegin() const
		{
			return m_array.rbegin();
		}

		[[nodiscard]] constexpr auto crbegin() const
		{
			return m_array.crbegin();
		}

		[[nodiscard]] constexpr auto rend()
		{
			return m_array.rend();
		}

		[[nodiscard]] constexpr auto rend() const
		{
			return m_array.rend();
		}

		[[nodiscard]] constexpr auto crend() const
		{
			return m_array.crend();
		}

		[[nodiscard]] constexpr auto empty() const
		{
			return m_array.empty();
		}

		[[nodiscard]] constexpr auto size() const
		{
			return m_array.size();
		}

		[[nodiscard]] constexpr auto max_size() const
		{
			return m_array.max_size();
		}

		constexpr auto fill(const T &value)
		{
			m_array.fill(value);
		}

		constexpr auto swap(AlignedArray &other)
		{
			m_array.swap(other.m_array);
		}

		[[nodiscard]] constexpr auto array() -> auto &
		{
			return m_array;
		}

		[[nodiscard]] constexpr auto array() const -> const auto &
		{
			return m_array;
		}

		constexpr operator std::array<T, Count> &()
		{
			return m_array;
		}

		constexpr operator const std::array<T, Count> &() const
		{
			return m_array;
		}

	private:
		alignas(Alignment) std::array<T, Count> m_array;
	};

	template <usize Alignment, typename T, usize Count>
	auto swap(AlignedArray<Alignment, T, Count> &a, AlignedArray<Alignment, T, Count> &b)
	{
		a.swap(b);
	}
}
