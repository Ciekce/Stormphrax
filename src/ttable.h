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

#include <vector>

#include "types.h"
#include "core.h"
#include "move.h"
#include "util/range.h"

namespace polaris
{
	constexpr u32 DefaultHashSize = 64;
	constexpr util::Range<size_t> HashSizeRange{1, 131072};

	enum class EntryType : u8
	{
		Alpha = 0,
		Beta,
		Exact
	};

	struct TTableEntry
	{
		u64 key;
		i32 score;
		Move move;
		u8 depth;
		EntryType type;
	};

	class TTable
	{
	public:
		explicit TTable(size_t size = DefaultHashSize);
		~TTable() = default;

		void resize(size_t size);

		bool probe(TTableEntry &dst, u64 key, i32 depth, Score alpha, Score beta) const;
		[[nodiscard]] Move probeMove(u64 key) const;

		void put(u64 key, Score score, Move move, i32 depth, EntryType type);

		void clear();

		[[nodiscard]] u32 full() const;

		inline void prefetch(u64 key)
		{
			if (m_table.empty())
				return;

			__builtin_prefetch(&m_table[key & m_mask]);
		}

	private:
		u64 m_mask{};
		std::vector<TTableEntry> m_table{};

		size_t m_entries{};
	};
}
