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
#include <span>

#include "../position/position.h"
#include "../core.h"
#include "../correction.h"
#include "../see.h"

namespace stormphrax::eval
{
	// black, white
	using Contempt = std::array<Score, 2>;

	constexpr usize PawnCacheEntries = 262144;

	struct PawnCacheEntry
	{
		u64 key{};
		TaperedScore eval{};
		Bitboard passers{};
	};

	static_assert(sizeof(PawnCacheEntry) == 24);

	class PawnCache
	{
	public:
		PawnCache()
		{
			m_cache.resize(PawnCacheEntries);
		}

		inline auto probe(u64 key) -> PawnCacheEntry &
		{
			return m_cache[key % PawnCacheEntries];
		}

		inline auto clear()
		{
			std::memset(m_cache.data(), 0, m_cache.size() * sizeof(PawnCacheEntry));
		}

	private:
		std::vector<PawnCacheEntry> m_cache{};
	};

	constexpr Score Tempo = 16;

	[[nodiscard]] inline auto flipSide(Score eval)
	{
		return -eval + Tempo * 2;
	}

	template <bool Correct = true>
	inline auto adjustEval(const Position &pos, std::span<search::PlayedMove> moves,
		i32 ply, const CorrectionHistoryTable *correction, i32 eval, i32 *corrDelta = nullptr)
	{
		eval = eval * (200 - pos.halfmove()) / 200;

		if constexpr (Correct)
		{
			const auto corrected = correction->correct(pos, moves, ply, eval);

			if (corrDelta)
				*corrDelta = std::abs(eval - corrected);

			eval = corrected;
		}

		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}

	auto staticEval(const Position &pos, PawnCache *pawnCache = nullptr, const Contempt &contempt = {}) -> Score;

	template <bool Correct = true>
	inline auto adjustedStaticEval(const Position &pos, std::span<search::PlayedMove> moves, i32 ply,
		const CorrectionHistoryTable *correction, PawnCache *pawnCache = nullptr, const Contempt &contempt = {})
	{
		const auto eval = staticEval(pos, pawnCache, contempt);
		return adjustEval<Correct>(pos, moves, ply, correction, eval);
	}
}
