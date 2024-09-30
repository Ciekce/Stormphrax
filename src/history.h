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

#include <cmath>
#include <array>
#include <cstring>
#include <utility>

#include "tunable.h"
#include "move.h"
#include "bitboard.h"
#include "util/multi_array.h"

namespace stormphrax
{
	using HistoryScore = i16;

	struct HistoryEntry
	{
		i16 value{};

		HistoryEntry() = default;
		HistoryEntry(HistoryScore v) : value{v} {}

		[[nodiscard]] inline operator HistoryScore() const
		{
			return value;
		}

		[[nodiscard]] inline auto operator=(HistoryScore v) -> auto &
		{
			value = v;
			return *this;
		}

		inline auto update(HistoryScore bonus)
		{
			value += bonus - value * std::abs(bonus) / tunable::maxHistory();
		}
	};

	inline auto historyBonus(i32 depth) -> HistoryScore
	{
		return static_cast<HistoryScore>(std::clamp(
			depth * tunable::historyBonusDepthScale() - tunable::historyBonusOffset(),
			0, tunable::maxHistoryBonus()
		));
	}

	inline auto historyPenalty(i32 depth) -> HistoryScore
	{
		return static_cast<HistoryScore>(-std::clamp(
			depth * tunable::historyPenaltyDepthScale() - tunable::historyPenaltyOffset(),
			0, tunable::maxHistoryPenalty()
		));
	}

	class ContinuationSubtable
	{
	public:
		ContinuationSubtable() = default;
		~ContinuationSubtable() = default;

		//TODO take two args when c++23 is usable
		inline auto operator[](std::pair<Piece, Move> move) const -> HistoryScore
		{
			const auto [piece, mv] = move;
			return m_data[static_cast<i32>(piece)][static_cast<i32>(mv.dst())];
		}

		inline auto operator[](std::pair<Piece, Move> move) -> auto &
		{
			const auto [piece, mv] = move;
			return m_data[static_cast<i32>(piece)][static_cast<i32>(mv.dst())];
		}

	private:
		// [piece type][to]
		util::MultiArray<HistoryEntry, 12, 64> m_data{};
	};

	class HistoryTables
	{
	public:
		HistoryTables() = default;
		~HistoryTables() = default;

		inline auto clear()
		{
			std::memset(&m_main        , 0, sizeof(m_main        ));
			std::memset(&m_continuation, 0, sizeof(m_continuation));
			std::memset(&m_noisy       , 0, sizeof(m_noisy       ));
		}

		[[nodiscard]] inline auto contTable(Piece moving, Square to) const -> const auto &
		{
			return m_continuation[static_cast<i32>(moving)][static_cast<i32>(to)];
		}

		[[nodiscard]] inline auto contTable(Piece moving, Square to) -> auto &
		{
			return m_continuation[static_cast<i32>(moving)][static_cast<i32>(to)];
		}

		inline auto updateConthist(std::span<ContinuationSubtable *> continuations,
			i32 ply, Piece moving, Move move, HistoryScore bonus)
		{
			updateConthist(continuations, ply, moving, move, bonus, 1);
			updateConthist(continuations, ply, moving, move, bonus, 2);
			updateConthist(continuations, ply, moving, move, bonus, 4);
		}

		inline auto updateQuietScore(std::span<ContinuationSubtable *> continuations,
			i32 ply, Bitboard threats, Piece moving, Move move, HistoryScore bonus)
		{
			mainEntry(threats, move).update(bonus);
			updateConthist(continuations, ply, moving, move, bonus);
		}

		inline auto updateNoisyScore(Move move, Piece captured, HistoryScore bonus)
		{
			noisyEntry(move, captured).update(bonus);
		}

		inline auto conthistScore(std::span<ContinuationSubtable *const> continuations,
			i32 ply, Piece moving, Move move, i32 offset) const -> i32
		{
			if (offset <= ply)
				return conthistEntry(continuations, ply, offset)[{moving, move}];

			return 0;
		}

		[[nodiscard]] inline auto quietScore(std::span<ContinuationSubtable *const> continuations,
			i32 ply, Bitboard threats, Piece moving, Move move) const -> i32
		{
			i32 score{};

			score += mainEntry(threats, move);

			score += conthistScore(continuations, ply, moving, move, 1);
			score += conthistScore(continuations, ply, moving, move, 2);
			score += conthistScore(continuations, ply, moving, move, 4) / 2;

			return score;
		}

		[[nodiscard]] inline auto noisyScore(Move move, Piece captured) const -> i32
		{
			return noisyEntry(move, captured);
		}

	private:
		// [from][to][from attacked][to attacked]
		util::MultiArray<HistoryEntry, 64, 64, 2, 2> m_main{};
		// [prev piece][to][curr piece type][to]
		util::MultiArray<ContinuationSubtable, 12, 64> m_continuation{};

		// [from][to][captured]
		// additional slot for non-capture queen promos
		util::MultiArray<HistoryEntry, 64, 64, 13> m_noisy{};

		static inline auto updateConthist(std::span<ContinuationSubtable *> continuations,
			i32 ply, Piece moving, Move move, HistoryScore bonus, i32 offset) -> void
		{
			if (offset <= ply)
				conthistEntry(continuations, ply, offset)[{moving, move}].update(bonus);
		}

		[[nodiscard]] inline auto mainEntry(Bitboard threats, Move move) const -> const HistoryEntry &
		{
			return m_main[move.srcIdx()][move.dstIdx()][threats[move.src()]][threats[move.dst()]];
		}

		[[nodiscard]] inline auto mainEntry(Bitboard threats, Move move) -> HistoryEntry &
		{
			return m_main[move.srcIdx()][move.dstIdx()][threats[move.src()]][threats[move.dst()]];
		}

		[[nodiscard]] static inline auto conthistEntry(std::span<ContinuationSubtable *const> continuations,
			i32 ply, i32 offset) -> const ContinuationSubtable &
		{
			return *continuations[ply - offset];
		}

		[[nodiscard]] static inline auto conthistEntry(
			std::span<ContinuationSubtable *> continuations, i32 ply, i32 offset) -> ContinuationSubtable &
		{
			return *continuations[ply - offset];
		}

		[[nodiscard]] inline auto noisyEntry(Move move, Piece captured) const -> const HistoryEntry &
		{
			return m_noisy[move.srcIdx()][move.dstIdx()][static_cast<i32>(captured)];
		}

		[[nodiscard]] inline auto noisyEntry(Move move, Piece captured) -> HistoryEntry &
		{
			return m_noisy[move.srcIdx()][move.dstIdx()][static_cast<i32>(captured)];
		}
	};
}
