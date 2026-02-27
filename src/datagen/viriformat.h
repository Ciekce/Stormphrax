/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "common.h"
#include "format.h"
#include "marlinformat.h"

namespace stormphrax::datagen {
    // Format originally from Viridithas
    // https://github.com/cosmobobak/viridithas/blob/029672a/src/datagen/dataformat.rs
    class Viriformat {
    public:
        Viriformat();

        static constexpr auto kExtension = "vf";

        void start(const Position& initialPosition);
        void push(bool filtered, Move move, Score score);
        usize writeAllWithOutcome(std::ostream& stream, Outcome outcome);

    private:
        using ScoredMove = std::pair<u16, i16>;
        static_assert(sizeof(ScoredMove) == sizeof(u16) + sizeof(i16));

        marlinformat::PackedBoard m_initial{};
        std::vector<ScoredMove> m_moves{};
    };

    static_assert(OutputFormat<Viriformat>);
} // namespace stormphrax::datagen
