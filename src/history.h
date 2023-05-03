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

#include <array>

#include "core.h"
#include "move.h"
#include "position/position.h"

namespace polaris
{
	struct HistoryMove
	{
		Piece moving{Piece::None};
		Square dst{Square::None};

		[[nodiscard]] explicit constexpr operator bool() const
		{
			return moving != Piece::None;
		}

		[[nodiscard]] static inline HistoryMove from(const PositionBoards &boards, Move move)
		{
			return {boards.pieceAt(move.src()), moveActualDst(move)};
		}

		[[nodiscard]] static inline HistoryMove from(const Position &pos, Move move)
		{
			return from(pos.boards(), move);
		}
	};

	struct HistoryEntry
	{
		i32 score{};
		Move countermove{NullMove};
	};

	class ContinuationEntry
	{
	public:
		ContinuationEntry() = default;
		~ContinuationEntry() = default;

		[[nodiscard]] inline auto &score(HistoryMove move)
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto score(HistoryMove move) const
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

	private:
		using Table = std::array<std::array<i32, 64>, 12>;

		Table m_table;
	};

	class HistoryTable
	{
	public:
		HistoryTable() = default;
		~HistoryTable() = default;

		[[nodiscard]] inline auto &entry(HistoryMove move)
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline const auto &entry(HistoryMove move) const
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto &contEntry(HistoryMove move)
		{
			return m_continuationTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline const auto &contEntry(HistoryMove move) const
		{
			return m_continuationTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		inline void age()
		{
			for (auto &pieceTable : m_table)
			{
				for (auto &dstSquare : pieceTable)
				{
					dstSquare.score /= 2;
				}
			}
		}

		inline void clear()
		{
			m_table = Table{};
		}

	private:
		using Table = std::array<std::array<HistoryEntry, 64>, 12>;
		using ContinuationTable = std::array<std::array<ContinuationEntry, 64>, 12>;

		Table m_table{};
		ContinuationTable m_continuationTable{};
	};
}
