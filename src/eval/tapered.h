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

#include <limits>
#include <cstring>

#include "../core.h"

namespace stormphrax::eval
{
	class TaperedScore
	{
	public:
		constexpr TaperedScore() : m_score{} {}

		constexpr TaperedScore(Score midgame, Score endgame)
			: m_score{static_cast<i32>(static_cast<u32>(endgame) << 16) + midgame}
		{
			assert(std::numeric_limits<i16>::min() <= midgame && std::numeric_limits<i16>::max() >= midgame);
			assert(std::numeric_limits<i16>::min() <= endgame && std::numeric_limits<i16>::max() >= endgame);
		}

		[[nodiscard]] inline auto midgame() const
		{
			const auto mg = static_cast<u16>(m_score);

			i16 v{};
			std::memcpy(&v, &mg, sizeof(mg));

			return static_cast<Score>(v);
		}

		[[nodiscard]] inline auto endgame() const
		{
			const auto eg = static_cast<u16>(static_cast<u32>(m_score + 0x8000) >> 16);

			i16 v{};
			std::memcpy(&v, &eg, sizeof(eg));

			return static_cast<Score>(v);
		}

		[[nodiscard]] constexpr auto operator+(const TaperedScore &other) const
		{
			return TaperedScore{m_score + other.m_score};
		}

		constexpr auto operator+=(const TaperedScore &other) -> auto &
		{
			m_score += other.m_score;
			return *this;
		}

		[[nodiscard]] constexpr auto operator-(const TaperedScore &other) const
		{
			return TaperedScore{m_score - other.m_score};
		}

		constexpr auto operator-=(const TaperedScore &other) -> auto &
		{
			m_score -= other.m_score;
			return *this;
		}

		[[nodiscard]] constexpr auto operator*(i32 v) const
		{
			return TaperedScore{m_score * v};
		}

		constexpr auto operator*=(i32 v) -> auto &
		{
			m_score *= v;
			return *this;
		}

		[[nodiscard]] constexpr auto operator-() const
		{
			return TaperedScore{-m_score};
		}

		[[nodiscard]] constexpr auto operator==(const TaperedScore &other) const -> bool = default;

	private:
		explicit constexpr TaperedScore(i32 score) : m_score{score} {}

		i32 m_score;
	};
}
