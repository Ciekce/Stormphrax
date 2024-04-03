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

#include "viri_binpack.h"

#include <array>

namespace stormphrax::datagen
{
	ViriBinpack::ViriBinpack()
	{
		m_moves.reserve(256);
	}

	auto ViriBinpack::start(const Position &initialPosition) -> void
	{
		m_initial = marlinformat::PackedBoard::pack(initialPosition, 0);
		m_moves.clear();
	}

	auto ViriBinpack::push([[maybe_unused]] bool filtered, Move move, Score score) -> void
	{
		static constexpr auto MoveTypes = std::array{
			static_cast<u16>(0x0000), // normal
			static_cast<u16>(0xC000), // promo
			static_cast<u16>(0x8000), // castling
			static_cast<u16>(0x4000)  // ep
		};

		u16 viriMove{};

		viriMove |= move.srcIdx();
		viriMove |= move.dstIdx() << 6;
		viriMove |= move.promoIdx() << 12;
		viriMove |= MoveTypes[static_cast<i32>(move.type())];

		m_moves.push_back({viriMove, static_cast<i16>(score)});
	}

	auto ViriBinpack::writeAllWithOutcome(std::ostream &stream, Outcome outcome) -> usize
	{
		static constexpr std::array<u8, sizeof(ScoredMove)> NullTerminator{};

		m_initial.wdl = outcome;

		stream.write(reinterpret_cast<const char *>(&m_initial), sizeof(marlinformat::PackedBoard));
		stream.write(reinterpret_cast<const char *>(m_moves.data()), sizeof(ScoredMove) * m_moves.size());
		stream.write(reinterpret_cast<const char *>(NullTerminator.data()), sizeof(ScoredMove));

		return m_moves.size() + 1;
	}
}
