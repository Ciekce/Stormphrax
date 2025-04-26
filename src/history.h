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

		[[nodiscard]] inline auto get() const -> HistoryScore
		{
			return value;
		}

		inline auto update(HistoryScore bonus)
		{
			value += bonus - value * std::abs(bonus) / tunable::maxHistory();
		}

		[[nodiscard]] inline auto operator=(HistoryScore v) -> auto &
		{
			value = v;
			return *this;
		}
	};

	struct MainHistoryEntry
	{
		util::MultiArray<HistoryEntry, 2, 2> buckets{};
		HistoryEntry factorizer{};

		[[nodiscard]] inline auto get(Bitboard threats, Move move) const -> i32
		{
			i32 value{};

			value += buckets[threats[move.src()]][threats[move.dst()]].get();
			value += factorizer.get();

			return value / 2;
		}

		inline auto update(Bitboard threats, Move move, HistoryScore bonus)
		{
			buckets[threats[move.src()]][threats[move.dst()]].update(bonus);
			factorizer.update(bonus);
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
		inline auto operator[](std::pair<Piece, Move> move) const -> const auto &
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
			std::memset(&m_butterfly   , 0, sizeof(m_butterfly   ));
			std::memset(&m_pieceTo     , 0, sizeof(m_pieceTo     ));
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
			butterflyEntry(threats, move).update(threats, move, bonus);
			pieceToEntry(threats, moving, move).update(threats, move, bonus);
			updateConthist(continuations, ply, moving, move, bonus);
		}

		inline auto updateNoisyScore(Move move, Piece captured, Bitboard threats, HistoryScore bonus)
		{
			noisyEntry(move, captured, threats[move.dst()]).update(bonus);
		}

		[[nodiscard]] inline auto quietScore(std::span<ContinuationSubtable *const> continuations,
			i32 ply, Bitboard threats, Piece moving, Move move) const -> i32
		{
			i32 score{};

			const auto butterfly = butterflyEntry(threats, move).get(threats, move);
			const auto pieceTo = pieceToEntry(threats, moving, move).get(threats, move);

			score += (butterfly + pieceTo) / 2;

			score += conthistScore(continuations, ply, moving, move, 1);
			score += conthistScore(continuations, ply, moving, move, 2);
			score += conthistScore(continuations, ply, moving, move, 4) / 2;

			return score;
		}

		[[nodiscard]] inline auto noisyScore(Move move, Piece captured, Bitboard threats) const -> i32
		{
			return noisyEntry(move, captured, threats[move.dst()]).get();
		}

	private:
		// [from][to][from attacked][to attacked]
		util::MultiArray<MainHistoryEntry, 64, 64> m_butterfly{};
		// [piece][to]
		util::MultiArray<MainHistoryEntry, 12, 64> m_pieceTo{};
		// [prev piece][to][curr piece type][to]
		util::MultiArray<ContinuationSubtable, 12, 64> m_continuation{};

		// [from][to][captured][defended]
		// additional slot for non-capture queen promos
		util::MultiArray<HistoryEntry, 64, 64, 13, 2> m_noisy{};

		static inline auto updateConthist(std::span<ContinuationSubtable *> continuations,
			i32 ply, Piece moving, Move move, HistoryScore bonus, i32 offset) -> void
		{
			if (offset <= ply)
				conthistEntry(continuations, ply, offset)[{moving, move}].update(bonus);
		}

		static inline auto conthistScore(std::span<ContinuationSubtable *const > continuations,
			i32 ply, Piece moving, Move move, i32 offset) -> HistoryScore
		{
			if (offset <= ply)
				return conthistEntry(continuations, ply, offset)[{moving, move}].get();

			return 0;
		}

		[[nodiscard]] inline auto butterflyEntry(Bitboard threats, Move move) const -> const MainHistoryEntry &
		{
			return m_butterfly[move.srcIdx()][move.dstIdx()];
		}

		[[nodiscard]] inline auto butterflyEntry(Bitboard threats, Move move) -> MainHistoryEntry &
		{
			return m_butterfly[move.srcIdx()][move.dstIdx()];
		}

		[[nodiscard]] inline auto pieceToEntry(Bitboard threats, Piece moving, Move move) const
			-> const MainHistoryEntry &
		{
			return m_pieceTo[static_cast<i32>(moving)][move.dstIdx()];
		}

		[[nodiscard]] inline auto pieceToEntry(Bitboard threats, Piece moving, Move move) -> MainHistoryEntry &
		{
			return m_pieceTo[static_cast<i32>(moving)][move.dstIdx()];
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

		[[nodiscard]] inline auto noisyEntry(Move move, Piece captured, bool defended) const -> const HistoryEntry &
		{
			return m_noisy[move.srcIdx()][move.dstIdx()][static_cast<i32>(captured)][defended];
		}

		[[nodiscard]] inline auto noisyEntry(Move move, Piece captured, bool defended) -> HistoryEntry &
		{
			return m_noisy[move.srcIdx()][move.dstIdx()][static_cast<i32>(captured)][defended];
		}
	};
}
