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
		static constexpr i32 Quiet = Killer + 1;
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

	template <bool Root, bool GoodNoisiesOnly = false>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, Move killer, MovegenData &data, Move ttMove,
			i32 ply = -1, std::span<const HistoryMove> prevMoves = {}, const HistoryTable *history = nullptr)
			: m_pos{pos},
			  m_data{data},
			  m_ttMove{ttMove},
			  m_ply{ply},
			  m_prevMoves{prevMoves},
			  m_killer{killer},
			  m_history{history}
		{
			if constexpr (Root)
			{
				static constexpr auto TtMoveScore = std::numeric_limits<i32>::max() - MovegenStage::TtMove;
				static constexpr auto KillerScore = GoodNoisyThreshold - MovegenStage::Killer;

				for (i32 i = 0; i < data.moves.size(); ++i)
				{
					auto &move = data.moves[i];

					if (move.move == m_ttMove)
					{
						move.score = TtMoveScore;
						data.histories[i] = moveHistory(move.move);
					}
					else if (move.move == m_killer)
					{
						move.score = KillerScore;
						data.histories[i] = moveHistory(move.move);
					}
					else if (m_pos.isNoisy(move.move))
						scoreNoisy(i);
					else scoreQuiet(i);
				}

				m_stage = MovegenStage::End;
			}
			else
			{
				m_data.moves.clear();
				m_data.moves.fill({NullMove, 0});
			}
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline auto next()
		{
			if constexpr (Root)
			{
				if (m_idx == m_data.moves.size())
					return MoveWithHistory{};

				const auto idx = findNext();
				return MoveWithHistory{m_data.moves[idx].move, m_data.histories[idx]};
			}

			while (true)
			{
				while (m_idx == m_data.moves.size() || m_idx == m_goodNoisyEnd)
				{
					++m_stage;

					switch (m_stage)
					{
					case MovegenStage::TtMove:
						if (m_ttMove)
							return MoveWithHistory{m_ttMove, moveHistory(m_ttMove)};
						break;

					case MovegenStage::GoodNoisy:
						genNoisy();
						if constexpr (GoodNoisiesOnly)
							m_stage = MovegenStage::End;
						break;

					case MovegenStage::Killer:
						if (m_killer
							&& m_killer != m_ttMove
							&& m_pos.isPseudolegal(m_killer))
							return MoveWithHistory{m_killer, moveHistory(m_killer)};
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
					&& move != m_killer)
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

		static constexpr i32 GoodNoisyBonus = 8 * 2000 * 2000;
		static constexpr i32 GoodNoisyThreshold = GoodNoisyBonus / 2;
		static constexpr i32 PromoMultiplier = 2000;

		inline auto moveHistory(Move move) -> i32
		{
			if constexpr (!GoodNoisiesOnly)
			{
				if (m_history)
				{
					const auto [noisy, captured] = m_pos.noisyCapturedPiece(move);
					const auto historyMove = HistoryMove::from(m_pos.boards(), move);

					return noisy
						? m_history->noisyScore(historyMove, m_pos.threats(), captured)
						: m_history->quietScore(historyMove, m_pos.threats(), m_ply, m_prevMoves);
				}
			}

			return 0;
		}

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

		inline auto genNoisy()
		{
			generateNoisy(m_data.moves, m_pos);

			for (i32 i = m_idx; i < m_data.moves.size(); ++i)
			{
				scoreNoisy(i);
			}

			//FIXME this does not sort histories, so they're broken
			std::stable_sort(m_data.moves.begin() + m_idx, m_data.moves.end(), [](const auto &a, const auto &b)
			{
				return a.score > b.score;
			});

			m_noisyEnd = m_data.moves.size();

			m_goodNoisyEnd = std::find_if(m_data.moves.begin() + m_idx, m_data.moves.end(), [](const auto &v)
			{
				return v.score < GoodNoisyThreshold;
			}) - m_data.moves.begin();
		}

		inline auto genQuiet()
		{
			generateQuiet(m_data.moves, m_pos);

			for (i32 i = m_noisyEnd; i < m_data.moves.size(); ++i)
			{
				scoreQuiet(i);
			}

			m_goodNoisyEnd = 9999;
		}

		inline auto scoreQuiet(i32 idx)
		{
			const auto &boards = m_pos.boards();

			auto &move = m_data.moves[idx];

			if (m_history)
			{
				const auto historyMove = HistoryMove::from(boards, move.move);
				const auto historyScore = m_history->quietScore(historyMove, m_pos.threats(), m_ply, m_prevMoves);
				m_data.histories[idx] = move.score = historyScore;
			}

			// knight promos first, rook then bishop promos last
			//TODO capture promos first
			if (move.move.type() == MoveType::Promotion)
				move.score += PromoScores[move.move.promoIdx()] * PromoMultiplier;
		}

		inline auto scoreNoisy(i32 idx)
		{
			const auto &boards = m_pos.boards();

			auto &move = m_data.moves[idx];

			const auto captured = move.move.type() == MoveType::EnPassant
				? colorPiece(PieceType::Pawn, m_pos.opponent())
				: boards.pieceAt(move.move.dst());

			if (m_history)
			{
				const auto historyMove = HistoryMove::from(boards, move.move);
				const auto historyScore = m_history->noisyScore(historyMove, m_pos.threats(), captured);
				m_data.histories[idx] = move.score = historyScore;
			}

			if (captured != Piece::None)
				move.score += Mvv[static_cast<i32>(pieceType(captured))];

			if ((captured != Piece::None || move.move.promo() == PieceType::Queen)
				&& see::see(m_pos, move.move))
				move.score += GoodNoisyBonus;
			else if (move.move.type() == MoveType::Promotion)
				move.score += PromoScores[move.move.promoIdx()] * PromoMultiplier;
		}

		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		MovegenData &m_data;

		Move m_ttMove;

		i32 m_ply;
		std::span<const HistoryMove> m_prevMoves;

		Move m_killer;

		const HistoryTable *m_history;

		u32 m_idx{};

		u32 m_noisyEnd{};
		u32 m_goodNoisyEnd{};
	};

	using QMoveGenerator = MoveGenerator<false, true>;
}
