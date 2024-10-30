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

#include "marlinformat.h"

namespace stormphrax::datagen {
    Marlinformat::Marlinformat() {
        m_positions.reserve(256);
    }

    auto Marlinformat::start(const Position& initialPosition) -> void {
        m_positions.clear();
        m_curr.copyStateFrom(initialPosition);
    }

    auto Marlinformat::push(bool filtered, Move move, Score score) -> void {
        if (!filtered)
            m_positions.push_back(marlinformat::PackedBoard::pack(m_curr, static_cast<i16>(score)));
        m_curr.applyMoveUnchecked<false, false>(move, nullptr);
    }

    auto Marlinformat::writeAllWithOutcome(std::ostream& stream, Outcome outcome) -> usize {
        for (auto& board: m_positions) {
            board.wdl = outcome;
        }

        stream.write(
            reinterpret_cast<const char*>(m_positions.data()),
            static_cast<std::streamsize>(m_positions.size() * sizeof(marlinformat::PackedBoard))
        );

        return m_positions.size();
    }
} // namespace stormphrax::datagen
