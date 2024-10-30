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

#include <concepts>
#include <ostream>
#include <string>

#include "../core.h"
#include "../move.h"
#include "../position/position.h"
#include "common.h"

namespace stormphrax::datagen {
    template<typename T>
    concept OutputFormat = requires(
        T t,
        const Position& initialPosition,
        bool filtered,
        Move move,
        Score score,
        Outcome outcome,
        std::ostream& stream
    ) {
        {
            T::Extension
        } -> std::convertible_to<const std::string&>;
        t.start(initialPosition);
        t.push(filtered, move, score);
        {
            t.writeAllWithOutcome(stream, outcome)
        } -> std::same_as<usize>;
    };
} // namespace stormphrax::datagen
