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
		assert(depth >= 0);
		assert(depth <= MaxDepth);

		auto entry = loadEntry(index(key));

		const auto entryKey = packEntryKey(key);

#ifndef NDEBUG
		if (std::abs(score) > std::numeric_limits<i16>::max())
			std::cerr << "trying to put out of bounds score " << score << " into ttable" << std::endl;
#endif

		entry.key = entryKey;
		entry.score = static_cast<i16>(scoreToTt(score, ply));
		entry.move = move;
		entry.depth = depth;
		entry.type = type;

		storeEntry(index(key), entry);
	}

	auto TTable::clear() -> void
	{
		std::memset(m_table.data(), 0, m_table.size() * sizeof(Entry));
	}

	auto TTable::full() const -> u32
	{
		u32 filledEntries{};

		for (u64 i = 0; i < 1000; ++i)
		{
			const auto entry = loadEntry(i);
			if (entry.type != EntryType::None)
				++filledEntries;
		}

		return filledEntries;
	}
}
