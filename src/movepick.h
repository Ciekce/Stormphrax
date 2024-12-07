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
#include "tunable.h"

namespace stormphrax
{
	struct KillerTable
	{
		Move killer{};

		inline auto push(Move move)
		{
			killer = move;
		}

		inline auto clear()
		{
			killer = NullMove;
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
		Killer,
		GenQuiet,
		Quiet,
		StartBadNoisy,
		BadNoisy,
		QsearchTtMove,
		QsearchGenNoisy,
		QsearchNoisy,
		QsearchEvasionsTtMove,
		QsearchEvasionsGenNoisy,
		QsearchEvasionsNoisy,
		QsearchEvasionsGenQuiet,
		QsearchEvasionsQuiet,
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
			using namespace tunable;

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
					const auto [move, score] = m_data.moves[idx];

					if (move == m_ttMove)
						continue;

					const auto threshold = -score / 4 + goodNoisySeeOffset();

					if (!see::see(m_pos, move, threshold))
						m_data.moves[m_badNoisyEnd++] = m_data.moves[idx];
					else return move;
				}

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::Killer:
			{
				++m_stage;

				if (!m_skipQuiets
					&& m_killers.killer
					&& m_killers.killer != m_ttMove
					&& m_pos.isPseudolegal(m_killers.killer))
					return m_killers.killer;

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

			case MovegenStage::QsearchEvasionsTtMove:
			{
				++m_stage;

				if (m_ttMove && m_pos.isPseudolegal(m_ttMove))
					return m_ttMove;

				[[fallthrough]];
			}

			case MovegenStage::QsearchEvasionsGenNoisy:
			{
				generateNoisy(m_data.moves, m_pos);
				m_end = m_data.moves.size();
				scoreNoisies();

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::QsearchEvasionsNoisy:
			{
				if (const auto move = selectNext([this](auto move) { return move != m_ttMove; }))
					return move;

				++m_stage;
				[[fallthrough]];
			}

			case MovegenStage::QsearchEvasionsGenQuiet:
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

			case MovegenStage::QsearchEvasionsQuiet:
			{
				if (!m_skipQuiets)
				{
					if (const auto move = selectNext([this](auto move) { return move != m_ttMove; }))
						return move;
				}

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
			MovegenData &data, Move ttMove, const HistoryTables &history,
			std::span<ContinuationSubtable *const> continuations, i32 ply)
		{
			const auto stage = pos.isCheck() ? MovegenStage::QsearchEvasionsTtMove : MovegenStage::QsearchTtMove;
			return MoveGenerator(stage, pos, data, ttMove, nullptr, history, continuations, ply);
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

			score += m_history.noisyScore(move, captured, m_pos.threats()) / 8;
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
				|| move == m_killers.killer;
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
