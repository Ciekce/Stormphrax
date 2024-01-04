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

#include <array>
#include <algorithm>

#include "../core.h"
#include "../util/bits.h"

namespace stormphrax::eval
{
	class ErrorHistory
	{
	public:
		inline auto update(u64 pawnKey, Score error)
		{
			auto &entry = m_data[pawnKey % Size];
			entry = (entry * (UpdateScale - 1) + error) / UpdateScale;
		}

		[[nodiscard]] inline auto get(u64 pawnKey) const
		{
			return m_data[pawnKey % Size];
		}

		inline auto clear()
		{
			std::ranges::fill(m_data, 0);
		}

	private:
		static constexpr u64 Size = 8192;
		static constexpr Score UpdateScale = 256;

		static_assert(util::isPowerOfTwo(Size));
		static_assert(UpdateScale > 0 && util::isPowerOfTwo(static_cast<u32>(UpdateScale)));

		std::array<Score, Size> m_data{};
	};
}
