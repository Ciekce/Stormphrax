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

namespace polaris
{
	struct ScoredMove
	{
		Move move;
		i32 score;
	};

	using ScoredMoveList = StaticVector<ScoredMove, DefaultMoveListCapacity>;

	void generateNoisy(ScoredMoveList &noisy, const Position &pos);
	void generateQuiet(ScoredMoveList &quiet, const Position &pos);

	void generateAll(ScoredMoveList &dst, const Position &pos);

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 Hash = 1;
		static constexpr i32 Noisy = 2;
		static constexpr i32 Quiet = 3;
		static constexpr i32 End = 4;
	};

	template <bool GenerateQuiets = true>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, ScoredMoveList &moves, Move hashMove)
			: m_pos{pos},
			  m_moves{moves},
			  m_hashMove{hashMove}
		{
			m_moves.clear();
			m_moves.fill({NullMove, 0});
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline Move next()
		{
			while (true)
			{
				while (m_idx == m_moves.size())
				{
					++m_stage;

					switch (m_stage)
					{
					case MovegenStage::Hash:
						if (m_hashMove)
							return m_hashMove;
						break;

					case MovegenStage::Noisy:
						genNoisy();
						break;

					case MovegenStage::Quiet:
						if constexpr (GenerateQuiets)
						{
							genQuiet();
							break;
						}

					default:
						return NullMove;
					}
				}

				if (m_idx == m_moves.size())
					return NullMove;

			//	swapBest();
				const auto move = m_moves[m_idx++];

				if (!m_hashMove || move.move != m_hashMove)
					return move.move;
			}
		}

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
			}
		}

		inline void scoreQuiet()
		{
			for (i32 i = m_idx; i < m_moves.size(); ++i)
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
		}

		inline void genQuiet()
		{
			generateQuiet(m_moves, m_pos);
			scoreQuiet();
			std::sort(m_moves.begin() + m_idx, m_moves.end(), [](const auto &a, const auto &b)
			{
				return a.score > b.score;
			});
		}

		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		ScoredMoveList &m_moves;
		Move m_hashMove{};

		u32 m_idx{};
	};

	using QMoveGenerator = MoveGenerator<false>;
}
