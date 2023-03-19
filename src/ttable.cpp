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

#include "ttable.h"

#include <cstring>
#include <bit>

namespace polaris
{
	TTable::TTable(usize size)
	{
		resize(size);
	}

	void TTable::resize(usize size)
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

	bool TTable::probe(TTableEntry &dst, u64 key, i32 depth, Score alpha, Score beta) const
	{
		if (m_table.empty())
			return false;

		const auto &entry = m_table[key & m_mask];

		if (key == entry.key)
		{
			dst.move = entry.move;

			if (entry.depth > depth)
			{
				switch (entry.type)
				{
				case EntryType::Alpha:
					if (entry.score <= alpha)
					{
						dst = entry;
						dst.score = alpha;
					}
					else return false;
					break;
				case EntryType::Beta:
					if (entry.score >= beta)
					{
						dst = entry;
						dst.score = beta;
					}
					else return false;
					break;
				default: dst = entry; break;
				}

				return true;
			}
		}

		return false;
	}

	Move TTable::probeMove(u64 key) const
	{
		if (m_table.empty())
			return NullMove;

		const auto &entry = m_table[key & m_mask];

		if (key == entry.key)
			return entry.move;

		return NullMove;
	}

	void TTable::put(u64 key, Score score, Move move, i32 depth, EntryType type)
	{
		if (m_table.empty())
			return;

		auto &entry = m_table[key & m_mask];

		if (key == entry.key && entry.depth > depth)
			return;

		if (entry.key == 0)
			++m_entries;

		entry.key = key;
		entry.score = score;
		entry.move = move;
		entry.depth = depth;
		entry.type = type;
	}

	void TTable::clear()
	{
		m_entries = 0;

		if (!m_table.empty())
			std::memset(m_table.data(), 0, m_table.size() * sizeof(TTableEntry));
	}

	u32 TTable::full() const
	{
		return static_cast<u32>(static_cast<f64>(m_entries) / static_cast<f64>(m_table.capacity()) * 1000.0);
	}
}
