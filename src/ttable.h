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

#include "types.h"

#include <vector>

#include "core.h"
#include "move.h"
#include "util/range.h"

namespace polaris
{
	constexpr usize DefaultHashSize = 64;
	constexpr util::Range<usize> HashSizeRange{1, 131072};

	enum class EntryType : u8
	{
		Alpha = 0,
		Beta,
		Exact
	};

	struct TTableEntry
	{
		u16 key;
		i16 score;
		Move move;
		u8 depth;
		EntryType type;
	};

#ifdef NDEBUG
	static_assert(sizeof(TTableEntry) == 8);
#endif

	struct ProbedTTableEntry
	{
		i32 score;
		Move move;
		EntryType type;
	};

	class TTable
	{
	public:
		explicit TTable(usize size = DefaultHashSize);
		~TTable() = default;

		void resize(usize size);

		bool probe(ProbedTTableEntry &dst, u64 key, i32 depth, Score alpha, Score beta) const;
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
		[[nodiscard]] inline TTableEntry loadEntry(u64 key) const
		{
			const auto *ptr = static_cast<volatile const i64 *>(&m_table[key & m_mask]);
			const auto v = *ptr;

			TTableEntry entry{};
			std::memcpy(&entry, &v, sizeof(TTableEntry));

			return entry;
		}

		inline void exchangeEntry(u64 key, TTableEntry &entry)
		{
			i64 v{};
			std::memcpy(&v, &entry, sizeof(TTableEntry));

			auto *ptr = static_cast<volatile i64 *>(&m_table[key & m_mask]);
			__atomic_exchange(ptr, &v, &v, __ATOMIC_ACQUIRE);

			std::memcpy(&entry, &v, sizeof(TTableEntry));
		}

		u64 m_mask{};
		std::vector<i64> m_table{};

		std::atomic_size_t m_entries{};
	};
}
