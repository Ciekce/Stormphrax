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

	enum class MovegenStage : i32
	{
		TtMove = 0,
		GenNoisy,
		GoodNoisy,
		Killer1,
		Killer2,
		GenQuiet,
		Quiet,
		StartBadNoisy,
		BadNoisy,
		QsearchTtMove,
		QsearchGenNoisy,
		QsearchNoisy,
		ProbcutTtMove,
		ProbcutGenNoisy,
		ProbcutNoisy,
		End,
	};

	inline auto operator++(MovegenStage &v) -> auto &
	{
		v = static_cast<MovegenStage>(static_cast<i32>(v) + 1);
		return v;
	}

	inline auto operator<=>(MovegenStage a, MovegenStage b)
	{
		return static_cast<i32>(a) <=> static_cast<i32>(b);
	}

	class MoveGenerator
	{
	public:
		~MoveGenerator() = default;

		[[nodiscard]] inline auto next()
		{
			switch (m_stage)
			{
			case MovegenStage::TtMove:
			{
				++m_stage;

				if (m_ttMove && m_pos.isPseudolegal(m_ttMove))
					return m_ttMove;

				[[fallthrough]];
			}

			case MovegenStage::GenNoisy:
			{
				generateNoisy(m_data.moves, m_pos);
				m_end = m_data.moves.size();
				scoreNoisies();

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::GoodNoisy:
			{
				while (m_idx < m_end)
				{
					const auto idx = findNext();
					const auto move = m_data.moves[idx].move;

					if (move == m_ttMove)
						continue;

					if (!see::see(m_pos, move, 0))
						m_data.moves[m_badNoisyEnd++] = m_data.moves[idx];
					else return move;
				}

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::Killer1:
			{
				++m_stage;

				if (!m_skipQuiets
					&& m_killers.killer1
					&& m_killers.killer1 != m_ttMove
					&& m_pos.isPseudolegal(m_killers.killer1))
					return m_killers.killer1;

				[[fallthrough]];
			}

			case MovegenStage::Killer2:
			{
				++m_stage;

				if (!m_skipQuiets
					&& m_killers.killer2
					&& m_killers.killer2 != m_ttMove
					&& m_pos.isPseudolegal(m_killers.killer2))
					return m_killers.killer2;

				[[fallthrough]];
			}

			case MovegenStage::GenQuiet:
			{
				if (!m_skipQuiets)
				{
					generateQuiet(m_data.moves, m_pos);
					m_end = m_data.moves.size();
					scoreQuiets();
				}

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::Quiet:
			{
				if (!m_skipQuiets)
				{
					if (const auto move = selectNext([this](auto move) { return !isSpecial(move); }))
						return move;
				}

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::StartBadNoisy:
			{
				m_idx = 0;
				m_end = m_badNoisyEnd;

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::BadNoisy:
			{
				if (const auto move = selectNext([this](auto move) { return move != m_ttMove; }))
					return move;

				m_stage = MovegenStage::End;
				return NullMove;
			}

			case MovegenStage::QsearchTtMove:
			{
				++m_stage;

				if (m_ttMove && m_pos.isPseudolegal(m_ttMove))
					return m_ttMove;

				[[fallthrough]];
			}

			case MovegenStage::QsearchGenNoisy:
			{
				generateNoisy(m_data.moves, m_pos);
				m_end = m_data.moves.size();
				scoreNoisies();

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::QsearchNoisy:
			{
				if (const auto move = selectNext([this](auto move) { return move != m_ttMove; }))
					return move;

				m_stage = MovegenStage::End;
				return NullMove;
			}

			case MovegenStage::ProbcutTtMove:
			{
				++m_stage;

				if (m_ttMove && m_pos.isPseudolegal(m_ttMove))
					return m_ttMove;

				[[fallthrough]];
			}

			case MovegenStage::ProbcutGenNoisy:
			{
				generateNoisy(m_data.moves, m_pos);
				m_end = m_data.moves.size();
				scoreNoisies();

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::ProbcutNoisy:
			{
				if (const auto move = selectNext([this](auto move) { return move != m_ttMove; }))
					return move;

				m_stage = MovegenStage::End;
				return NullMove;
			}

			default: return NullMove;
			}
		}

		inline auto skipQuiets()
		{
			m_skipQuiets = true;
		}

		[[nodiscard]] inline auto stage() const
		{
			return m_stage;
		}

		[[nodiscard]] static inline auto main(const Position &pos, MovegenData &data,
			Move ttMove, const KillerTable &killers, const HistoryTables &history,
			std::span<ContinuationSubtable *const> continuations, i32 ply)
		{
			return MoveGenerator(MovegenStage::TtMove, pos, data, ttMove, &killers, history, continuations, ply);
		}

		[[nodiscard]] static inline auto qsearch(const Position &pos,
			MovegenData &data, Move ttMove, const HistoryTables &history)
		{
			return MoveGenerator(MovegenStage::QsearchTtMove, pos, data, ttMove, nullptr, history, {}, 0);
		}

		[[nodiscard]] static inline auto probcut(const Position &pos,
			Move ttMove, MovegenData &data, const HistoryTables &history)
		{
			return MoveGenerator(MovegenStage::ProbcutTtMove, pos, data, ttMove, nullptr, history, {}, 0);
		}

	private:
		MoveGenerator(MovegenStage initialStage, const Position &pos, MovegenData &data,
			Move ttMove, const KillerTable *killers, const HistoryTables &history,
			std::span<ContinuationSubtable *const> continuations, i32 ply)
			: m_stage{initialStage},
			  m_pos{pos},
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

		inline auto scoreNoisy(ScoredMove &scoredMove)
		{
			const auto move = scoredMove.move;
			auto &score = scoredMove.score;

			const auto captured = m_pos.captureTarget(move);

			score += m_history.noisyScore(move, captured) / 8;
			score += see::value(captured);

			if (move.type() == MoveType::Promotion)
				score += see::value(PieceType::Queen) - see::value(PieceType::Pawn);
		}

		inline auto scoreNoisies() -> void
		{
			for (u32 i = m_idx; i < m_end; ++i)
			{
				scoreNoisy(m_data.moves[i]);
			}
		}

		inline auto scoreQuiet(ScoredMove &move)
		{
			move.score = m_history.quietScore(m_continuations, m_ply,
				m_pos.threats(), m_pos.boards().pieceAt(move.move.src()), move.move);
		}

		inline auto scoreQuiets() -> void
		{
			for (u32 i = m_idx; i < m_end; ++i)
			{
				scoreQuiet(m_data.moves[i]);
			}
		}

		[[nodiscard]] inline auto findNext() -> u32
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

		[[nodiscard]] inline auto selectNext(auto predicate) -> Move
		{
			while (m_idx < m_end)
			{
				const auto idx = findNext();
				const auto move = m_data.moves[idx].move;

				if (predicate(move))
					return move;
			}

			return NullMove;
		}

		[[nodiscard]] inline auto isSpecial(Move move) -> bool
		{
			return move == m_ttMove
				|| move == m_killers.killer1
				|| move == m_killers.killer2;
		}

		MovegenStage m_stage;

		const Position &m_pos;
		MovegenData &m_data;

		Move m_ttMove;

		KillerTable m_killers{};
		const HistoryTables &m_history;

		std::span<ContinuationSubtable *const> m_continuations;
		i32 m_ply{};

		bool m_skipQuiets{false};

		u32 m_idx{};
		u32 m_end{};

		u32 m_badNoisyEnd{};
	};
}
