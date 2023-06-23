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
#include <atomic>
#include <cstring>

#include "core.h"
#include "move.h"
#include "util/range.h"

namespace polaris
{
	constexpr usize DefaultHashSize = 64;
	constexpr util::Range<usize> HashSizeRange{1, 131072};

	enum class EntryType : u8
	{
		None = 0,
		Alpha,
		Beta,
		Exact
	};

	struct TTableEntry
	{
		u16 key;
		i16 score;
		Move move;
		u8 depth;
		u8 age : 6;
		EntryType type : 2;
	};

	static_assert(sizeof(TTableEntry) == 8);

	struct ProbedTTableEntry
	{
		i32 score;
		i32 depth;
		Move move;
		EntryType type;
	};

	class TTable
	{
	public:
		explicit TTable(usize size = DefaultHashSize);
		~TTable() = default;

		auto resize(usize size) -> void;

		auto probe(ProbedTTableEntry &dst, u64 key, i32 depth, i32 ply, Score alpha, Score beta) const -> bool;
		[[nodiscard]] auto probePvMove(u64 key) const -> Move;

		auto put(u64 key, Score score, Move move, i32 depth, i32 ply, EntryType type) -> void;

		auto clear() -> void;

		[[nodiscard]] auto full() const -> u32;

		inline auto prefetch(u64 key)
		{
			if (m_table.empty())
				return;

			__builtin_prefetch(&m_table[key & m_mask]);
		}

		inline auto age()
		{
			m_currentAge = (m_currentAge + 1) % 64;
		}

	private:
		[[nodiscard]] inline auto loadEntry(u64 key) const
		{
			const auto *ptr = static_cast<volatile const i64 *>(&m_table[key & m_mask]);
			const auto v = *ptr;

			TTableEntry entry{};
			std::memcpy(&entry, &v, sizeof(TTableEntry));

			return entry;
		}

		inline auto exchangeEntry(u64 key, TTableEntry &entry)
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

		u8 m_currentAge{};
	};
}
