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

#include "ttable.h"

#include <cstring>
#include <bit>
#include <iostream>
#include <thread>

#include "tunable.h"
#include "util/cemath.h"

namespace stormphrax
{
	using namespace stormphrax::tunable;

	namespace
	{
		// for a long time, these were backwards
		// cheers toanth
		inline auto scoreToTt(Score score, i32 ply)
		{
			if (score < -ScoreWin)
				return score - ply;
			else if (score > ScoreWin)
				return score + ply;
			return score;
		}

		inline auto scoreFromTt(Score score, i32 ply)
		{
			if (score < -ScoreWin)
				return score + ply;
			else if (score > ScoreWin)
				return score - ply;
			return score;
		}

		inline auto packEntryKey(u64 key)
		{
			return static_cast<u16>(key);
		}
	}

	TTable::TTable(usize size)
	{
		resize(size);
	}

	auto TTable::resize(usize size) -> void
	{
		size *= 1024 * 1024;

		const auto capacity = size / sizeof(Entry);

		// don't bother reallocating if we're already at the right size
		if (m_table.size() != capacity)
		{
			try
			{
				m_table.resize(capacity);
				m_table.shrink_to_fit();
			}
			catch (...)
			{
				std::cout << "info string Failed to reallocate TT - out of memory?" << std::endl;
				throw;
			}
		}

		clear();
	}

	auto TTable::probe(ProbedTTableEntry &dst, u64 key, i32 ply) const -> bool
	{
		const auto entry = loadEntry(index(key));

		if (packEntryKey(key) == entry.key)
		{
			dst.score = scoreFromTt(static_cast<Score>(entry.score), ply);
			dst.staticEval = static_cast<Score>(entry.staticEval);
			dst.depth = entry.depth;
			dst.move = entry.move;
			dst.wasPv = entry.pv();
			dst.flag = entry.flag();

			return true;
		}

		return false;
	}

	auto TTable::put(u64 key, Score score, Score staticEval,
		Move move, i32 depth, i32 ply, TtFlag flag, bool pv) -> void
	{
		assert(depth >= 0);
		assert(depth <= MaxDepth);

		assert(staticEval == ScoreNone || staticEval > -ScoreWin);
		assert(staticEval == ScoreNone || staticEval <  ScoreWin);

		auto entry = loadEntry(index(key));

		const auto newKey = packEntryKey(key);

#ifndef NDEBUG
		if (std::abs(score) > std::numeric_limits<i16>::max())
			std::cerr << "trying to put out of bounds score " << score << " into ttable" << std::endl;
#endif

		// Roughly the SF replacement scheme
		if (!(flag == TtFlag::Exact
				|| newKey != entry.key
				|| entry.age() != m_age
				|| depth + ttReplacementDepthOffset() + pv * ttReplacementPvOffset() > entry.depth))
			return;

		if (move || entry.key != newKey)
			entry.move = move;

		entry.key = newKey;
		entry.score = static_cast<i16>(scoreToTt(score, ply));
		entry.staticEval = static_cast<i16>(staticEval);
		entry.depth = depth;
		entry.setAgePvFlag(m_age, pv, flag);

		storeEntry(index(key), entry);
	}

	auto TTable::clear() -> void
	{
		const auto threadCount = g_opts.threads;

		std::vector<std::jthread> threads{};
		threads.reserve(threadCount);

		const auto chunkSize = util::ceilDiv<usize>(m_table.size(), threadCount);

		for (u32 i = 0; i < threadCount; ++i)
		{
			threads.emplace_back([this, chunkSize, i]
			{
				const auto start = chunkSize * i;
				const auto end = std::min(start + chunkSize, m_table.size());

				const auto count = end - start;

				std::memset(&m_table[start], 0, count * sizeof(Entry));
			});
		}

		m_age = 0;
	}

	auto TTable::full() const -> u32
	{
		u32 filledEntries{};

		for (u64 i = 0; i < 1000; ++i)
		{
			const auto entry = loadEntry(i);
			if (entry.flag() != TtFlag::None && entry.age() == m_age)
				++filledEntries;
		}

		return filledEntries;
	}
}
