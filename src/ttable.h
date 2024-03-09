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

#include "types.h"

#include <vector>
#include <atomic>
#include <cstring>
#include <bit>

#include "core.h"
#include "move.h"
#include "util/range.h"

namespace stormphrax
{
	constexpr usize DefaultTtSize = 64;
	constexpr util::Range<usize> TtSizeRange{1, 131072};

	enum class EntryType : u8
	{
		None = 0,
		Alpha,
		Beta,
		Exact
	};

	struct ProbedTtEntry
	{
		Score score;
		Score staticEval;
		i32 depth;
		Move move;
		EntryType type;
	};

	class TTable
	{
	public:
		explicit TTable(usize size = DefaultTtSize);
		~TTable() = default;

		auto resize(usize size) -> void;

		auto probe(ProbedTtEntry &dst, u64 key, i32 ply) const -> void;

		auto put(u64 key, Score score, Score staticEval, Move move, i32 depth, i32 ply, EntryType type) -> void;

		auto clear() -> void;

		[[nodiscard]] auto full() const -> u32;

		inline auto prefetch(u64 key)
		{
			__builtin_prefetch(&m_table[index(key)]);
		}

		inline auto age()
		{
			m_currentAge = (m_currentAge + 1) % 64;
		}

	private:
		static constexpr usize EntriesPerCluster = 3;

		struct Entry
		{
			u16 key;
			i16 score;
			i16 staticEval;
			Move move;
			u8 depth;
			u8 age : 6;
			EntryType type : 2;
		};

		struct Cluster
		{
			std::array<Entry, EntriesPerCluster> entries{};
			// round up to nearest power of 2 bytes
			[[maybe_unused]] std::array<u8,
				std::bit_ceil(sizeof(Entry) * EntriesPerCluster) - sizeof(Entry) * EntriesPerCluster>
				padding{};
		};

		static_assert(sizeof(Cluster) == 32);

		[[nodiscard]] inline auto index(u64 key) const -> u64
		{
			// this emits a single mul on both x64 and arm64
			return static_cast<u64>((static_cast<u128>(key) * static_cast<u128>(m_table.size())) >> 64);
		}

		std::vector<Cluster> m_table{};
		u8 m_currentAge{};
	};
}
