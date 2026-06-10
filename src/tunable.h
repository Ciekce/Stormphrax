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

#include "core.h"
#include "util/multi_array.h"

#ifndef SP_EXTERNAL_TUNE
    #define SP_EXTERNAL_TUNE 0
#endif

#if SP_EXTERNAL_TUNE
    #include <functional>
    #include <string>
    #include <string_view>

    #include "util/range.h"
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
    SP_TUNABLE_PARAM_F64(incrementScale, 0.94, 0.5, 1.5, 0.05, 100)
    SP_TUNABLE_PARAM_F64(softTimeScale, 0.68, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(hardTimeScale, 0.65, 0.2, 1.0, 0.05, 100)

    SP_TUNABLE_PARAM_F64(nodeTmBase, 2.59, 1.5, 3.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScale, 1.6, 1.0, 2.5, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScaleMin, 0.188, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_F64(bmStabilityTmMin, 0.78, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmMax, 2.36, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmScale, 8.59, 2.0, 15.0, 0.65, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmOffset, 0.9, 0.5, 2.0, 0.08, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmPower, -2.57, -4.0, -1.5, 0.13, 100)

    SP_TUNABLE_PARAM_F64(scoreTrendTmMin, 0.63, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmMax, 2.48, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScoreScale, 4.94, 0.1, 10.0, 0.5, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmStretch, 0.94, 0.1, 2.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScale, 0.36, 0.1, 0.9, 0.04, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmPositiveScale, 0.94, 0.5, 2.0, 0.075, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmNegativeScale, 1.1, 0.5, 2.0, 0.075, 100)

    SP_TUNABLE_PARAM_F64(timeScaleMin, 0.09, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_CALLBACK(seeValuePawn, 97, 50, 200, 7.5, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueKnight, 434, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueBishop, 464, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueRook, 646, 400, 1000, 30, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueQueen, 1289, 800, 1600, 40, updateSeeTables)

    SP_TUNABLE_PARAM(scalingValuePawn, 48, 20, 200, 6)
    SP_TUNABLE_PARAM(scalingValueKnight, 442, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueBishop, 461, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueRook, 637, 400, 1000, 30)
    SP_TUNABLE_PARAM(scalingValueQueen, 1223, 800, 1600, 40)

    SP_TUNABLE_PARAM(materialScalingBase, 26000, 10000, 40000, 1500)
    SP_TUNABLE_PARAM(optimismBase, 2024, 0, 12000, 400)
    SP_TUNABLE_PARAM(optimismMaterialScale, 1005, 0, 2048, 160)

    SP_TUNABLE_PARAM(optimismStretch, 101, 50, 200, 12)
    SP_TUNABLE_PARAM(optimismScale, 147, 75, 300, 16)

    SP_TUNABLE_PARAM(pawnCorrhistWeight, 173, 32, 384, 18)
    SP_TUNABLE_PARAM(nonPawnCorrhistWeight, 167, 32, 384, 18)
    SP_TUNABLE_PARAM(majorCorrhistWeight, 127, 32, 384, 18)

    SP_TUNABLE_PARAM(contCorrhist1Weight, 152, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist2Weight, 214, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist4Weight, 144, 32, 384, 18)

    SP_TUNABLE_PARAM(initialAspWindow, 7, 1, 50, 2)
    SP_TUNABLE_PARAM(aspSqScoreScale, 200, 0, 1024, 30)
    SP_TUNABLE_PARAM(aspWideningFactor, 17, 1, 24, 2)

    SP_TUNABLE_PARAM(goodNoisySeeOffset, 72, -384, 384, 40)

    SP_TUNABLE_PARAM(movepickButterflyWeight, 425, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickPieceToWeight, 426, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont1Weight, 1043, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont2Weight, 951, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont4Weight, 561, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont6Weight, 293, 0, 4096, 100)

    SP_TUNABLE_PARAM(directCheckBonus, 9994, 2048, 16384, 716)
    SP_TUNABLE_PARAM(directCheckSeeThreshold, -37, -300, 100, 60)

    SP_TUNABLE_PARAM(maxTtCutoffBonus, 2866, 1024, 4096, 256)
    SP_TUNABLE_PARAM(ttCutoffBonusDepthScale, 263, 128, 512, 32)
    SP_TUNABLE_PARAM(ttCutoffBonusOffset, 385, 0, 768, 64)

    SP_TUNABLE_PARAM(evalPolicyScale, 10, 5, 20, 0.7)
    SP_TUNABLE_PARAM(evalPolicyMin, -1841, -4000, -1000, 150)
    SP_TUNABLE_PARAM(evalPolicyMax, 1841, 1000, 4000, 150)
    SP_TUNABLE_PARAM(evalPolicyOffset, 503, 0, 1200, 60)

    SP_TUNABLE_PARAM(hindsightReductionMargin, 202, 100, 400, 15)

    SP_TUNABLE_PARAM(rfpLinearMargin, 85, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpQuadMargin, 7, 0, 25, 1.2)
    SP_TUNABLE_PARAM(rfpImprovingMargin, 75, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpCorrplexityScale, 62, 16, 128, 5)
    SP_TUNABLE_PARAM(rfpFailFirmT, 711, 0, 1024, 51)

    SP_TUNABLE_PARAM(razoringMargin, 347, 100, 700, 40)

    SP_TUNABLE_PARAM(nmpBetaBaseMargin, 213, 100, 400, 30)
    SP_TUNABLE_PARAM(nmpBetaMarginDepthScale, 1281, 128, 2560, 60)
    SP_TUNABLE_PARAM(nmpBetaImprovingMargin, 41, 10, 80, 6)

    SP_TUNABLE_PARAM(probcutMargin, 307, 150, 400, 40)
    SP_TUNABLE_PARAM(probcutImprovingMargin, 10, -250, 250, 40)
    SP_TUNABLE_PARAM(probcutSeeScale, 17, 6, 24, 3)

    SP_TUNABLE_PARAM(searchButterflyWeight, 481, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchPieceToWeight, 469, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont1Weight, 1082, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont2Weight, 1092, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont4Weight, 689, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont6Weight, 292, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrDepthTtpvScale, 726, 384, 1536, 64)

    SP_TUNABLE_PARAM(lmpHistoryScale, 985, 384, 3072, 100)

    SP_TUNABLE_PARAM(fpMargin, 274, 120, 400, 20)
    SP_TUNABLE_PARAM(fpScale, 68, 30, 120, 4)
    SP_TUNABLE_PARAM(fpHistoryDivisor, 102, 32, 384, 18)

    SP_TUNABLE_PARAM(quietHistPruningMargin, -2242, -4000, -1000, 175)
    SP_TUNABLE_PARAM(quietHistPruningOffset, -1315, -4000, 4000, 400)

    SP_TUNABLE_PARAM(noisyHistPruningMargin, -1006, -4000, -1000, 175)
    SP_TUNABLE_PARAM(noisyHistPruningOffset, -1121, -4000, 4000, 400)

    SP_TUNABLE_PARAM(seePruningThresholdQuiet, -20, -80, -1, 12)
    SP_TUNABLE_PARAM(seePruningThresholdNoisy, -111, -120, -40, 20)

    SP_TUNABLE_PARAM(seePruningNoisyHistDivisor, 65, 12, 256, 10)

    SP_TUNABLE_PARAM(sBetaBaseMargin, 143, 32, 256, 12)
    SP_TUNABLE_PARAM(sBetaPrevPvMargin, 136, 32, 256, 12)

    SP_TUNABLE_PARAM(doubleExtBaseMargin, 13, -20, 50, 4)
    SP_TUNABLE_PARAM(doubleExtPvMargin, 135, 75, 300, 12)
    SP_TUNABLE_PARAM(doubleExtCorrScale, 881, 0, 8192, 128)
    SP_TUNABLE_PARAM(doubleExtNewPvMargin, 50, 0, 100, 5)

    SP_TUNABLE_PARAM(tripleExtBaseMargin, 99, 30, 150, 8)
    SP_TUNABLE_PARAM(tripleExtPvMargin, 375, 200, 800, 30)
    SP_TUNABLE_PARAM(tripleExtNoisyMargin, 199, 100, 400, 15)
    SP_TUNABLE_PARAM(tripleExtCorrScale, 1014, 0, 8192, 128)
    SP_TUNABLE_PARAM(tripleExtNewPvMargin, 50, 0, 100, 5)

    SP_TUNABLE_PARAM(multicutFailFirmT, 503, 0, 1024, 51)

    SP_TUNABLE_PARAM(ldseMargin, 22, 10, 60, 3)
    SP_TUNABLE_PARAM(ldseDoubleExtMargin, 39, 20, 80, 3)

    SP_TUNABLE_PARAM_CALLBACK(quietLmrBase, 78, 50, 120, 10, updateQuietLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(quietLmrDivisor, 236, 100, 300, 20, updateQuietLmrTable)

    SP_TUNABLE_PARAM_CALLBACK(noisyLmrBase, -10, -50, 75, 10, updateNoisyLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(noisyLmrDivisor, 249, 150, 350, 20, updateNoisyLmrTable)

    SP_TUNABLE_PARAM(lmrButterflyWeight, 446, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrPieceToWeight, 379, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont1Weight, 1123, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont2Weight, 1209, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont4Weight, 604, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont6Weight, 343, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrOffset, -37, -2048, 2048, 50)
    SP_TUNABLE_PARAM(lmrNonPvReductionScale, 1069, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtpvReductionScale, 1146, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrImprovingReductionScale, 1242, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrCheckReductionScale, 852, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrCutnodeReductionScale, 1945, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtpvFailLowReductionScale, 1054, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrAlphaRaiseReductionScale, 597, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtMoveNoisyReductionScale, 1081, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrMoveCountReductionScale, 42, 0, 128, 10)

    SP_TUNABLE_PARAM(lmrQuietHistoryScale, 447, 192, 768, 28)
    SP_TUNABLE_PARAM(lmrNoisyHistoryScale, 423, 192, 768, 28)

    SP_TUNABLE_PARAM(lmrComplexityScale, 480, 256, 1024, 38)

    SP_TUNABLE_PARAM(lmrTtpvExtThreshold, -886, -3072, 0, 152)

    SP_TUNABLE_PARAM(lmrDeeperBase, 44, 20, 100, 6)
    SP_TUNABLE_PARAM(lmrDeeperScale, 4, 3, 12, 1)

    SP_TUNABLE_PARAM(maxPostLmrContBonus, 2723, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContBonusDepthScale, 277, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContBonusOffset, 575, 0, 768, 64)

    SP_TUNABLE_PARAM(maxPostLmrContPenalty, 1027, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContPenaltyDepthScale, 307, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContPenaltyOffset, 245, 0, 768, 64)

    SP_TUNABLE_PARAM(maxButterflyHistory, 16334, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxPieceToHistory, 16220, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxConthist, 16443, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxNoisyHistory, 16287, 8192, 32768, 256)

    SP_TUNABLE_PARAM(maxQuietBonus, 2663, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBonusDepthScale, 339, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBonusOffset, 313, 0, 768, 64)

    SP_TUNABLE_PARAM(maxQuietPenalty, 1318, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietPenaltyDepthScale, 384, 128, 512, 32)
    SP_TUNABLE_PARAM(quietPenaltyOffset, 177, 0, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBonus, 2355, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBonusDepthScale, 274, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBonusOffset, 469, 0, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBmNoisyPenalty, 1777, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyDepthScale, 319, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyOffset, 170, 0, 768, 64)

    SP_TUNABLE_PARAM(maxQuietBmNoisyPenalty, 1629, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyDepthScale, 371, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyOffset, 78, 0, 768, 64)

    SP_TUNABLE_PARAM(maxPcmBonus, 1338, 768, 4096, 256)
    SP_TUNABLE_PARAM(pcmBonusDepthScale, 132, 64, 512, 32)
    SP_TUNABLE_PARAM(pcmBonusOffset, 58, 0, 768, 64)

    SP_TUNABLE_PARAM(pcmBaseWeight, 260, -1024, 1024, 51)
    SP_TUNABLE_PARAM(pcmDepthWeight, 401, 0, 768, 38)
    SP_TUNABLE_PARAM(pcmParentMoveCountWeight, 976, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmParentTtMoveWeight, 948, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmStaticEvalWeight, 1047, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmParentStaticEvalWeight, 1023, 0, 2048, 100)

    SP_TUNABLE_PARAM(pcmDepthMax, 4018, 2048, 8192, 300)

    SP_TUNABLE_PARAM(pcmStaticEvalThreshold, 127, 40, 240, 20)
    SP_TUNABLE_PARAM(pcmParentStaticEvalThreshold, 131, 40, 240, 20)

    SP_TUNABLE_PARAM(pcmMainUpdateWeight, 1029, 512, 2048, 76)

    SP_TUNABLE_PARAM(noisyPcmBonus, 59, 0, 2048, 30)

    SP_TUNABLE_PARAM(butterflyAgeingWeight, 977, 0, 1536, 80)
    SP_TUNABLE_PARAM(pieceToAgeingWeight, 1024, 0, 1536, 80)

    SP_TUNABLE_PARAM(butterflyUpdateWeight, 975, 512, 2048, 76)
    SP_TUNABLE_PARAM(pieceToUpdateWeight, 891, 512, 2048, 76)

    SP_TUNABLE_PARAM(cont1UpdateWeight, 1084, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont2UpdateWeight, 1142, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont4UpdateWeight, 952, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont6UpdateWeight, 1003, 512, 2048, 76)

    SP_TUNABLE_PARAM(contBaseButterflyWeight, 211, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBasePieceToWeight, 101, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont1Weight, 929, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont2Weight, 917, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont4Weight, 553, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont6Weight, 320, 0, 4096, 100)

    SP_TUNABLE_PARAM(standPatFailFirmT, 610, 0, 1024, 51)

    SP_TUNABLE_PARAM(qsearchFpMargin, 142, 50, 400, 17)
    SP_TUNABLE_PARAM(qsearchSeeThreshold, -81, -2000, 200, 30)

    SP_TUNABLE_PARAM(threadWeightScoreOffset, 11, 0, 20, 1)

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
} // namespace stormphrax::tunable
