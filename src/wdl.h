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
    constexpr Score Material58NormalizationK = 259;

    [[nodiscard]] auto wdlParams(i32 material) -> std::pair<f64, f64>;
    [[nodiscard]] auto wdlModel(Score povScore, i32 material) -> std::pair<i32, i32>; // [win, loss]

    inline auto normalizeScore(Score score, i32 material) {
        // don't normalise wins/losses, or zeroes that are pointless to normalise
        if (score == 0 || std::abs(score) > ScoreWin) {
            return score;
        }

        const auto [a, b] = wdlParams(material);
        return static_cast<Score>(std::round(100.0 * static_cast<f64>(score) / a));
    }

    inline auto unnormalizeScoreMaterial58(Score score) {
        return score == 0 || std::abs(score) > ScoreWin ? score : score * Material58NormalizationK / 100;
    }
} // namespace stormphrax::wdl
