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

#include "../search_fwd.h"

namespace polaris::limit
{
	class ISearchLimiter
	{
	public:
		virtual ~ISearchLimiter() = default;

		virtual void update(const search::SearchData &data, bool stableBestMove) {}

		[[nodiscard]] virtual bool stop(const search::SearchData &data, bool allowSoftTimeout) const = 0;
	};
}
