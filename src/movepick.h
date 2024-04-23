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

	enum class MovegenType
	{
		Normal = 0,
		Qsearch
	};

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 TtMove = Start + 1;
		static constexpr i32 GoodNoisy = TtMove + 1;
		static constexpr i32 Killer = GoodNoisy + 1;
		static constexpr i32 Quiet = Killer + 1;
		static constexpr i32 BadNoisy = Quiet + 1;
		static constexpr i32 End = BadNoisy + 1;
	};

	template <MovegenType Type>
	class MoveGenerator
	{
		static constexpr bool NoisiesOnly = Type == MovegenType::Qsearch;

	public:
		MoveGenerator(const Position &pos, MovegenData &data, Move ttMove, const HistoryTables &history, Move killer)
			: m_pos{pos},
			  m_data{data},
			  m_ttMove{ttMove},
			  m_history{history},
			  m_killer{killer}
		{
			m_data.moves.clear();
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline auto next()
		{
			while (true)
			{
				while (m_idx == m_end)
				{
					switch (++m_stage)
					{
						case MovegenStage::TtMove:
							if (m_ttMove && m_pos.isPseudolegal(m_ttMove))
								return m_ttMove;
							break;

						case MovegenStage::GoodNoisy:
							generateNoisy(m_data.moves, m_pos);
							m_end = m_data.moves.size();
							scoreNoisy();
							break;

						case MovegenStage::Killer:
							if (!NoisiesOnly
								&& !m_skipQuiets
								&& !m_killer.isNull()
								&& m_killer != m_ttMove
								&& m_pos.isPseudolegal(m_killer))
								return m_killer;
							else
							{
								++m_stage;
								[[fallthrough]];
							}

						case MovegenStage::Quiet:
							if (!NoisiesOnly && !m_skipQuiets)
							{
								generateQuiet(m_data.moves, m_pos);
								m_end = m_data.moves.size();
								scoreQuiet();
								break;
							}
							else
							{
								++m_stage;
								[[fallthrough]];
							}

						case MovegenStage::BadNoisy:
							m_idx = 0;
							m_end = m_badNoisyEnd;
							break;

						default:
							return NullMove;
					}
				}

				assert(m_idx < m_end);

				if (m_skipQuiets && m_stage == MovegenStage::Quiet)
					return NullMove;

				if (Type != MovegenType::Qsearch
					&& m_stage == MovegenStage::GoodNoisy)
				{
					while (m_idx != m_end)
					{
						const auto idx = findNext();
						const auto move = m_data.moves[idx].move;

						if (move == m_ttMove)
							continue;

						if (!see::see(m_pos, move, 0))
							m_data.moves[m_badNoisyEnd++] = m_data.moves[idx];
						else return move;
					}
				}
				else
				{
					const auto idx = findNext();
					const auto move = m_data.moves[idx].move;

					if (move != m_ttMove)
						return move;
				}
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
			for (u32 i = m_idx; i < m_end; ++i)
			{
				scoreSingleNoisy(m_data.moves[i], boards);
			}
		}

		inline auto scoreSingleQuiet(ScoredMove &move)
		{
			move.score = m_history.quietScore(m_pos.threats(), move.move);
		}

		inline auto scoreQuiet() -> void
		{
			for (u32 i = m_idx; i < m_end; ++i)
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

			for (auto i = m_idx + 1; i < m_end; ++i)
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
		Move m_killer;

		u32 m_idx{};
		u32 m_end{};

		u32 m_badNoisyEnd{};

		Move m_ttMove;
	};

	[[nodiscard]] static inline auto mainMoveGenerator(const Position &pos, MovegenData &data,
		Move ttMove, const HistoryTables &history, Move killer)
	{
		return MoveGenerator<MovegenType::Normal>(pos, data, ttMove, history, killer);
	}

	[[nodiscard]] static inline auto qsearchMoveGenerator(const Position &pos,
		MovegenData &data, const HistoryTables &history)
	{
		return MoveGenerator<MovegenType::Qsearch>(pos, data, NullMove, history, NullMove);
	}
}
