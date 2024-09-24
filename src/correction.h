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
			std::memset(&m_pawnTable, 0, sizeof(m_pawnTable));
			std::memset(&m_majorTable, 0, sizeof(m_majorTable));
			std::memset(&m_minorTable, 0, sizeof(m_minorTable));
		}

		inline auto update(const Position &pos, i32 depth, Score searchScore, Score staticEval)
		{
			const auto scaledError = (searchScore - staticEval) * Grain;
			const auto newWeight = std::min(depth + 1, 16);

			m_pawnTable[static_cast<i32>(pos.toMove())][pos.pawnKey() % Entries].update(scaledError, newWeight);
			m_majorTable[static_cast<i32>(pos.toMove())][pos.majorKey() % Entries].update(scaledError, newWeight);
			m_minorTable[static_cast<i32>(pos.toMove())][pos.minorKey() % Entries].update(scaledError, newWeight);
		}

		[[nodiscard]] inline auto correct(const Position &pos, Score score) const
		{
			score = m_pawnTable[static_cast<i32>(pos.toMove())][pos.pawnKey() % Entries].correct(score);
			score = m_majorTable[static_cast<i32>(pos.toMove())][pos.majorKey() % Entries].correct(score);
			score = m_minorTable[static_cast<i32>(pos.toMove())][pos.minorKey() % Entries].correct(score);

			return std::clamp(score, -ScoreWin + 1, ScoreWin - 1);
		}

	private:
		static constexpr usize Entries = 16384;

		static constexpr i32 Grain = 256;
		static constexpr i32 WeightScale = 256;
		static constexpr i32 Max = Grain * 32;

		struct Entry
		{
			i32 value{};

			inline auto update(i32 scaledError, i32 newWeight) -> void
			{
				value = util::ilerp<WeightScale>(value, scaledError, newWeight);
				value = std::clamp(value, -Max, Max);
			}

			[[nodiscard]] inline auto correct(Score score) const -> Score
			{
				return score + value / Grain;
			}
		};

		util::MultiArray<Entry, 2, Entries> m_pawnTable{};
		util::MultiArray<Entry, 2, Entries> m_majorTable{};
		util::MultiArray<Entry, 2, Entries> m_minorTable{};
	};
}
