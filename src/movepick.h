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
#include "history.h"

namespace stormphrax
{
	struct MovegenData
	{
		ScoredMoveList moves;
	};

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 TtMove = Start + 1;
		static constexpr i32 Noisy = TtMove + 1;
		static constexpr i32 Quiet = Noisy + 1;
		static constexpr i32 End = Quiet + 1;
	};

	template <bool NoisiesOnly>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, MovegenData &data, Move ttMove, const HistoryTables &history)
			: m_pos{pos},
			  m_data{data},
			  m_ttMove{ttMove},
			  m_history{history}
		{
			m_data.moves.clear();
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline auto next()
		{
			while (true)
			{
				while (m_idx == m_data.moves.size())
				{
					switch (++m_stage)
					{
						case MovegenStage::TtMove:
							if (m_ttMove && m_pos.isPseudolegal(m_ttMove))
								return m_ttMove;
							break;

						case MovegenStage::Noisy:
							generateNoisy(m_data.moves, m_pos);
							scoreNoisy();
							if constexpr (NoisiesOnly)
								m_stage = MovegenStage::End;
							break;

						case MovegenStage::Quiet:
							if (!m_skipQuiets)
							{
								generateQuiet(m_data.moves, m_pos);
								scoreQuiet();
							}
							break;

						default:
							return NullMove;
					}
				}

				assert(m_idx < m_data.moves.size());

				if (m_skipQuiets && m_stage >= MovegenStage::Quiet)
					return NullMove;

				const auto idx = findNext();
				const auto move = m_data.moves[idx].move;

				if (move != m_ttMove)
					return move;
			}
		}

		inline auto skipQuiets()
		{
			m_skipQuiets = true;
		}

		[[nodiscard]] inline auto stage() const { return m_stage; }

	private:
		inline auto scoreSingleNoisy(ScoredMove &scoredMove, const PositionBoards &boards)
		{
			const auto move = scoredMove.move;
			auto &score = scoredMove.score;

			const auto captured = m_pos.captureTarget(move);

			score += m_history.noisyScore(move, captured) / 8;
			score += see::value(captured);

			if (move.type() == MoveType::Promotion)
				score += see::value(PieceType::Queen) - see::value(PieceType::Pawn);
		}

		inline auto scoreNoisy() -> void
		{
			const auto &boards = m_pos.boards();
			for (u32 i = m_idx; i < m_data.moves.size(); ++i)
			{
				scoreSingleNoisy(m_data.moves[i], boards);
			}
		}

		inline auto scoreSingleQuiet(ScoredMove &move)
		{
			move.score = m_history.quietScore(move.move);
		}

		inline auto scoreQuiet() -> void
		{
			for (u32 i = m_idx; i < m_data.moves.size(); ++i)
			{
				scoreSingleQuiet(m_data.moves[i]);
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
					scoreSingleNoisy(move, boards);
				}
				else scoreSingleQuiet(move);
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

		bool m_skipQuiets{false};

		const HistoryTables &m_history;

		MovegenData &m_data;
		u32 m_idx{};

		Move m_ttMove;
	};

	[[nodiscard]] static inline auto mainMoveGenerator(const Position &pos, MovegenData &data,
		Move ttMove, const HistoryTables &history)
	{
		return MoveGenerator<false>(pos, data, ttMove, history);
	}

	[[nodiscard]] static inline auto qsearchMoveGenerator(const Position &pos,
		MovegenData &data, const HistoryTables &history)
	{
		return MoveGenerator<true>(pos, data, NullMove, history);
	}
}
