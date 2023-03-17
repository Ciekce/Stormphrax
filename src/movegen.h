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
#include "position.h"
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
		MoveGenerator(const Position &pos, MovegenData &data, Move hashMove)
			: m_pos{pos},
			  m_moves{data.moves},
			  m_hashMove{hashMove},
			  m_killer1{data.killer1},
			  m_killer2{data.killer2}
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
						if constexpr(Quiescence)
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

				const auto move = m_moves[m_idx++];

				if (!move.move)
					return NullMove;

				if (move.move != m_hashMove
					&& move.move != m_killer1
					&& move.move != m_killer2)
					return move.move;
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

		inline void scoreNoisy()
		{
			for (i32 i = m_idx; i < m_moves.size(); ++i)
			{
				//TODO see
				auto &move = m_moves[i];

				const auto srcValue = eval::pieceValue(m_pos.pieceAt(move.move.src())).midgame;
				// 0 for non-capture promo
				const auto dstValue = move.move.type() == MoveType::EnPassant
					? eval::values::Pawn.midgame
					: eval::pieceValue(m_pos.pieceAt(move.move.dst())).midgame;

				move.score = (dstValue - srcValue) * 2000 + dstValue;

				if (move.move.type() == MoveType::Promotion)
					move.score += PromoScores[move.move.targetIdx()] * 2000 * 2000;

				if (dstValue > 0 && !see::see(m_pos, move.move))
					move.score -= 8 * 2000 * 2000;
			}
		}

		inline void scoreQuiet()
		{
			for (i32 i = m_noisyEnd; i < m_moves.size(); ++i)
			{
				auto &move = m_moves[i];
				// knight promos first, rook then bishop promos last
				//TODO capture promos first
				if (move.move.type() == MoveType::Promotion)
					move.score = PromoScores[move.move.targetIdx()];
				// leave rest unsorted
			}
		}

		inline void genNoisy()
		{
			generateNoisy(m_moves, m_pos);
			scoreNoisy();

			std::sort(m_moves.begin() + m_idx, m_moves.end(), [](const auto &a, const auto &b)
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

			// also sorts bad noisy moves to the end
			std::sort(m_moves.begin() + m_idx, m_moves.end(), [](const auto &a, const auto &b)
			{
				return a.score > b.score;
			});

			m_goodNoisyEnd = 9999;
		}

		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		ScoredMoveList &m_moves;

		Move m_hashMove{};
		Move m_killer1{};
		Move m_killer2{};

		u32 m_idx{};

		u32 m_noisyEnd{};
		u32 m_goodNoisyEnd{};
	};

	using QMoveGenerator = MoveGenerator<true>;
}
