/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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
		inline auto scoreToTt(Score score, i32 ply)
		{
			if (score < -ScoreWin)
				return score + ply;
			else if (score > ScoreWin)
				return score - ply;
			return score;
		}

		inline auto scoreFromTt(Score score, i32 ply)
		{
			if (score < -ScoreWin)
				return score - ply;
			else if (score > ScoreWin)
				return score + ply;
			return score;
		}
	}

	TTable::TTable(usize size)
	{
		resize(size);
	}

	auto TTable::resize(usize size) -> void
	{
		clear();

		size *= 1024 * 1024;

		const usize capacity = std::bit_floor(size / sizeof(TTableEntry));

		//TODO handle oom
		m_table.resize(capacity);
		m_table.shrink_to_fit();

		if (capacity > 0)
			std::memset(m_table.data(), 0, capacity * sizeof(TTableEntry));

		m_mask = capacity - 1;
	}

	auto TTable::probe(ProbedTTableEntry &dst, u64 key, i32 depth, i32 ply, Score alpha, Score beta) const -> bool
	{
		if (m_table.empty())
			return false;

		const auto entry = loadEntry(key);

		if (entry.type != EntryType::None
			&& static_cast<u16>(key >> 48) == entry.key)
		{
			dst.score = scoreFromTt(static_cast<Score>(entry.score), ply);
			dst.depth = entry.depth;
			dst.move = entry.move;
			dst.type = entry.type;

			if (entry.depth >= depth)
			{
				if (entry.type == EntryType::Alpha)
				{
					if (entry.score <= alpha)
						dst.score = alpha;
					else return false;
				}
				else if (entry.type == EntryType::Beta)
				{
					if (entry.score >= beta)
						dst.score = beta;
					else return false;
				}

				return true;
			}
		}
		else dst.type = EntryType::None;

		return false;
	}

	auto TTable::probePvMove(u64 key) const -> Move
	{
		if (m_table.empty())
			return NullMove;

		const auto entry = loadEntry(key);

		if (entry.type == EntryType::Exact
		    && static_cast<u16>(key >> 48) == entry.key)
			return entry.move;

		return NullMove;
	}

	auto TTable::put(u64 key, Score score, Move move, i32 depth, i32 ply, EntryType type) -> void
	{
		if (m_table.empty())
			return;

		auto entry = loadEntry(key);

		const auto entryKey = static_cast<u16>(key >> 48);

		// always replace empty entries
		const bool replace = entry.key == 0
			// always replace with PV entries
			|| type == EntryType::Exact
			// always replace entries from previous searches
			|| entry.age != m_currentAge
			// otherwise, replace if the depth is greater
			// only replace entries from the same position if the depth is significantly greater
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

		exchangeEntry(key, entry);

		if (entry.type == EntryType::None)
			++m_entries;
	}

	auto TTable::clear() -> void
	{
		m_entries = 0;
		m_currentAge = 0;

		if (!m_table.empty())
			std::memset(m_table.data(), 0, m_table.size() * sizeof(TTableEntry));
	}

	auto TTable::full() const -> u32
	{
		return static_cast<u32>(static_cast<f64>(m_entries.load(std::memory_order::relaxed))
			/ static_cast<f64>(m_table.capacity()) * 1000.0);
	}
}
