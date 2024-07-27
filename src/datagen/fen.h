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

#include "../types.h"

#include <vector>

#include "format.h"
#include "../position/position.h"
#include "../util/u4array.h"

namespace stormphrax::datagen
{
	class Fen
	{
	public:
		Fen();
		~Fen() = default;

		static constexpr auto Extension = "txt";

		auto start(const Position &initialPosition) -> void;
		auto push(bool filtered, Move move, Score score) -> void;
		auto writeAllWithOutcome(std::ostream &stream, Outcome outcome) -> usize;

	private:
		std::vector<std::string> m_positions{};
		Position m_curr;
	};

	static_assert(OutputFormat<Fen>);
}
