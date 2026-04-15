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

#include <array>
#include <functional>
#include <string_view>

#include "util/multi_array.h"
#include "util/range.h"
#include "core.h"

#ifndef SP_EXTERNAL_TUNE
    #define SP_EXTERNAL_TUNE 0
#endif

namespace stormphrax::tunable {
    void init();

    // [noisy][depth][legal moves]
    extern util::MultiArray<i32, 2, 256, 256> g_lmrTable;

    // [coloured piece], +1 for none
    extern std::array<i32, 13> g_seeValues;
    extern std::array<PieceType, PieceTypes::kCount> g_seeOrderedPts;

    void updateQuietLmrTable();
    void updateNoisyLmrTable();

    void updateSeeTables();

#define SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
    static_assert((Default) >= (Min)); \
    static_assert((Default) <= (Max)); \
    static_assert((Min) < (Max)); \
    static_assert((Min) + (Step) <= (Max)); \
    static_assert((Step) >= 0.5);

#define SP_TUNABLE_ASSERTS_F64(Default, Min, Max, Step, Q) \
    SP_TUNABLE_ASSERTS( \
        static_cast<i32>((Default) * (Q)), \
        static_cast<i32>((Min) * (Q)), \
        static_cast<i32>((Max) * (Q)), \
        static_cast<i32>((Step) * (Q)) \
    ) \
    static_assert((Q) > 0); \
    static_assert(static_cast<f64>(static_cast<i32>(Q)) == (Q));

#if SP_EXTERNAL_TUNE
    struct TunableParam {
        std::string name;
        std::string lowerName;
        i32 defaultValue;
        i32 value;
        util::Range<i32> range;
        f64 step;
        std::function<void()> callback;
    };

    TunableParam& addTunableParam(
        std::string_view name,
        i32 value,
        i32 min,
        i32 max,
        f64 step,
        std::function<void()> callback
    );

    #define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) \
        SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
        inline TunableParam& param_##Name = addTunableParam(#Name, Default, Min, Max, Step, nullptr); \
        [[nodiscard]] inline i32 Name() { \
            return param_##Name.value; \
        }

    #define SP_TUNABLE_PARAM_CALLBACK(Name, Default, Min, Max, Step, Callback) \
        SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
        inline TunableParam& param_##Name = addTunableParam(#Name, Default, Min, Max, Step, Callback); \
        [[nodiscard]] inline i32 Name() { \
            return param_##Name.value; \
        }

    //TODO tune these as actual floating-point values
    #define SP_TUNABLE_PARAM_F64(Name, Default, Min, Max, Step, Q) \
        SP_TUNABLE_ASSERTS_F64(Default, Min, Max, Step, Q) \
        inline TunableParam& param_##Name = \
            addTunableParam(#Name, (Default) * (Q), (Min) * (Q), (Max) * (Q), (Step) * (Q), nullptr); \
        [[nodiscard]] inline f64 Name() { \
            return static_cast<f64>(param_##Name.value) / (Q); \
        }
#else
    #define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) \
        SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
        [[nodiscard]] constexpr i32 Name() { \
            return Default; \
        }
    #define SP_TUNABLE_PARAM_CALLBACK(Name, Default, Min, Max, Step, Callback) \
        SP_TUNABLE_PARAM(Name, Default, Min, Max, Step)

    #define SP_TUNABLE_PARAM_F64(Name, Default, Min, Max, Step, Q) \
        SP_TUNABLE_ASSERTS_F64(Default, Min, Max, Step, Q) \
        [[nodiscard]] constexpr f64 Name() { \
            return Default; \
        }
#endif

    SP_TUNABLE_PARAM(defaultMovesToGo, 19, 12, 40, 1)
    SP_TUNABLE_PARAM_F64(incrementScale, 0.92, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(softTimeScale, 0.71, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(hardTimeScale, 0.64, 0.2, 1.0, 0.05, 100)

    SP_TUNABLE_PARAM_F64(nodeTmBase, 2.57, 1.5, 3.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScale, 1.6, 1.0, 2.5, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScaleMin, 0.138, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_F64(bmStabilityTmMin, 0.77, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmMax, 2.18, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmScale, 8.19, 2.0, 15.0, 0.65, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmOffset, 0.85, 0.5, 2.0, 0.08, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmPower, -2.71, -4.0, -1.5, 0.13, 100)

    SP_TUNABLE_PARAM_F64(scoreTrendTmMin, 0.62, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmMax, 2.05, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScoreScale, 4.88, 0.1, 10.0, 0.5, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmStretch, 0.93, 0.1, 2.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScale, 0.36, 0.1, 0.9, 0.04, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmPositiveScale, 0.97, 0.5, 2.0, 0.075, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmNegativeScale, 1.03, 0.5, 2.0, 0.075, 100)

    SP_TUNABLE_PARAM_F64(timeScaleMin, 0.118, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_CALLBACK(seeValuePawn, 95, 50, 200, 7.5, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueKnight, 449, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueBishop, 447, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueRook, 670, 400, 1000, 30, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueQueen, 1240, 800, 1600, 40, updateSeeTables)

    SP_TUNABLE_PARAM(scalingValueKnight, 450, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueBishop, 450, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueRook, 650, 400, 1000, 30)
    SP_TUNABLE_PARAM(scalingValueQueen, 1250, 800, 1600, 40)

    SP_TUNABLE_PARAM(materialScalingBase, 26500, 10000, 40000, 1500)
    SP_TUNABLE_PARAM(optimismBase, 2000, 0, 12000, 400)

    SP_TUNABLE_PARAM(optimismStretch, 103, 50, 200, 8)
    SP_TUNABLE_PARAM(optimismScale, 147, 75, 300, 12)

    SP_TUNABLE_PARAM(pawnCorrhistWeight, 157, 32, 384, 18)
    SP_TUNABLE_PARAM(stmNonPawnCorrhistWeight, 150, 32, 384, 18)
    SP_TUNABLE_PARAM(nstmNonPawnCorrhistWeight, 159, 32, 384, 18)
    SP_TUNABLE_PARAM(majorCorrhistWeight, 115, 32, 384, 18)

    SP_TUNABLE_PARAM(contCorrhist1Weight, 128, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist2Weight, 204, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist4Weight, 181, 32, 384, 18)

    SP_TUNABLE_PARAM(initialAspWindow, 16, 4, 50, 1)
    SP_TUNABLE_PARAM(aspWideningFactor, 17, 1, 24, 1)

    SP_TUNABLE_PARAM(goodNoisySeeOffset, 61, -384, 384, 40)

    SP_TUNABLE_PARAM(movepickButterflyWeight, 477, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickPieceToWeight, 501, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont1Weight, 1101, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont2Weight, 1025, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont4Weight, 561, 0, 4096, 100)

    SP_TUNABLE_PARAM(directCheckBonus, 8678, 2048, 16384, 716)
    SP_TUNABLE_PARAM(directCheckSeeThreshold, -13, -300, 0, 60)

    SP_TUNABLE_PARAM(maxTtCutoffBonus, 2543, 1024, 4096, 256)
    SP_TUNABLE_PARAM(ttCutoffBonusDepthScale, 277, 128, 512, 32)
    SP_TUNABLE_PARAM(ttCutoffBonusOffset, 403, 128, 768, 64)

    SP_TUNABLE_PARAM(evalPolicyScale, 10, 5, 20, 0.7)
    SP_TUNABLE_PARAM(evalPolicyMin, -2025, -4000, -1000, 150)
    SP_TUNABLE_PARAM(evalPolicyMax, 1891, 1000, 4000, 150)
    SP_TUNABLE_PARAM(evalPolicyOffset, 545, 0, 1200, 60)

    SP_TUNABLE_PARAM(hindsightReductionMargin, 205, 100, 400, 15)

    SP_TUNABLE_PARAM(rfpConstantMargin, 1, 0, 100, 5)
    SP_TUNABLE_PARAM(rfpLinearMargin, 79, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpQuadMargin, 5, 0, 25, 1.2)
    SP_TUNABLE_PARAM(rfpImprovingMargin, 76, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpCorrplexityScale, 63, 16, 128, 5)
    SP_TUNABLE_PARAM(rfpFailFirmT, 606, 0, 1024, 51)

    SP_TUNABLE_PARAM(razoringMargin, 328, 100, 350, 40)

    SP_TUNABLE_PARAM(nmpBetaBaseMargin, 204, 100, 400, 15)
    SP_TUNABLE_PARAM(nmpBetaMarginDepthScale, 1280, 128, 2560, 32)
    SP_TUNABLE_PARAM(nmpBetaImprovingMargin, 40, 10, 80, 3)

    SP_TUNABLE_PARAM(probcutMargin, 314, 150, 400, 13)
    SP_TUNABLE_PARAM(probcutSeeScale, 17, 6, 24, 1)

    SP_TUNABLE_PARAM(searchButterflyWeight, 517, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchPieceToWeight, 490, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont1Weight, 1131, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont2Weight, 1062, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont4Weight, 579, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrDepthTtpvScale, 89, 48, 192, 8)

    SP_TUNABLE_PARAM(fpMargin, 263, 120, 400, 20)
    SP_TUNABLE_PARAM(fpScale, 69, 40, 120, 4)
    SP_TUNABLE_PARAM(fpHistoryDivisor, 113, 32, 384, 18)

    SP_TUNABLE_PARAM(quietHistPruningMargin, -2415, -4000, -1000, 175)
    SP_TUNABLE_PARAM(quietHistPruningOffset, -1000, -4000, 4000, 400)

    SP_TUNABLE_PARAM(noisyHistPruningMargin, -1086, -4000, -1000, 175)
    SP_TUNABLE_PARAM(noisyHistPruningOffset, -1135, -4000, 4000, 400)

    SP_TUNABLE_PARAM(seePruningThresholdQuiet, -19, -80, -1, 12)
    SP_TUNABLE_PARAM(seePruningThresholdNoisy, -106, -120, -40, 20)

    SP_TUNABLE_PARAM(seePruningNoisyHistDivisor, 68, 12, 256, 10)

    SP_TUNABLE_PARAM(sBetaBaseMargin, 131, 32, 256, 12)
    SP_TUNABLE_PARAM(sBetaPrevPvMargin, 128, 32, 256, 12)

    SP_TUNABLE_PARAM(doubleExtBaseMargin, 11, -20, 50, 4)
    SP_TUNABLE_PARAM(doubleExtPvMargin, 155, 75, 300, 12)
    SP_TUNABLE_PARAM(doubleExtNoisyMargin, 2, 0, 200, 10)
    SP_TUNABLE_PARAM(tripleExtBaseMargin, 98, 30, 150, 8)
    SP_TUNABLE_PARAM(tripleExtPvMargin, 374, 200, 800, 30)
    SP_TUNABLE_PARAM(tripleExtNoisyMargin, 186, 100, 400, 15)

    SP_TUNABLE_PARAM(multicutFailFirmT, 469, 0, 1024, 51)

    SP_TUNABLE_PARAM(ldseMargin, 21, 10, 60, 3)
    SP_TUNABLE_PARAM(ldseDoubleExtMargin, 41, 20, 80, 3)

    SP_TUNABLE_PARAM_CALLBACK(quietLmrBase, 78, 50, 120, 5, updateQuietLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(quietLmrDivisor, 239, 100, 300, 10, updateQuietLmrTable)

    SP_TUNABLE_PARAM_CALLBACK(noisyLmrBase, -13, -50, 75, 5, updateNoisyLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(noisyLmrDivisor, 242, 150, 350, 10, updateNoisyLmrTable)

    SP_TUNABLE_PARAM(lmrNonPvReductionScale, 139, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrTtpvReductionScale, 152, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrImprovingReductionScale, 150, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrCheckReductionScale, 111, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrCutnodeReductionScale, 254, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrTtpvFailLowReductionScale, 144, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrAlphaRaiseReductionScale, 56, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrTtMoveNoisyReductionScale, 125, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrHighComplexityReductionScale, 141, 32, 384, 12)

    SP_TUNABLE_PARAM(lmrQuietHistoryScale, 429, 192, 768, 28)
    SP_TUNABLE_PARAM(lmrNoisyHistoryScale, 411, 192, 768, 28)

    SP_TUNABLE_PARAM(lmrHighComplexityThreshold, 69, 30, 120, 5)

    SP_TUNABLE_PARAM(lmrTtpvExtThreshold, -123, -384, 0, 19)

    SP_TUNABLE_PARAM(lmrDeeperBase, 43, 20, 100, 6)
    SP_TUNABLE_PARAM(lmrDeeperScale, 4, 3, 12, 1)

    SP_TUNABLE_PARAM(maxPostLmrContBonus, 2668, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContBonusDepthScale, 263, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContBonusOffset, 523, 128, 768, 64)

    SP_TUNABLE_PARAM(maxPostLmrContPenalty, 1197, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContPenaltyDepthScale, 312, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContPenaltyOffset, 278, 128, 768, 64)

    SP_TUNABLE_PARAM(maxButterflyHistory, 16261, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxPieceToHistory, 16232, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxConthist, 16248, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxNoisyHistory, 16403, 8192, 32768, 256)

    SP_TUNABLE_PARAM(maxQuietBonus, 2640, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBonusDepthScale, 304, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBonusOffset, 315, 128, 768, 64)

    SP_TUNABLE_PARAM(maxQuietPenalty, 1153, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietPenaltyDepthScale, 353, 128, 512, 32)
    SP_TUNABLE_PARAM(quietPenaltyOffset, 194, 128, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBonus, 2459, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBonusDepthScale, 263, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBonusOffset, 476, 128, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBmNoisyPenalty, 1613, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyDepthScale, 352, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyOffset, 190, 128, 768, 64)

    SP_TUNABLE_PARAM(maxQuietBmNoisyPenalty, 1573, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyDepthScale, 367, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyOffset, 147, 128, 768, 64)

    SP_TUNABLE_PARAM(butterflyUpdateWeight, 941, 512, 2048, 76)
    SP_TUNABLE_PARAM(pieceToUpdateWeight, 870, 512, 2048, 76)

    SP_TUNABLE_PARAM(cont1UpdateWeight, 1031, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont2UpdateWeight, 1079, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont4UpdateWeight, 1042, 512, 2048, 76)

    SP_TUNABLE_PARAM(contBaseButterflyWeight, 246, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBasePieceToWeight, 190, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont1Weight, 995, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont2Weight, 1029, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont4Weight, 496, 0, 4096, 100)

    SP_TUNABLE_PARAM(standPatFailFirmT, 548, 0, 1024, 51)

    SP_TUNABLE_PARAM(qsearchFpMargin, 137, 50, 400, 17)
    SP_TUNABLE_PARAM(qsearchSeeThreshold, -88, -2000, 200, 30)

    SP_TUNABLE_PARAM(threadWeightScoreOffset, 10, 0, 20, 1)

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
} // namespace stormphrax::tunable
