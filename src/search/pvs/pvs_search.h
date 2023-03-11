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

#include "../../types.h"

#include <utility>

#include "../search.h"
#include "../../util/timer.h"
#include "../../ttable.h"
#include "../../eval/eval.h"

namespace polaris::search::pvs
{
	struct PvsData;

	class PvsSearcher final : public ISearcher
	{
	public:
		explicit PvsSearcher(std::optional<size_t> hashSize = {});
		~PvsSearcher() final = default;

		void newGame() final;

		void startSearch(Position &pos, i32 maxDepth, std::unique_ptr<limit::ISearchLimiter> limiter) final;
		void stop() final;

		void clearHash() final;
		void setHashSize(size_t size) final;

	private:
		TTable m_table{};
		eval::PawnCache m_pawnCache{};

		bool m_stop{};

		bool shouldStop(const SearchData &data, const limit::ISearchLimiter &limiter);

		Score search(PvsData &data, Position &pos, const limit::ISearchLimiter &limiter,
			i32 depth, i32 ply, Score alpha, Score beta);
		Score qsearch(PvsData &data, Position &pos, const limit::ISearchLimiter &limiter,
			Score alpha, Score beta, i32 ply);

		void report(const SearchData &data, Move move, f64 time, Score score, Score alpha, Score beta);
	};
}
