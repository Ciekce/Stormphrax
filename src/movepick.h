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
	struct KillerTable
	{
		Move killer1{};
		Move killer2{};

		inline auto push(Move move)
		{
			if (move != killer1)
			{
				killer2 = killer1;
				killer1 = move;
			}
		}

		inline auto clear()
		{
			killer1 = NullMove;
			killer2 = NullMove;
		}
	};

	struct MovegenData
	{
		ScoredMoveList moves;
	};

	enum class MovegenType
	{
		Normal = 0,
		Qsearch,
		Probcut
	};

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 TtMove = Start + 1;
		static constexpr i32 GoodNoisy = TtMove + 1;
		static constexpr i32 Killer1 = GoodNoisy + 1;
		static constexpr i32 Killer2 = Killer1 + 1;
		static constexpr i32 Quiet = Killer2 + 1;
		static constexpr i32 BadNoisy = Quiet + 1;
		static constexpr i32 End = BadNoisy + 1;
	};

	template <MovegenType Type>
	class MoveGenerator
	{
		static constexpr bool NoisiesOnly = Type == MovegenType::Qsearch || Type == MovegenType::Probcut;

	public:
		MoveGenerator(const Position &pos, MovegenData &data, Move ttMove, const KillerTable *killers,
			const HistoryTables &history, std::span<ContinuationSubtable *const> continuations, i32 ply)
			: m_pos{pos},
			  m_data{data},
			  m_ttMove{ttMove},
			  m_history{history},
			  m_continuations{continuations},
			  m_ply{ply}
		{
			if (killers)
				m_killers = *killers;
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

						case MovegenStage::Killer1:
							if (!NoisiesOnly && !m_skipQuiets
								&& !m_killers.killer1.isNull()
								&& m_killers.killer1 != m_ttMove
								&& m_pos.isPseudolegal(m_killers.killer1))
								return m_killers.killer1;
							else continue;

						case MovegenStage::Killer2:
							if (!NoisiesOnly && !m_skipQuiets
								&& !m_killers.killer2.isNull()
								&& m_killers.killer2 != m_ttMove
								&& m_pos.isPseudolegal(m_killers.killer2))
								return m_killers.killer2;
							else continue;

						case MovegenStage::Quiet:
							if (!NoisiesOnly && !m_skipQuiets)
							{
								generateQuiet(m_data.moves, m_pos);
								m_end = m_data.moves.size();
								scoreQuiet();
							}
							else
							{
								++m_stage;
								continue;
							}
							break;

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
				{
					m_idx = m_end;
					continue;
				}

				if (m_stage == MovegenStage::GoodNoisy)
				{
					while (m_idx != m_end)
					{
						const auto idx = findNext();
						const auto move = m_data.moves[idx].move;

						if constexpr (Type == MovegenType::Qsearch)
							return move;
						else
						{
							if (move == m_ttMove)
								continue;

							if (Type == MovegenType::Normal && !see::see(m_pos, move, 0))
								m_data.moves[m_badNoisyEnd++] = m_data.moves[idx];
							else return move;
						}
					}
				}
				else
				{
					const auto idx = findNext();
					const auto move = m_data.moves[idx].move;

					if (move != m_ttMove && move != m_killers.killer1 && move != m_killers.killer2)
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
		inline auto scoreSingleNoisy(ScoredMove &scoredMove)
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
			for (u32 i = m_idx; i < m_end; ++i)
			{
				scoreSingleNoisy(m_data.moves[i]);
			}
		}

		inline auto scoreSingleQuiet(ScoredMove &move)
		{
			move.score = m_history.quietScore(m_continuations, m_ply,
				m_pos.threats(), m_pos.boards().pieceAt(move.move.src()), move.move);
		}

		inline auto scoreQuiet() -> void
		{
			for (u32 i = m_idx; i < m_end; ++i)
			{
				scoreSingleQuiet(m_data.moves[i]);
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

		std::span<ContinuationSubtable *const> m_continuations;
		i32 m_ply{};

		MovegenData &m_data;
		KillerTable m_killers{};

		u32 m_idx{};
		u32 m_end{};

		u32 m_badNoisyEnd{};

		Move m_ttMove;
	};

	[[nodiscard]] static inline auto mainMoveGenerator(const Position &pos, MovegenData &data,
		Move ttMove, const KillerTable &killers, const HistoryTables &history,
		std::span<ContinuationSubtable *const> continuations, i32 ply)
	{
		return MoveGenerator<MovegenType::Normal>(pos, data, ttMove, &killers, history, continuations, ply);
	}

	[[nodiscard]] static inline auto qsearchMoveGenerator(const Position &pos,
		MovegenData &data, const HistoryTables &history)
	{
		return MoveGenerator<MovegenType::Qsearch>(pos, data, NullMove, nullptr, history, {}, 0);
	}

	[[nodiscard]] static inline auto probcutMoveGenerator(const Position &pos,
		Move ttMove, MovegenData &data, const HistoryTables &history)
	{
		return MoveGenerator<MovegenType::Probcut>(pos, data, ttMove, nullptr, history, {}, 0);
	}
}
