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

#include <memory>
#include <limits>
#include <string>
#include <string_view>
#include <optional>
#include <span>

#include "search_fwd.h"
#include "../position/position.h"
#include "../limit/limit.h"

namespace polaris::search
{
	constexpr i32 MaxDepth = 255;

	struct BenchData
	{
		SearchData search{};
		f64 time{};
	};

	class ISearcher
	{
	public:
		virtual ~ISearcher() = default;

		virtual void newGame() = 0;

		virtual void startSearch(const Position &pos, i32 maxDepth, std::unique_ptr<limit::ISearchLimiter> limiter) = 0;
		virtual void stop() = 0;

		virtual void runBench(BenchData &data, const Position &pos, i32 depth) = 0;

		virtual bool searching() = 0;

		virtual void clearHash() = 0;
		virtual void setHashSize(usize size) = 0;

		[[nodiscard]] static std::span<const std::string> searchers();

		[[nodiscard]] static std::unique_ptr<ISearcher> createDefault();
		[[nodiscard]] static std::optional<std::unique_ptr<ISearcher>> create(const std::string &name,
			std::optional<usize> hashSize);
	};
}
