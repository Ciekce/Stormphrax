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

#include <span>

#include "psq.h"

namespace stormphrax::eval::nnue::features::threats {
    constexpr u32 kTotalThreatFeatures = 60144;

    // just in case
    constexpr usize kMaxThreatsAdded = 128;
    constexpr usize kMaxThreatsRemoved = 128;

    using AddedThreatList = StaticVector<psq::UpdatedThreat, kMaxThreatsAdded>;
    using RemovedThreatList = StaticVector<psq::UpdatedThreat, kMaxThreatsRemoved>;

    template <typename PsqFeatureSet>
    struct ThreatInputs : PsqFeatureSet {
        static constexpr bool kThreatInputs = true;
        static constexpr u32 kThreatFeatures = kTotalThreatFeatures;

        struct Updates : psq::PsqFeaturesBase::Updates {
            // black, white
            std::array<bool, 2> refreshThreats{};

            AddedThreatList threatsAdded{};
            RemovedThreatList threatsRemoved{};

            inline void setThreatRefresh(Color c) {
                refreshThreats[c.idx()] = true;
            }

            [[nodiscard]] inline bool requiresThreatRefresh(Color c) const {
                return refreshThreats[c.idx()];
            }

            inline void addThreatFeature(Piece attacker, Square attackerSq, Piece attacked, Square attackedSq) {
                threatsAdded.push({
                    .attacker = attacker,
                    .attackerSq = attackerSq,
                    .attacked = attacked,
                    .attackedSq = attackedSq,
                });
            }

            inline void removeThreatFeature(Piece attacker, Square attackerSq, Piece attacked, Square attackedSq) {
                threatsRemoved.push({
                    .attacker = attacker,
                    .attackerSq = attackerSq,
                    .attacked = attacked,
                    .attackedSq = attackedSq,
                });
            }
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
