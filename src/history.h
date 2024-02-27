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

#include <array>
#include <utility>
#include <cstring>

#include "core.h"
#include "move.h"
#include "position/position.h"
#include "tunable.h"

namespace stormphrax
{
	using HistoryScore = i16;

	inline auto historyBonus(i32 depth) -> HistoryScore
	{
		return static_cast<HistoryScore>(std::min(
			depth * tunable::historyBonusDepthScale() - tunable::historyBonusOffset(),
			tunable::maxHistoryBonus()
		));
	}

	inline auto historyPenalty(i32 depth) -> HistoryScore
	{
		return static_cast<HistoryScore>(-std::min(
			depth * tunable::historyPenaltyDepthScale() - tunable::historyPenaltyOffset(),
			tunable::maxHistoryPenalty()
		));
	}

	inline auto updateHistoryScore(HistoryScore &score, HistoryScore adjustment)
	{
		score += adjustment - score * std::abs(adjustment) / tunable::maxHistory();
	}

	struct HistoryMove
	{
		Piece moving{Piece::None};
		Square src{Square::None};
		Square dst{Square::None};

		[[nodiscard]] explicit constexpr operator bool() const
		{
			return moving != Piece::None;
		}

		[[nodiscard]] static inline auto from(const PositionBoards &boards, Move move)
		{
			return HistoryMove{boards.pieceAt(move.src()), move.src(), move.historyDst()};
		}

		[[nodiscard]] static inline auto from(const Position &pos, Move move)
		{
			return from(pos.boards(), move);
		}
	};

	using PrevMoveTable = std::array<HistoryMove, MaxDepth>;

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
		using Table = std::array<std::array<HistoryScore, 64>, 12>;

		Table m_table{};
	};

	class HistoryTable
	{
	public:
		HistoryTable() = default;
		~HistoryTable() = default;

		inline auto updateCountermove(i32 ply, std::span<const HistoryMove> prevMoves, Move countermove)
		{
			if (ply > 0 && prevMoves[ply - 1])
				countermoveEntry(prevMoves[ply - 1]) = countermove;
		}

		[[nodiscard]] inline auto countermove(i32 ply, std::span<const HistoryMove> prevMoves) const
		{
			if (ply > 0 && prevMoves[ply - 1])
				return countermoveEntry(prevMoves[ply - 1]);
			else return NullMove;
		}

		inline auto updateQuietScore(HistoryMove move, Bitboard threats,
			i32 ply, std::span<const HistoryMove> prevMoves, HistoryScore adjustment)
		{
			updateMainScore(move, threats[move.src], threats[move.dst], adjustment);

			updateContinuationScore(move, ply, prevMoves, 1, adjustment);
			updateContinuationScore(move, ply, prevMoves, 2, adjustment);
			updateContinuationScore(move, ply, prevMoves, 4, adjustment);
		}

		[[nodiscard]] inline auto quietScore(HistoryMove move, Bitboard threats,
			i32 ply, std::span<const HistoryMove> prevMoves) const
		{
			auto history = mainScore(move, threats[move.src], threats[move.dst]);

			history += continuationScore(move, ply, prevMoves, 1);
			history += continuationScore(move, ply, prevMoves, 2);
			history += continuationScore(move, ply, prevMoves, 4);

			return history;
		}

		inline auto updateNoisyScore(HistoryMove move, Bitboard threats, Piece captured, HistoryScore adjustment)
		{
			updateHistoryScore(noisyEntry(move, captured, threats[move.dst]), adjustment);
		}

		[[nodiscard]] inline auto noisyScore(HistoryMove move, Bitboard threats, Piece captured) const
		{
			return noisyEntry(move, captured, threats[move.dst]);
		}

		inline auto clear()
		{
			std::memset(m_table.data(), 0, sizeof(Table));
			std::memset(m_countermoveTable.data(), 0, sizeof(CountermoveTable));
			std::memset(m_captureTable.data(), 0, sizeof(CaptureTable));
			std::memset(m_continuationTable.data(), 0, sizeof(ContinuationTable));
		}

	private:
		using Table = std::array<std::array<std::array<std::array<HistoryScore, 2>, 2>, 64>, 12>;
		using CountermoveTable = std::array<std::array<Move, 64>, 12>;
		// 13 to account for non-capture queen promos
		using CaptureTable = std::array<std::array<std::array<std::array<HistoryScore, 2>, 64>, 12>, 13>;
		using ContinuationTable = std::array<std::array<ContinuationEntry, 64>, 12>;

		[[nodiscard]] inline auto entry(HistoryMove move, bool srcThreat, bool dstThreat) -> auto &
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)][srcThreat][dstThreat];
		}

		[[nodiscard]] inline auto entry(HistoryMove move, bool srcThreat, bool dstThreat) const -> const auto &
		{
			return m_table[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)][srcThreat][dstThreat];
		}

		[[nodiscard]] inline auto countermoveEntry(HistoryMove move) -> Move &
		{
			return m_countermoveTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto countermoveEntry(HistoryMove move) const -> Move
		{
			return m_countermoveTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto noisyEntry(HistoryMove move,
			Piece captured, bool defended) -> HistoryScore &
		{
			return m_captureTable[static_cast<i32>(captured)]
				[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)][defended];
		}

		[[nodiscard]] inline auto noisyEntry(HistoryMove move,
			Piece captured, bool defended) const -> const HistoryScore &
		{
			return m_captureTable[static_cast<i32>(captured)]
				[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)][defended];
		}

		[[nodiscard]] inline auto contEntry(HistoryMove move) -> auto &
		{
			return m_continuationTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto contEntry(HistoryMove move) const -> const auto &
		{
			return m_continuationTable[static_cast<i32>(move.moving)][static_cast<i32>(move.dst)];
		}

		[[nodiscard]] inline auto mainScore(HistoryMove move, bool srcThreat, bool dstThreat) const -> i32
		{
			return entry(move, srcThreat, dstThreat);
		}

		inline auto updateMainScore(HistoryMove move, bool srcThreat, bool dstThreat, HistoryScore adjustment) -> void
		{
			updateHistoryScore(entry(move, srcThreat, dstThreat), adjustment);
		}

		[[nodiscard]] inline auto continuationScore(HistoryMove move,
			i32 ply, std::span<const HistoryMove> prevMoves, i32 pliesAgo) const -> i32
		{
			if (ply >= pliesAgo && prevMoves[ply - pliesAgo])
				return contEntry(prevMoves[ply - pliesAgo]).score(move);
			else return 0;
		}

		inline auto updateContinuationScore(HistoryMove move,
			i32 ply, std::span<const HistoryMove> prevMoves, i32 pliesAgo, HistoryScore adjustment) -> void
		{
			if (ply >= pliesAgo && prevMoves[ply - pliesAgo])
				updateHistoryScore(contEntry(prevMoves[ply - pliesAgo]).score(move), adjustment);
		}

		Table m_table{};
		CountermoveTable m_countermoveTable{};
		CaptureTable m_captureTable{};
		ContinuationTable m_continuationTable{};
	};
}
