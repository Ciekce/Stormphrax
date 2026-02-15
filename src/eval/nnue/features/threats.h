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

#include "../../../types.h"

#include <optional>

#include "psq.h"

namespace stormphrax::eval::nnue::features::threats {
    constexpr u32 kTotalThreatFeatures = 66864;

    template <typename PsqFeatureSet>
    struct ThreatInputs : PsqFeatureSet {
        static constexpr bool kThreatInputs = true;
        static constexpr u32 kThreatFeatures = kTotalThreatFeatures;

        struct Updates : psq::PsqFeaturesBase::Updates {
            // [black, white]
            //std::array<bool, 2> refreshThreats{};
        };
    };

    [[nodiscard]] u32 featureIndex(
        Color c,
        Square king,
        Piece attacker,
        Square attackerSq,
        Piece attacked,
        Square attackedSq
    );
} // namespace stormphrax::eval::nnue::features::threats
