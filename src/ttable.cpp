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

		const auto capacity = size / sizeof(TTableEntry);

		//TODO handle oom
		m_table.resize(capacity);
		m_table.shrink_to_fit();

		clear();
	}

	auto TTable::probe(ProbedTTableEntry &dst, u64 key, i32 ply) const -> void
	{
		const auto entry = loadEntry(index(key));

		if (entry.type != EntryType::None
			&& packEntryKey(key) == entry.key)
		{
			dst.score = scoreFromTt(static_cast<Score>(entry.score), ply);
			dst.depth = entry.depth;
			dst.move = entry.move;
			dst.type = entry.type;
		}
		else dst.type = EntryType::None;
	}

	auto TTable::put(u64 key, Score score, Move move, i32 depth, i32 ply, EntryType type) -> void
	{
		auto entry = loadEntry(index(key));

		const auto entryKey = packEntryKey(key);

		// always replace empty entries
		const bool replace = entry.key == 0
			// always replace with PV entries
			|| type == EntryType::Exact
			// always replace entries from previous searches
			|| entry.age != m_currentAge
			// otherwise, replace if the depth is greater
			// only keep entries from the same position if their depth is significantly greater
			|| entry.depth < depth + (entry.key == entryKey ? 3 : 0);

		if (!replace)
			return;

#ifndef NDEBUG
		if (std::abs(score) > std::numeric_limits<i16>::max())
			std::cerr << "trying to put out of bounds score " << score << " into ttable" << std::endl;
#endif

		entry.key = entryKey;
		entry.score = static_cast<i16>(scoreToTt(score, ply));
		entry.move = move;
		entry.depth = depth;
		entry.age = m_currentAge;
		entry.type = type;

		storeEntry(index(key), entry);
	}

	auto TTable::clear() -> void
	{
		m_currentAge = 0;

		std::memset(m_table.data(), 0, m_table.size() * sizeof(TTableEntry));
	}

	auto TTable::full() const -> u32
	{
		u32 filledEntries{};

		for (u64 i = 0; i < 1000; ++i)
		{
			if (loadEntry(i).type != EntryType::None)
				++filledEntries;
		}

		return filledEntries;
	}
}
