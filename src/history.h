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

#pragma once

#include "types.h"

#include <array>
#include <utility>
#include <cstring>

#include "core.h"
#include "move.h"
#include "position/position.h"
#include "tunable.h"

namespace stormphrax
{
	inline auto historyAdjustment(i32 depth)
	{
		return std::min(depth * tunable::historyDepthScale() - tunable::historyOffset(),
			tunable::maxHistoryAdjustment());
	}

	inline auto updateHistoryScore(i32 &score, i32 adjustment)
	{
		score += adjustment - score * std::abs(adjustment) / tunable::maxHistory();
	}

	struct HistoryMove
	{
		Piece moving{Piece::None};
		Square dst{Square::None};

		[[nodiscard]] explicit constexpr operator bool() const
		{
			return moving != Piece::None;
		}

		[[nodiscard]] static inline auto from(const PositionBoards &boards, Move move)
		{
			return HistoryMove{boards.pieceAt(move.src()), moveActualDst(move)};
		}

		[[nodiscard]] static inline auto from(const Position &pos, Move move)
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

		[[nodiscard]] inline auto score(HistoryMove move) -> auto &
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto score(HistoryMove move) const
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

	private:
		using Table = std::array<std::array<i32, 64>, 12>;

		Table m_table{};
	};

	class HistoryTable
	{
	public:
		HistoryTable() = default;
		~HistoryTable() = default;

		[[nodiscard]] inline auto entry(HistoryMove move) -> auto &
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto entry(HistoryMove move) const -> const auto &
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto captureScore(HistoryMove move, Piece captured) -> auto &
		{
			return m_captureTable[static_cast<i32>(captured)]
				[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto captureScore(HistoryMove move, Piece captured) const -> const auto &
		{
			return m_captureTable[static_cast<i32>(captured)]
				[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto contEntry(HistoryMove move) -> auto &
		{
			return m_continuationTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto contEntry(HistoryMove move) const -> const auto &
		{
			return m_continuationTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		inline auto clear()
		{
			std::memset(m_table.data(), 0, sizeof(HistoryEntry) * 64 * 12);
			std::memset(m_captureTable.data(), 0, sizeof(i32) * 64 * 12 * 13);
			std::memset(m_continuationTable.data(), 0, sizeof(i32) * 64 * 12 * 64 * 12);
		}

	private:
		using Table = std::array<std::array<HistoryEntry, 64>, 12>;
		// 13 to account for non-capture queen promos
		using CaptureTable = std::array<std::array<std::array<i32, 64>, 12>, 13>;
		using ContinuationTable = std::array<std::array<ContinuationEntry, 64>, 12>;

		Table m_table{};
		CaptureTable m_captureTable{};
		ContinuationTable m_continuationTable{};
	};
}
