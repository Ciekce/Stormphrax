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

#include "types.h"

#include <algorithm>
#include <cstring>

#include "core.h"
#include "position/position.h"
#include "util/multi_array.h"
#include "util/cemath.h"

namespace stormphrax
{
	class CorrectionHistoryTable
	{
	public:
		CorrectionHistoryTable() = default;
		~CorrectionHistoryTable() = default;

		inline auto clear()
		{
			std::memset(&m_table, 0, sizeof(m_table));
		}

		inline auto update(const Position &pos, i32 depth, Score searchScore, Score staticEval)
		{
			m_table[static_cast<i32>(pos.toMove())][pos.pawnKey() % Entries].update(depth, searchScore, staticEval);
		}

		[[nodiscard]] inline auto correct(const Position &pos, Score score) const
		{
			return m_table[static_cast<i32>(pos.toMove())][pos.pawnKey() % Entries].correct(score);
		}

	private:
		static constexpr usize Entries = 16384;

		static constexpr i32 Grain = 256;
		static constexpr i32 WeightScale = 256;
		static constexpr i32 Max = Grain * 32;

		struct Entry
		{
			i32 value{};

			inline auto update(i32 depth, Score searchScore, Score staticEval) -> void
			{
				const auto scaledError = (searchScore - staticEval) * Grain;
				const auto newWeight = std::min(depth + 1, 16);

				value = util::ilerp<WeightScale>(value, scaledError, newWeight);
				value = std::clamp(value, -Max, Max);
			}

			[[nodiscard]] inline auto correct(Score score) const -> Score
			{
				return score + value / Grain;
			}
		};

		util::MultiArray<Entry, 2, Entries> m_table{};
	};
}
