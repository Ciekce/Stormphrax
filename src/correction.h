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

#include <algorithm>
#include <cstring>

#include "core.h"
#include "position/position.h"
#include "util/multi_array.h"
#include "util/cemath.h"
#include "search_fwd.h"
#include "tunable.h"

namespace stormphrax
{
	class CorrectionHistoryTable
	{
	public:
		CorrectionHistoryTable() = default;
		~CorrectionHistoryTable() = default;

		inline auto clear()
		{
			std::memset(&m_pawnTable, 0, sizeof(m_pawnTable));
			std::memset(&m_blackNonPawnTable, 0, sizeof(m_blackNonPawnTable));
			std::memset(&m_whiteNonPawnTable, 0, sizeof(m_whiteNonPawnTable));
			std::memset(&m_majorTable, 0, sizeof(m_majorTable));
			std::memset(&m_minorTable, 0, sizeof(m_minorTable));
			std::memset(&m_contTable, 0, sizeof(m_contTable));
		}

		inline auto update(const Position &pos, std::span<search::PlayedMove> moves,
			i32 ply, i32 depth, Score searchScore, Score staticEval)
		{
			const auto scaledError = static_cast<i32>((searchScore - staticEval) * Grain);
			const auto newWeight = static_cast<i32>(std::min(depth + 1, 16));

			const auto stm = static_cast<i32>(pos.toMove());

			m_pawnTable[stm][pos.pawnKey() % Entries].update(scaledError, newWeight);
			m_blackNonPawnTable[stm][pos.blackNonPawnKey() % Entries].update(scaledError, newWeight);
			m_whiteNonPawnTable[stm][pos.whiteNonPawnKey() % Entries].update(scaledError, newWeight);
			m_majorTable[stm][pos.majorKey() % Entries].update(scaledError, newWeight);
			m_minorTable[stm][pos.majorKey() % Entries].update(scaledError, newWeight);

			if (ply >= 2)
			{
				const auto [moving2, dst2] = moves[ply - 2];
				const auto [moving1, dst1] = moves[ply - 1];

				if (moving2 != Piece::None && moving1 != Piece::None)
					m_contTable[stm][static_cast<i32>(pieceType(moving2))][static_cast<i32>(dst2)]
						[static_cast<i32>(pieceType(moving1))][static_cast<i32>(dst1)].update(scaledError, newWeight);
			}
		}

		[[nodiscard]] inline auto correct(const Position &pos,
			std::span<search::PlayedMove> moves, i32 ply, Score score) const
		{
			using namespace tunable;

			const auto stm = static_cast<i32>(pos.toMove());

			const auto [blackNpWeight, whiteNpWeight] = pos.toMove() == Color::Black
				? std::pair{ stmNonPawnCorrhistWeight(), nstmNonPawnCorrhistWeight()}
				: std::pair{nstmNonPawnCorrhistWeight(),  stmNonPawnCorrhistWeight()};

			i32 correction{};

			correction += pawnCorrhistWeight() * m_pawnTable[stm][pos.pawnKey() % Entries];
			correction += blackNpWeight * m_blackNonPawnTable[stm][pos.blackNonPawnKey() % Entries];
			correction += whiteNpWeight * m_whiteNonPawnTable[stm][pos.whiteNonPawnKey() % Entries];
			correction += majorCorrhistWeight() * m_majorTable[stm][pos.majorKey() % Entries];
			correction += majorCorrhistWeight() * m_majorTable[stm][pos.majorKey() % Entries];

			if (ply >= 2)
			{
				const auto [moving2, dst2] = moves[ply - 2];
				const auto [moving1, dst1] = moves[ply - 1];

				if (moving2 != Piece::None && moving1 != Piece::None)
					correction += contCorrhistWeight() * m_contTable[stm]
						[static_cast<i32>(pieceType(moving2))][static_cast<i32>(dst2)]
						[static_cast<i32>(pieceType(moving1))][static_cast<i32>(dst1)];
			}

			score += correction / 128;

			return std::clamp(score, -ScoreWin + 1, ScoreWin - 1);
		}

	private:
		static constexpr usize Entries = 16384;

		static constexpr i32 Grain = 256;
		static constexpr i32 WeightScale = 256;
		static constexpr i32 Max = Grain * 32;

		struct Entry
		{
			i16 value{};

			inline auto update(i32 scaledError, i32 newWeight) -> void
			{
				const auto v = util::ilerp<WeightScale>(value, scaledError, newWeight);
				value = static_cast<i16>(std::clamp(v, -Max, Max));
			}

			[[nodiscard]] inline operator i32() const
			{
				return value / Grain;
			}
		};

		util::MultiArray<Entry, 2, Entries> m_pawnTable{};
		util::MultiArray<Entry, 2, Entries> m_blackNonPawnTable{};
		util::MultiArray<Entry, 2, Entries> m_whiteNonPawnTable{};
		util::MultiArray<Entry, 2, Entries> m_majorTable{};
		util::MultiArray<Entry, 2, Entries> m_minorTable{};
		util::MultiArray<Entry, 2, 6, 64, 6, 64> m_contTable{};
	};
}
