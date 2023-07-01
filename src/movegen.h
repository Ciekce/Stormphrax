/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

#include "move.h"
#include "position/position.h"
#include "see.h"
#include "history.h"

namespace stormphrax
{
	struct ScoredMove
	{
		Move move;
		i32 score;
	};

	using ScoredMoveList = StaticVector<ScoredMove, DefaultMoveListCapacity>;

	auto generateNoisy(ScoredMoveList &noisy, const Position &pos) -> void;
	auto generateQuiet(ScoredMoveList &quiet, const Position &pos) -> void;

	auto generateAll(ScoredMoveList &dst, const Position &pos) -> void;

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 TtMove = Start + 1;
		static constexpr i32 GoodNoisy = TtMove + 1;
		static constexpr i32 Killer = GoodNoisy + 1;
		static constexpr i32 Countermove = Killer + 1;
		static constexpr i32 Quiet = Countermove + 1;
		static constexpr i32 BadNoisy = Quiet + 1;
		static constexpr i32 End = BadNoisy + 1;
	};

	template <bool Quiescence = false>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, Move killer, ScoredMoveList &moves, Move ttMove,
			HistoryMove prevMove = {}, HistoryMove prevPrevMove = {}, const HistoryTable *history = nullptr)
			: m_pos{pos},
			  m_moves{moves},
			  m_ttMove{ttMove},
			  m_prevMove{prevMove},
			  m_prevPrevMove{prevPrevMove},
			  m_killer{killer},
			  m_history{history}
		{
			m_moves.clear();
			m_moves.fill({NullMove, 0});
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline auto next()
		{
			while (true)
			{
				while (m_idx == m_moves.size() || m_idx == m_goodNoisyEnd)
				{
					++m_stage;

					switch (m_stage)
					{
					case MovegenStage::TtMove:
						if (m_ttMove)
							return m_ttMove;
						break;

					case MovegenStage::GoodNoisy:
						genNoisy();
						if constexpr (Quiescence)
							m_stage = MovegenStage::End;
						break;

					case MovegenStage::Killer:
						if (m_killer
							&& m_killer != m_ttMove
							&& m_pos.isPseudolegal(m_killer))
							return m_killer;
						break;

					case MovegenStage::Countermove:
						if (m_history && m_prevMove)
						{
							m_countermove = m_history->entry(m_prevMove).countermove;
							if (m_countermove
								&& m_countermove != m_ttMove
								&& m_countermove != m_killer
								&& m_pos.isPseudolegal(m_countermove))
								return m_countermove;
						}
						break;

					case MovegenStage::Quiet:
						genQuiet();
						break;

					case MovegenStage::BadNoisy:
						break;

					default:
						return NullMove;
					}
				}

				if (m_idx == m_moves.size())
					return NullMove;

				const auto move = findNext();

				if (move != m_ttMove
					&& move != m_killer
					&& move != m_countermove)
					return move;
			}
		}

		[[nodiscard]] inline auto stage() const { return m_stage; }

	private:
		static constexpr auto Mvv = std::array {
			10, // pawn
			38, // knight
			40, // bishop
			50, // rook
			110 // queen
		};

		static constexpr auto PromoScores = std::array {
			-1, // knight
			-3, // bishop
			-2, // rook
			 0  // queen
		};

		inline auto findNext()
		{
			if (m_stage == MovegenStage::GoodNoisy)
				return m_moves[m_idx++].move;

			auto best = m_idx;
			auto bestScore = m_moves[m_idx].score;

			for (auto i = m_idx + 1; i < m_moves.size(); ++i)
			{
				if (m_moves[i].score > bestScore)
				{
					best = i;
					bestScore = m_moves[i].score;
				}
			}

			if (best != m_idx)
				std::swap(m_moves[m_idx], m_moves[best]);

			return m_moves[m_idx++].move;
		}

		inline auto scoreNoisy()
		{
			const auto &boards = m_pos.boards();

			for (i32 i = m_idx; i < m_moves.size(); ++i)
			{
				auto &move = m_moves[i];

				const auto captured = move.move.type() == MoveType::EnPassant
					? colorPiece(BasePiece::Pawn, m_pos.opponent())
					: boards.pieceAt(move.move.dst());

				if (m_history)
				{
					const auto historyMove = HistoryMove::from(boards, move.move);
					move.score = m_history->captureScore(historyMove, captured);
				}

				if (captured != Piece::None)
					move.score += Mvv[static_cast<i32>(basePiece(captured))];

				if ((captured != Piece::None || move.move.target() == BasePiece::Queen)
					&& see::see(m_pos, move.move))
					move.score += 8 * 2000 * 2000;
				else if (move.move.type() == MoveType::Promotion)
					move.score += PromoScores[move.move.targetIdx()] * 2000;
			}
		}

		inline auto scoreQuiet()
		{
			const auto &boards = m_pos.boards();

			for (i32 i = m_noisyEnd; i < m_moves.size(); ++i)
			{
				auto &move = m_moves[i];

				if (m_history)
				{
					const auto historyMove = HistoryMove::from(boards, move.move);

					move.score = m_history->entry(historyMove).score;

					if (m_prevMove)
						move.score += m_history->contEntry(m_prevMove).score(historyMove);
					if (m_prevPrevMove)
						move.score += m_history->contEntry(m_prevPrevMove).score(historyMove);
				}

				// knight promos first, rook then bishop promos last
				//TODO capture promos first
				if (move.move.type() == MoveType::Promotion)
					move.score += PromoScores[move.move.targetIdx()] * 2000;
			}
		}

		inline auto genNoisy()
		{
			generateNoisy(m_moves, m_pos);
			scoreNoisy();

			std::stable_sort(m_moves.begin() + m_idx, m_moves.end(), [](const auto &a, const auto &b)
			{
				return a.score > b.score;
			});

			m_noisyEnd = m_moves.size();

			m_goodNoisyEnd = std::find_if(m_moves.begin() + m_idx, m_moves.end(), [](const auto &v)
			{
				return v.score < 4 * 2000 * 2000;
			}) - m_moves.begin();
		}

		inline auto genQuiet()
		{
			generateQuiet(m_moves, m_pos);
			scoreQuiet();

			m_goodNoisyEnd = 9999;
		}

		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		ScoredMoveList &m_moves;

		Move m_ttMove;

		HistoryMove m_prevMove;
		HistoryMove m_prevPrevMove;

		Move m_killer;

		Move m_countermove{NullMove};

		const HistoryTable *m_history;

		u32 m_idx{};

		u32 m_noisyEnd{};
		u32 m_goodNoisyEnd{};
	};

	using QMoveGenerator = MoveGenerator<true>;
}
