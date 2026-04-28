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

#include "core.h"
#include "util/multi_array.h"
#include "util/range.h"

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
    SP_TUNABLE_PARAM_F64(incrementScale, 0.93, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(softTimeScale, 0.70, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(hardTimeScale, 0.65, 0.2, 1.0, 0.05, 100)

    SP_TUNABLE_PARAM_F64(nodeTmBase, 2.59, 1.5, 3.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScale, 1.6, 1.0, 2.5, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScaleMin, 0.232, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_F64(bmStabilityTmMin, 0.76, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmMax, 2.42, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmScale, 8.64, 2.0, 15.0, 0.65, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmOffset, 0.89, 0.5, 2.0, 0.08, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmPower, -2.56, -4.0, -1.5, 0.13, 100)

    SP_TUNABLE_PARAM_F64(scoreTrendTmMin, 0.64, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmMax, 2.43, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScoreScale, 4.89, 0.1, 10.0, 0.5, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmStretch, 0.97, 0.1, 2.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScale, 0.34, 0.1, 0.9, 0.04, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmPositiveScale, 0.95, 0.5, 2.0, 0.075, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmNegativeScale, 1.1, 0.5, 2.0, 0.075, 100)

    SP_TUNABLE_PARAM_F64(timeScaleMin, 0.92, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_CALLBACK(seeValuePawn, 94, 50, 200, 7.5, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueKnight, 439, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueBishop, 452, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueRook, 660, 400, 1000, 30, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueQueen, 1265, 800, 1600, 40, updateSeeTables)

    SP_TUNABLE_PARAM(scalingValuePawn, 49, 20, 200, 6)
    SP_TUNABLE_PARAM(scalingValueKnight, 450, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueBishop, 455, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueRook, 639, 400, 1000, 30)
    SP_TUNABLE_PARAM(scalingValueQueen, 1214, 800, 1600, 40)

    SP_TUNABLE_PARAM(materialScalingBase, 26000, 10000, 40000, 1500)
    SP_TUNABLE_PARAM(optimismBase, 2000, 0, 12000, 400)

    SP_TUNABLE_PARAM(optimismStretch, 100, 50, 200, 8)
    SP_TUNABLE_PARAM(optimismScale, 143, 75, 300, 12)

    SP_TUNABLE_PARAM(pawnCorrhistWeight, 170, 32, 384, 18)
    SP_TUNABLE_PARAM(stmNonPawnCorrhistWeight, 164, 32, 384, 18)
    SP_TUNABLE_PARAM(nstmNonPawnCorrhistWeight, 168, 32, 384, 18)
    SP_TUNABLE_PARAM(majorCorrhistWeight, 123, 32, 384, 18)

    SP_TUNABLE_PARAM(contCorrhist1Weight, 147, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist2Weight, 209, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist4Weight, 156, 32, 384, 18)

    SP_TUNABLE_PARAM(initialAspWindow, 16, 4, 50, 1)
    SP_TUNABLE_PARAM(aspWideningFactor, 17, 1, 24, 1)

    SP_TUNABLE_PARAM(goodNoisySeeOffset, 84, -384, 384, 40)

    SP_TUNABLE_PARAM(movepickButterflyWeight, 447, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickPieceToWeight, 470, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont1Weight, 1044, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont2Weight, 999, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont4Weight, 591, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont6Weight, 292, 0, 4096, 100)

    SP_TUNABLE_PARAM(directCheckBonus, 9808, 2048, 16384, 716)
    SP_TUNABLE_PARAM(directCheckSeeThreshold, -5, -300, 0, 60)

    SP_TUNABLE_PARAM(maxTtCutoffBonus, 2761, 1024, 4096, 256)
    SP_TUNABLE_PARAM(ttCutoffBonusDepthScale, 273, 128, 512, 32)
    SP_TUNABLE_PARAM(ttCutoffBonusOffset, 371, 0, 768, 64)

    SP_TUNABLE_PARAM(evalPolicyScale, 10, 5, 20, 0.7)
    SP_TUNABLE_PARAM(evalPolicyMin, -1916, -4000, -1000, 150)
    SP_TUNABLE_PARAM(evalPolicyMax, 1874, 1000, 4000, 150)
    SP_TUNABLE_PARAM(evalPolicyOffset, 511, 0, 1200, 60)

    SP_TUNABLE_PARAM(hindsightReductionMargin, 206, 100, 400, 15)

    SP_TUNABLE_PARAM(rfpConstantMargin, 1, 0, 100, 5)
    SP_TUNABLE_PARAM(rfpLinearMargin, 82, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpQuadMargin, 6, 0, 25, 1.2)
    SP_TUNABLE_PARAM(rfpImprovingMargin, 74, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpCorrplexityScale, 63, 16, 128, 5)
    SP_TUNABLE_PARAM(rfpFailFirmT, 662, 0, 1024, 51)

    SP_TUNABLE_PARAM(razoringMargin, 349, 100, 350, 40)

    SP_TUNABLE_PARAM(nmpBetaBaseMargin, 203, 100, 400, 15)
    SP_TUNABLE_PARAM(nmpBetaMarginDepthScale, 1267, 128, 2560, 32)
    SP_TUNABLE_PARAM(nmpBetaImprovingMargin, 41, 10, 80, 3)

    SP_TUNABLE_PARAM(probcutMargin, 310, 150, 400, 13)
    SP_TUNABLE_PARAM(probcutSeeScale, 17, 6, 24, 1)

    SP_TUNABLE_PARAM(searchButterflyWeight, 468, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchPieceToWeight, 453, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont1Weight, 1113, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont2Weight, 1073, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont4Weight, 655, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont6Weight, 258, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrDepthTtpvScale, 742, 384, 1536, 64)

    SP_TUNABLE_PARAM(fpMargin, 263, 120, 400, 20)
    SP_TUNABLE_PARAM(fpScale, 68, 40, 120, 4)
    SP_TUNABLE_PARAM(fpHistoryDivisor, 101, 32, 384, 18)

    SP_TUNABLE_PARAM(quietHistPruningMargin, -2236, -4000, -1000, 175)
    SP_TUNABLE_PARAM(quietHistPruningOffset, -1038, -4000, 4000, 400)

    SP_TUNABLE_PARAM(noisyHistPruningMargin, -1044, -4000, -1000, 175)
    SP_TUNABLE_PARAM(noisyHistPruningOffset, -1242, -4000, 4000, 400)

    SP_TUNABLE_PARAM(seePruningThresholdQuiet, -22, -80, -1, 12)
    SP_TUNABLE_PARAM(seePruningThresholdNoisy, -117, -120, -40, 20)

    SP_TUNABLE_PARAM(seePruningNoisyHistDivisor, 67, 12, 256, 10)

    SP_TUNABLE_PARAM(sBetaBaseMargin, 137, 32, 256, 12)
    SP_TUNABLE_PARAM(sBetaPrevPvMargin, 131, 32, 256, 12)

    SP_TUNABLE_PARAM(doubleExtBaseMargin, 11, -20, 50, 4)
    SP_TUNABLE_PARAM(doubleExtPvMargin, 151, 75, 300, 12)
    SP_TUNABLE_PARAM(doubleExtCorrScale, 902, 0, 8192, 128)
    SP_TUNABLE_PARAM(tripleExtBaseMargin, 96, 30, 150, 8)
    SP_TUNABLE_PARAM(tripleExtPvMargin, 383, 200, 800, 30)
    SP_TUNABLE_PARAM(tripleExtNoisyMargin, 186, 100, 400, 15)
    SP_TUNABLE_PARAM(tripleExtCorrScale, 998, 0, 8192, 128)

    SP_TUNABLE_PARAM(multicutFailFirmT, 519, 0, 1024, 51)

    SP_TUNABLE_PARAM(ldseMargin, 22, 10, 60, 3)
    SP_TUNABLE_PARAM(ldseDoubleExtMargin, 40, 20, 80, 3)

    SP_TUNABLE_PARAM_CALLBACK(quietLmrBase, 76, 50, 120, 5, updateQuietLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(quietLmrDivisor, 248, 100, 300, 10, updateQuietLmrTable)

    SP_TUNABLE_PARAM_CALLBACK(noisyLmrBase, -11, -50, 75, 5, updateNoisyLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(noisyLmrDivisor, 241, 150, 350, 10, updateNoisyLmrTable)

    SP_TUNABLE_PARAM(lmrButterflyWeight, 455, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrPieceToWeight, 368, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont1Weight, 1180, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont2Weight, 1170, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont4Weight, 554, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont6Weight, 307, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrOffset, -14, -2048, 2048, 50)
    SP_TUNABLE_PARAM(lmrNonPvReductionScale, 1042, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtpvReductionScale, 1173, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrImprovingReductionScale, 1237, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrCheckReductionScale, 849, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrCutnodeReductionScale, 2020, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtpvFailLowReductionScale, 1080, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrAlphaRaiseReductionScale, 525, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtMoveNoisyReductionScale, 1043, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrMoveCountReductionScale, 47, 0, 128, 10)

    SP_TUNABLE_PARAM(lmrQuietHistoryScale, 447, 192, 768, 28)
    SP_TUNABLE_PARAM(lmrNoisyHistoryScale, 410, 192, 768, 28)

    SP_TUNABLE_PARAM(lmrComplexityScale, 480, 256, 1024, 38)

    SP_TUNABLE_PARAM(lmrTtpvExtThreshold, -820, -3072, 0, 152)

    SP_TUNABLE_PARAM(lmrDeeperBase, 42, 20, 100, 6)
    SP_TUNABLE_PARAM(lmrDeeperScale, 4, 3, 12, 1)

    SP_TUNABLE_PARAM(maxPostLmrContBonus, 2806, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContBonusDepthScale, 282, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContBonusOffset, 558, 0, 768, 64)

    SP_TUNABLE_PARAM(maxPostLmrContPenalty, 1026, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContPenaltyDepthScale, 302, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContPenaltyOffset, 241, 0, 768, 64)

    SP_TUNABLE_PARAM(maxButterflyHistory, 16406, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxPieceToHistory, 16213, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxConthist, 16416, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxNoisyHistory, 16402, 8192, 32768, 256)

    SP_TUNABLE_PARAM(maxQuietBonus, 2650, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBonusDepthScale, 327, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBonusOffset, 315, 0, 768, 64)

    SP_TUNABLE_PARAM(maxQuietPenalty, 1064, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietPenaltyDepthScale, 380, 128, 512, 32)
    SP_TUNABLE_PARAM(quietPenaltyOffset, 171, 0, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBonus, 2309, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBonusDepthScale, 259, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBonusOffset, 457, 0, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBmNoisyPenalty, 1791, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyDepthScale, 331, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyOffset, 187, 0, 768, 64)

    SP_TUNABLE_PARAM(maxQuietBmNoisyPenalty, 1709, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyDepthScale, 366, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyOffset, 84, 0, 768, 64)

    SP_TUNABLE_PARAM(maxPcmBonus, 1211, 768, 4096, 256)
    SP_TUNABLE_PARAM(pcmBonusDepthScale, 155, 64, 512, 32)
    SP_TUNABLE_PARAM(pcmBonusOffset, 2, 0, 768, 64)

    SP_TUNABLE_PARAM(pcmBaseWeight, 262, -1024, 1024, 51)
    SP_TUNABLE_PARAM(pcmDepthWeight, 389, 0, 768, 38)
    SP_TUNABLE_PARAM(pcmParentMoveCountWeight, 974, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmParentTtMoveWeight, 946, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmStaticEvalWeight, 1061, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmParentStaticEvalWeight, 994, 0, 2048, 100)

    SP_TUNABLE_PARAM(pcmDepthMax, 4052, 2048, 8192, 300)

    SP_TUNABLE_PARAM(pcmStaticEvalThreshold, 125, 60, 240, 9)
    SP_TUNABLE_PARAM(pcmParentStaticEvalThreshold, 122, 60, 240, 9)

    SP_TUNABLE_PARAM(noisyPcmBonus, 42, 0, 2048, 30)

    SP_TUNABLE_PARAM(butterflyUpdateWeight, 958, 512, 2048, 76)
    SP_TUNABLE_PARAM(pieceToUpdateWeight, 907, 512, 2048, 76)

    SP_TUNABLE_PARAM(cont1UpdateWeight, 1043, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont2UpdateWeight, 1112, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont4UpdateWeight, 1019, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont6UpdateWeight, 1024, 512, 2048, 76)

    SP_TUNABLE_PARAM(contBaseButterflyWeight, 188, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBasePieceToWeight, 164, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont1Weight, 972, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont2Weight, 966, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont4Weight, 477, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont6Weight, 286, 0, 4096, 100)

    SP_TUNABLE_PARAM(standPatFailFirmT, 607, 0, 1024, 51)

    SP_TUNABLE_PARAM(qsearchFpMargin, 143, 50, 400, 17)
    SP_TUNABLE_PARAM(qsearchSeeThreshold, -91, -2000, 200, 30)

    SP_TUNABLE_PARAM(threadWeightScoreOffset, 11, 0, 20, 1)

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
} // namespace stormphrax::tunable
