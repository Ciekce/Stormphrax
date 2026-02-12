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

#include "types.h"

#include "core.h"
#include "pv.h"

namespace stormphrax::search {
    struct RootMove {
        Score score{-kScoreInf};
        Score windowScore{-kScoreInf};

        Score displayScore{-kScoreInf};
        Score previousScore{-kScoreInf};

        bool upperbound{false};
        bool lowerbound{false};

        i32 seldepth{};
        PvList pv{};

        usize nodes{};
    };
} // namespace stormphrax::search
