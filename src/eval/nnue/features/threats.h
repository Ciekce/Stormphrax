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

#include <array>

#include "../../../bitboard.h"
#include "psq.h"

namespace stormphrax::eval::nnue::features::threats {
    constexpr u32 kTotalThreatFeatures = 60144;

    constexpr u32 kTotalPpFeatures = 96 * 95 / 2;
    constexpr u32 kTotalPpThreatFeatures = 59808 + kTotalPpFeatures;

    // just in case
    constexpr usize kMaxThreatsAdded = 128;
    constexpr usize kMaxThreatsRemoved = 128;

    using AddedThreatList = StaticVector<psq::UpdatedThreat, kMaxThreatsAdded>;
    using RemovedThreatList = StaticVector<psq::UpdatedThreat, kMaxThreatsRemoved>;

    template <typename PsqFeatureSet>
    struct ThreatInputs : PsqFeatureSet {
        static constexpr bool kThreatInputs = true;
        static constexpr bool kPawnPawnThreats = true;
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

    template <typename PsqFeatureSet>
    struct PawnPawnThreatInputs : ThreatInputs<PsqFeatureSet> {
        static constexpr bool kPawnPawnThreats = false;
        static constexpr u32 kThreatFeatures = kTotalPpThreatFeatures;
        static constexpr u32 kThreatOffset = kTotalPpFeatures;
    };

    constexpr std::array<Bitboard, Squares::kCount> kPpMasks = [] {
        std::array<Bitboard, Squares::kCount> masks{};

        for (u8 sqIdx = 8; sqIdx < 56; ++sqIdx) {
            const auto sq = Square::fromRaw(sqIdx);

            auto bb = Bitboard::fromSquare(sq);

            bb |= bb.shiftLeft();
            bb |= bb.shiftRight();

            bb = bb.fillFile();

            masks[sqIdx] = bb;
        }

        return masks;
    }();

    [[nodiscard]] i32 threatFeatureIndex(
        Color c,
        Square kingSq,
        Piece attacker,
        Square attackerSq,
        Piece attacked,
        Square attackedSq
    );

    [[nodiscard]] i32 ppFeatureIndex(Color c, Square kingSq, Color a, Square aSq, Color b, Square bSq);
} // namespace stormphrax::eval::nnue::features::threats
