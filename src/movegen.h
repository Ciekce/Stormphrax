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

	struct MovegenData
	{
		ScoredMoveList moves;
		std::array<i32, DefaultMoveListCapacity> histories;
	};

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

	struct MoveWithHistory
	{
		Move move{NullMove};
		i32 history{0};

		[[nodiscard]] inline explicit operator bool() const
		{
			return !move.isNull();
		}
	};

	template <bool Quiescence = false>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, Move killer, MovegenData &data, Move ttMove, i32 seeThreshold = 0,
			i32 ply = -1, std::span<const HistoryMove> prevMoves = {}, const HistoryTable *history = nullptr)
			: m_pos{pos},
			  m_data{data},
			  m_ttMove{ttMove},
			  m_seeThreshold{seeThreshold},
			  m_ply{ply},
			  m_prevMoves{prevMoves},
			  m_killer{killer},
			  m_history{history}
		{
			m_data.moves.clear();
			m_data.moves.fill({NullMove, 0});
		}

		~MoveGenerator() = default;

		// Note that this does *not* return history scores for TT
		// or refutation moves (killers, countermoves), as they are
		// not needed and returning them increases complexity a lot
		[[nodiscard]] inline auto next()
		{
			while (true)
			{
				while (m_idx == m_data.moves.size() || m_idx == m_goodNoisyEnd)
				{
					++m_stage;

					switch (m_stage)
					{
					case MovegenStage::TtMove:
						if (m_ttMove)
							return MoveWithHistory{m_ttMove};
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
							return MoveWithHistory{m_killer};
						break;

					case MovegenStage::Countermove:
						if (m_history && m_ply > 0)
						{
							m_countermove = m_history->countermove(m_ply, m_prevMoves);
							if (m_countermove
								&& m_countermove != m_ttMove
								&& m_countermove != m_killer
								&& m_pos.isPseudolegal(m_countermove))
								return MoveWithHistory{m_countermove};
						}
						break;

					case MovegenStage::Quiet:
						genQuiet();
						break;

					case MovegenStage::BadNoisy:
						break;

					default:
						return MoveWithHistory{};
					}
				}

				if (m_idx == m_data.moves.size())
					return MoveWithHistory{};

				const auto idx = findNext();

				const auto move = m_data.moves[idx].move;

				if (move != m_ttMove
					&& move != m_killer
					&& move != m_countermove)
					return MoveWithHistory{move, m_data.histories[idx]};
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
			// good noisies are already sorted
			if (m_stage == MovegenStage::GoodNoisy)
				return m_idx++;

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
			{
				std::swap(m_data.moves[m_idx], m_data.moves[best]);
				std::swap(m_data.histories[m_idx], m_data.histories[best]);
			}

			return m_idx++;
		}

		inline auto scoreNoisy()
		{
			const auto &boards = m_pos.boards();

			for (i32 i = m_idx; i < m_data.moves.size(); ++i)
			{
				auto &move = m_data.moves[i];

				const auto captured = move.move.type() == MoveType::EnPassant
					? colorPiece(PieceType::Pawn, m_pos.opponent())
					: boards.pieceAt(move.move.dst());

				if (m_history)
				{
					const auto historyMove = HistoryMove::from(boards, move.move);
					const auto historyScore = m_history->noisyScore(historyMove, m_pos.threats(), captured);
					m_data.histories[i] = move.score = historyScore;
				}

				if (captured != Piece::None)
					move.score += Mvv[static_cast<i32>(pieceType(captured))];

				if ((captured != Piece::None || move.move.target() == PieceType::Queen)
					&& see::see(m_pos, move.move, m_seeThreshold))
					move.score += 8 * 2000 * 2000;
				else if (move.move.type() == MoveType::Promotion)
					move.score += PromoScores[move.move.targetIdx()] * 2000;
			}
		}

		inline auto scoreQuiet()
		{
			const auto &boards = m_pos.boards();

			for (i32 i = m_noisyEnd; i < m_data.moves.size(); ++i)
			{
				auto &move = m_data.moves[i];

				if (m_history)
				{
					const auto historyMove = HistoryMove::from(boards, move.move);
					const auto historyScore = m_history->quietScore(historyMove, m_pos.threats(), m_ply, m_prevMoves);
					m_data.histories[i] = move.score = historyScore;
				}

				// knight promos first, rook then bishop promos last
				//TODO capture promos first
				if (move.move.type() == MoveType::Promotion)
					move.score += PromoScores[move.move.targetIdx()] * 2000;
			}
		}

		inline auto genNoisy()
		{
			generateNoisy(m_data.moves, m_pos);
			scoreNoisy();

			std::stable_sort(m_data.moves.begin() + m_idx, m_data.moves.end(), [](const auto &a, const auto &b)
			{
				return a.score > b.score;
			});

			m_noisyEnd = m_data.moves.size();

			m_goodNoisyEnd = std::find_if(m_data.moves.begin() + m_idx, m_data.moves.end(), [](const auto &v)
			{
				return v.score < 4 * 2000 * 2000;
			}) - m_data.moves.begin();
		}

		inline auto genQuiet()
		{
			generateQuiet(m_data.moves, m_pos);
			scoreQuiet();

			m_goodNoisyEnd = 9999;
		}

		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		MovegenData &m_data;

		Move m_ttMove;

		i32 m_seeThreshold;

		i32 m_ply;
		std::span<const HistoryMove> m_prevMoves;

		Move m_killer;

		Move m_countermove{NullMove};

		const HistoryTable *m_history;

		u32 m_idx{};

		u32 m_noisyEnd{};
		u32 m_goodNoisyEnd{};
	};

	using QMoveGenerator = MoveGenerator<true>;
	using PcMoveGenerator = MoveGenerator<true>; // probcut
}
