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

namespace polaris
{
	void generateNoisy(MoveList &noisy, const Position &pos);
	void generateQuiet(MoveList &quiet, const Position &pos);

	void generateAll(MoveList &dst, const Position &pos);

	void orderMoves(MoveList &moves, const Position &pos, Move first = Move::null());

	struct MovegenStage
	{
		static constexpr i32 Start = 0;
		static constexpr i32 Hash = 1;
		static constexpr i32 Noisy = 2;
		static constexpr i32 Quiet = 3;
		static constexpr i32 End = 4;
	};

	/*
	template <bool GenerateQuiets = true>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, Move hashMove)
			: m_pos{pos},
			  m_hashMove{hashMove} {}

		~MoveGenerator() = default;

		[[nodiscard]] inline Move next()
		{
			while (m_idx >= m_data.size())
			{
				++m_stage;

				m_idx = m_data.size();

				switch (m_stage)
				{
				case MovegenStage::Hash:
					if (m_hashMove)
						return m_hashMove;
					break;

				case MovegenStage::Noisy:
					generateNoisy(m_data, m_pos);
					orderMoves(m_data, m_pos);
					break;

				case MovegenStage::Quiet:
					if constexpr(GenerateQuiets)
						generateQuiet(m_data, m_pos);
					break;

				default: return Move::null();
				}

				if (m_idx < m_data.size() && m_data[m_idx] == m_hashMove)
					++m_idx;
			}

			auto move = m_data[m_idx++];

			if (m_hashMove && move == m_hashMove)
				move = m_data[m_idx++];

			return move;
		}

	private:
		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		Move m_hashMove{};

		u32 m_idx{};
		MoveList m_data{};
	};
	 */

	template <bool GenerateQuiets = true>
	class MoveGenerator
	{
	public:
		MoveGenerator(const Position &pos, MoveList &moves, Move hashMove)
			: m_pos{pos},
			  m_moves{moves},
			  m_hashMove{hashMove}
		{
			m_moves.clear();
			m_moves.fill(Move::null());
		}

		~MoveGenerator() = default;

		[[nodiscard]] inline Move next()
		{
			while (m_idx >= m_moves.size())
			{
				++m_stage;

				m_idx = m_moves.size();

				switch (m_stage)
				{
				case MovegenStage::Hash:
					if (m_hashMove)
						return m_hashMove;
					break;

				case MovegenStage::Noisy:
					generateNoisy(m_moves, m_pos);
					if constexpr(GenerateQuiets)
						generateQuiet(m_moves, m_pos);
					orderMoves(m_moves, m_pos, m_hashMove);
					break;

				default: return Move::null();
				}
			}

			auto move = m_moves[m_idx++];

			if (m_hashMove && move == m_hashMove)
				move = m_moves[m_idx++];

			return move;
		}

	private:
		const Position &m_pos;

		i32 m_stage{MovegenStage::Start};

		MoveList &m_moves;
		Move m_hashMove{};

		u32 m_idx{};
	};

#if 0
	template <bool GenerateQuiets = true>
	class KnownGoodMoveGenerator
	{
	public:
		KnownGoodMoveGenerator(const Position &pos, Move hashMove)
		{
			generateNoisy(m_data, pos);

			if constexpr(GenerateQuiets)
				generateQuiet(m_data, pos);

			orderMoves(m_data, pos, hashMove);
		}

		~KnownGoodMoveGenerator() = default;

		[[nodiscard]] inline Move next()
		{
			return m_data[m_idx++];
		}

	private:
		u32 m_idx{};
		MoveList m_data{};
	};
#endif

	using QMoveGenerator = MoveGenerator<false>;
}
