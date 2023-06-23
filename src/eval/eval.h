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

#include <vector>
#include <cstring>

#include "../core.h"
#include "../position/position.h"

namespace polaris::eval
{
	constexpr Score Tempo = 16;

	constexpr usize PawnCacheEntries = 262144;

	struct PawnCacheEntry
	{
		u64 key{};
		TaperedScore eval{};
		Bitboard passers{};
	};

	static_assert(util::resetLsb(PawnCacheEntries) == 0); // power of 2
	static_assert(sizeof(PawnCacheEntry) == 24);

	class PawnCache
	{
	public:
		PawnCache()
		{
			m_cache.resize(PawnCacheEntries);
		}

		~PawnCache() = default;

		inline auto probe(u64 key) -> PawnCacheEntry &
		{
			constexpr usize PawnCacheMask = PawnCacheEntries - 1;
			return m_cache[key & PawnCacheMask];
		}

		inline auto clear()
		{
			std::memset(m_cache.data(), 0, m_cache.size() * sizeof(PawnCacheEntry));
		}

	private:
		std::vector<PawnCacheEntry> m_cache{};
	};

	auto staticEval(const Position &pos, PawnCache *pawnCache = nullptr) -> Score;

	inline auto staticEvalAbs(const Position &pos, PawnCache *pawnCache = nullptr)
	{
		const auto eval = staticEval(pos, pawnCache);
		return pos.toMove() == Color::Black ? -eval : eval;
	}

	inline auto flipTempo(Score score)
	{
		return score + Tempo * 2;
	}

	auto printEval(const Position &pos, PawnCache *pawnCache = nullptr) -> void;
}
