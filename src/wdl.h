/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

#include <cmath>
#include <utility>

#include "core.h"

namespace stormphrax::wdl {
    // Only used for unnormalisation, as a kind of best effort attempt
    // Normalisation goes through the wdl model so as to be independent of material
    constexpr Score kMaterial58NormalizationK = 276;

    [[nodiscard]] std::pair<f64, f64> wdlParams(i32 material);
    [[nodiscard]] std::pair<i32, i32> wdlModel(Score povScore, i32 material); // [win, loss]

    inline Score normalizeScore(Score score, [[maybe_unused]] i32 material) {
        return score;
    }

    inline Score unnormalizeScoreMaterial58(Score score) {
        return score;
    }
} // namespace stormphrax::wdl
