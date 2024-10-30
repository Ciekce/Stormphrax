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

#include "fen.h"

namespace stormphrax::datagen {
    Fen::Fen() {
        m_positions.reserve(256);
    }

    auto Fen::start(const Position& initialPosition) -> void {
        m_positions.clear();
        m_curr.copyStateFrom(initialPosition);
    }

    auto Fen::push(bool filtered, Move move, Score score) -> void {
        if (!filtered)
            m_positions.push_back(m_curr.toFen() + " | " + std::to_string(score));
        m_curr.applyMoveUnchecked<false, false>(move, nullptr);
    }

    auto Fen::writeAllWithOutcome(std::ostream& stream, Outcome outcome) -> usize {
        for (auto& fen: m_positions) {
            stream << fen << " | ";

            switch (outcome) {
                case Outcome::WhiteLoss:
                    stream << "0.0";
                    break;
                case Outcome::Draw:
                    stream << "0.5";
                    break;
                case Outcome::WhiteWin:
                    stream << "1.0";
                    break;
            }

            stream << '\n';
        }

        return m_positions.size();
    }
} // namespace stormphrax::datagen
