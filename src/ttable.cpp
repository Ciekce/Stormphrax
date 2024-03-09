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
#include <stdexcept>
#include <iostream>
#include <cstdlib>

#ifndef NDEBUG
#include <iostream>
#endif

namespace stormphrax
{
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

		const auto capacity = size / sizeof(Cluster);

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

	auto TTable::probe(ProbedTtEntry &dst, u64 key, i32 ply) const -> void
	{
		const auto &cluster = m_table[index(key)];

		Entry entry{};
		for (const auto candidate : cluster.entries)
		{
			if (candidate.type != EntryType::None
				&& packEntryKey(key) == candidate.key)
			{
				entry = candidate;
				break;
			}
		}

		if (entry.type != EntryType::None)
		{
			const auto score = entry.score;
			dst.score = std::abs(score) == ScoreMate ? score : scoreFromTt(static_cast<Score>(entry.score), ply);
			dst.staticEval = entry.staticEval;
			dst.depth = entry.depth;
			dst.move = entry.move;
			dst.type = entry.type;
		}
		else dst.type = EntryType::None;
	}

	auto TTable::put(u64 key, Score score, Score staticEval, Move move, i32 depth, i32 ply, EntryType type) -> void
	{
		assert(depth >= 0);
		assert(depth < MaxDepth);

		assert(std::abs(staticEval) <= std::numeric_limits<i16>::max());

		const auto entryKey = packEntryKey(key);

		auto &cluster = m_table[index(key)];

		Entry entry{};
		usize entryIdx = 0;

		auto worstQuality = std::numeric_limits<i32>::max();

		for (usize i = 0; i < EntriesPerCluster; ++i)
		{
			const auto candidate = cluster.entries[i];

			if (candidate.type == EntryType::None || candidate.key == entryKey)
			{
				entry = candidate;
				entryIdx = i;
				break;
			}

			const auto age = std::min(0, static_cast<i32>(m_currentAge) - static_cast<i32>(entry.age));
			const auto quality = static_cast<i32>(entry.depth) - age;

			if (quality < worstQuality)
			{
				entry = candidate;
				entryIdx = i;

				worstQuality = quality;
			}
		}

		// always replace empty entries
		const bool replace = entry.type == EntryType::None
			// always replace with PV entries
			|| type == EntryType::Exact
			// otherwise, replace if the depth is greater
			// only keep entries from the same position if their depth is significantly greater
			|| entry.depth < depth + (entry.key == entryKey ? 3 : 0);

		if (!replace)
			return;

#ifndef NDEBUG
		if (std::abs(score) != ScoreMate && std::abs(scoreToTt(score, ply)) > std::numeric_limits<i16>::max())
			std::cerr << "trying to put out of bounds score " << score << " into ttable" << std::endl;
#endif

		entry.key = entryKey;
		entry.score = static_cast<i16>(std::abs(score) == ScoreMate ? score : scoreToTt(score, ply));
		entry.staticEval = static_cast<i16>(staticEval);
		entry.move = move;
		entry.depth = depth;
		entry.age = m_currentAge;
		entry.type = type;

		cluster.entries[entryIdx] = entry;
	}

	auto TTable::clear() -> void
	{
		m_currentAge = 0;
		std::memset(m_table.data(), 0, m_table.size() * sizeof(Cluster));
	}

	auto TTable::full() const -> u32
	{
		u32 filledEntries{};

		for (u64 i = 0; i < 1000; ++i)
		{
			const auto &cluster = m_table[i];

			for (const auto &entry : cluster.entries)
			{
				if (entry.type != EntryType::None)
					++filledEntries;
			}
		}

		return filledEntries / EntriesPerCluster;
	}
}
