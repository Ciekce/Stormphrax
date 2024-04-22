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

#include <cmath>
#include <array>
#include <cstring>

#include "tunable.h"
#include "move.h"

namespace stormphrax
{
	using HistoryScore = i16;

	struct HistoryEntry
	{
		i16 value{};

		HistoryEntry() = default;
		HistoryEntry(HistoryScore v) : value{v} {}

		[[nodiscard]] inline operator HistoryScore() const
		{
			return value;
		}

		[[nodiscard]] inline auto operator=(HistoryScore v) -> auto &
		{
			value = v;
			return *this;
		}

		inline auto update(HistoryScore bonus)
		{
			value += bonus - value * std::abs(bonus) / tunable::maxHistory();
		}
	};

	inline auto historyBonus(i32 depth) -> HistoryScore
	{
		return static_cast<HistoryScore>(std::min(
			depth * tunable::historyBonusDepthScale() - tunable::historyBonusOffset(),
			tunable::maxHistoryBonus()
		));
	}

	class HistoryTables
	{
	public:
		HistoryTables() = default;
		~HistoryTables() = default;

		inline auto clear()
		{
			std::memset(&m_main , 0, sizeof(m_main ));
			std::memset(&m_noisy, 0, sizeof(m_noisy));
		}

		inline auto updateQuietScore(Move move, HistoryScore bonus)
		{
			auto &score = m_main[move.srcIdx()][move.dstIdx()];
			score.update(bonus);
		}

		inline auto updateNoisyScore(Move move, Piece captured, HistoryScore bonus)
		{
			auto &score = m_noisy[move.srcIdx()][move.dstIdx()][static_cast<i32>(captured)];
			score.update(bonus);
		}

		[[nodiscard]] inline auto quietScore(Move move) const -> HistoryScore
		{
			return m_main[move.srcIdx()][move.dstIdx()];
		}

		[[nodiscard]] inline auto noisyScore(Move move, Piece captured) const -> HistoryScore
		{
			return m_noisy[move.srcIdx()][move.dstIdx()][static_cast<i32>(captured)];
		}

	private:
		// [from][to]
		std::array<std::array<HistoryEntry, 64>, 64> m_main{};

		// [from][to][captured]
		// additional slot for non-capture queen promos
		std::array<std::array<std::array<HistoryEntry, 13>, 64>, 64> m_noisy{};
	};
}
