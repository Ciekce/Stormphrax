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

#include "movegen.h"
#include "see.h"

namespace stormphrax
{
	struct MovegenData
	{
		ScoredMoveList moves;
	};

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 Noisy = Start + 1;
		static constexpr i32 Quiet = Noisy + 1;
		static constexpr i32 End = Quiet + 1;
	};

	template <bool Root, bool NoisiesOnly = false>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, MovegenData &data)
			: m_pos{pos},
			  m_data{data}
		{
			if constexpr (Root)
				scoreAll();
			else m_data.moves.clear();
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline auto next()
		{
			if constexpr (Root)
			{
				if (m_idx == m_data.moves.size())
					return NullMove;

				const auto idx = findNext();
				return m_data.moves[idx].move;
			}

			while (true)
			{
				while (m_idx == m_data.moves.size())
				{
					++m_stage;

					switch (m_stage)
					{
						case MovegenStage::Noisy:
							generateNoisy(m_data.moves, m_pos);
							scoreNoisies();
							if constexpr (NoisiesOnly)
								m_stage = MovegenStage::End;
							break;

						case MovegenStage::Quiet:
							generateQuiet(m_data.moves, m_pos);
							break;

						default:
							return NullMove;
					}
				}

				assert(m_idx < m_data.moves.size());

				const auto idx = findNext();
				return m_data.moves[idx].move;
			}
		}

		[[nodiscard]] inline auto stage() const { return m_stage; }

	private:
		inline auto scoreNoisy(const PositionBoards &boards, ScoredMove &scoredMove)
		{
			const auto move = scoredMove.move;
			auto &score = scoredMove.score;

			const auto moving = boards.pieceAt(move.src());
			score -= see::value(moving);

			const auto captured = move.type() == MoveType::EnPassant
				? PieceType::Pawn
				: pieceTypeOrNone(boards.pieceAt(move.dst()));

			score += see::value(captured) * 4000;
		}

		inline auto scoreNoisies() -> void
		{
			const auto &boards = m_pos.boards();
			for (u32 i = m_idx; i < m_data.moves.size(); ++i)
			{
				scoreNoisy(boards, m_data.moves[i]);
			}
		}

		inline auto scoreAll() -> void
		{
			const auto &boards = m_pos.boards();
			for (auto &move : m_data.moves)
			{
				move.score = 0;

				if (m_pos.isNoisy(move.move))
				{
					move.score += 16000000;
					scoreNoisy(boards, move);
				}
			}
		}

		inline auto findNext()
		{
			auto best = m_idx;
			auto bestScore = m_data.moves[m_idx].score;

			for (auto i = m_idx + 1; i < m_data.moves.size(); ++i)
			{
				if (m_data.moves[i].score > bestScore)
				{
					best = i;
					bestScore = m_data.moves[i].score;
				}
			}

			if (best != m_idx)
				std::swap(m_data.moves[m_idx], m_data.moves[best]);

			return m_idx++;
		}

		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		MovegenData &m_data;
		u32 m_idx{};
	};

	template <bool Root>
	[[nodiscard]] static inline auto mainMoveGenerator(const Position &pos, MovegenData &data)
	{
		return MoveGenerator<Root, false>(pos, data);
	}

	[[nodiscard]] static inline auto qsearchMoveGenerator(const Position &pos, MovegenData &data)
	{
		return MoveGenerator<false, true>(pos, data);
	}
}
