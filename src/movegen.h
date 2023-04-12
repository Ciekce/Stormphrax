/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"

#include "move.h"
#include "position/position.h"
#include "eval/material.h"
#include "see.h"

namespace polaris
{
	struct ScoredMove
	{
		Move move;
		i32 score;
	};

	using ScoredMoveList = StaticVector<ScoredMove, DefaultMoveListCapacity>;

	struct MovegenData
	{
		ScoredMoveList moves{};

		Move killer1{NullMove};
		Move killer2{NullMove};
	};

	using HistoryTable = std::array<std::array<i32, 64>, 12>;

	void generateNoisy(ScoredMoveList &noisy, const Position &pos);
	void generateQuiet(ScoredMoveList &quiet, const Position &pos);

	void generateAll(ScoredMoveList &dst, const Position &pos);

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 Hash = 1;
		static constexpr i32 GoodNoisy = 2;
		static constexpr i32 Killer1 = 3;
		static constexpr i32 Killer2 = 4;
		static constexpr i32 Quiet = 5;
		static constexpr i32 BadNoisy = 6;
		static constexpr i32 End = 7;
	};

	template <bool Quiescence = false>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, MovegenData &data, Move hashMove, const HistoryTable *history = nullptr)
			: m_pos{pos},
			  m_moves{data.moves},
			  m_hashMove{hashMove},
			  m_killer1{data.killer1},
			  m_killer2{data.killer2},
			  m_history{history}
		{
			m_moves.clear();
			m_moves.fill({NullMove, 0});
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline Move next()
		{
			while (true)
			{
				while (m_idx == m_moves.size() || m_idx == m_goodNoisyEnd)
				{
					++m_stage;

					switch (m_stage)
					{
					case MovegenStage::Hash:
						if (m_hashMove)
							return m_hashMove;
						break;

					case MovegenStage::GoodNoisy:
						genNoisy();
						if constexpr (Quiescence)
							m_stage = MovegenStage::End;
						break;

					case MovegenStage::Killer1:
						if (m_killer1
							&& m_killer1 != m_hashMove
							&& m_pos.isPseudolegal(m_killer1))
							return m_killer1;
						break;

					case MovegenStage::Killer2:
						if (m_killer2
							&& m_killer2 != m_hashMove
							&& m_pos.isPseudolegal(m_killer2))
							return m_killer2;
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

				if (move != m_hashMove
					&& move != m_killer1
					&& move != m_killer2)
					return move;
			}
		}

		[[nodiscard]] inline auto stage() const { return m_stage; }

	private:
		static constexpr auto PromoScores = std::array {
			 1, // knight
			-2, // bishop
			-1, // rook
			 2  // queen
		};

		inline Move findNext()
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

		inline void scoreNoisy()
		{
			const auto &boards = m_pos.boards();

			for (i32 i = m_idx; i < m_moves.size(); ++i)
			{
				//TODO see
				auto &move = m_moves[i];

				const auto srcValue = eval::pieceValue(boards.pieceAt(move.move.src())).midgame;
				// 0 for non-capture promo
				const auto dstValue = move.move.type() == MoveType::EnPassant
					? eval::values::Pawn.midgame
					: eval::pieceValue(boards.pieceAt(move.move.dst())).midgame;

				move.score = (dstValue - srcValue) * 2000 + dstValue;

				if (move.move.type() == MoveType::Promotion)
					move.score += PromoScores[move.move.targetIdx()] * 2000 * 2000;

				if (dstValue > 0 && !see::see(m_pos, move.move))
					move.score -= 8 * 2000 * 2000;
			}
		}

		inline void scoreQuiet()
		{
			const auto &boards = m_pos.boards();

			for (i32 i = m_noisyEnd; i < m_moves.size(); ++i)
			{
				auto &move = m_moves[i];

				if (m_history)
					move.score = (*m_history)[static_cast<i32>(boards.pieceAt(move.move.src()))]
						[static_cast<i32>(moveActualDst(move.move))];

				// knight promos first, rook then bishop promos last
				//TODO capture promos first
				if (move.move.type() == MoveType::Promotion)
					move.score += PromoScores[move.move.targetIdx()] * 2000;
			}
		}

		inline void genNoisy()
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
				return v.score < -4 * 2000 * 2000;
			}) - m_moves.begin();
		}

		inline void genQuiet()
		{
			generateQuiet(m_moves, m_pos);
			scoreQuiet();

			m_goodNoisyEnd = 9999;
		}

		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		ScoredMoveList &m_moves;

		Move m_hashMove{};
		Move m_killer1{};
		Move m_killer2{};

		const HistoryTable *m_history{};

		u32 m_idx{};

		u32 m_noisyEnd{};
		u32 m_goodNoisyEnd{};
	};

	using QMoveGenerator = MoveGenerator<true>;
}
