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
    SP_TUNABLE_PARAM_F64(incrementScale, 0.94, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(softTimeScale, 0.69, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(hardTimeScale, 0.65, 0.2, 1.0, 0.05, 100)

    SP_TUNABLE_PARAM_F64(nodeTmBase, 2.6, 1.5, 3.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScale, 1.6, 1.0, 2.5, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScaleMin, 0.197, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_F64(bmStabilityTmMin, 0.77, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmMax, 2.43, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmScale, 8.45, 2.0, 15.0, 0.65, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmOffset, 0.9, 0.5, 2.0, 0.08, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmPower, -2.57, -4.0, -1.5, 0.13, 100)

    SP_TUNABLE_PARAM_F64(scoreTrendTmMin, 0.63, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmMax, 2.49, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScoreScale, 4.99, 0.1, 10.0, 0.5, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmStretch, 0.97, 0.1, 2.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScale, 0.35, 0.1, 0.9, 0.04, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmPositiveScale, 0.93, 0.5, 2.0, 0.075, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmNegativeScale, 1.11, 0.5, 2.0, 0.075, 100)

    SP_TUNABLE_PARAM_F64(timeScaleMin, 0.89, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_CALLBACK(seeValuePawn, 96, 50, 200, 7.5, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueKnight, 437, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueBishop, 463, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueRook, 645, 400, 1000, 30, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueQueen, 1273, 800, 1600, 40, updateSeeTables)

    SP_TUNABLE_PARAM(scalingValuePawn, 48, 20, 200, 6)
    SP_TUNABLE_PARAM(scalingValueKnight, 435, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueBishop, 455, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueRook, 642, 400, 1000, 30)
    SP_TUNABLE_PARAM(scalingValueQueen, 1217, 800, 1600, 40)

    SP_TUNABLE_PARAM(materialScalingBase, 26000, 10000, 40000, 1500)
    SP_TUNABLE_PARAM(optimismBase, 2000, 0, 12000, 400)

    SP_TUNABLE_PARAM(optimismStretch, 102, 50, 200, 8)
    SP_TUNABLE_PARAM(optimismScale, 141, 75, 300, 12)

    SP_TUNABLE_PARAM(pawnCorrhistWeight, 177, 32, 384, 18)
    SP_TUNABLE_PARAM(stmNonPawnCorrhistWeight, 168, 32, 384, 18)
    SP_TUNABLE_PARAM(nstmNonPawnCorrhistWeight, 172, 32, 384, 18)
    SP_TUNABLE_PARAM(majorCorrhistWeight, 130, 32, 384, 18)

    SP_TUNABLE_PARAM(contCorrhist1Weight, 152, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist2Weight, 212, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist4Weight, 155, 32, 384, 18)

    SP_TUNABLE_PARAM(initialAspWindow, 16, 4, 50, 1)
    SP_TUNABLE_PARAM(aspWideningFactor, 17, 1, 24, 1)

    SP_TUNABLE_PARAM(goodNoisySeeOffset, 85, -384, 384, 40)

    SP_TUNABLE_PARAM(movepickButterflyWeight, 435, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickPieceToWeight, 443, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont1Weight, 1041, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont2Weight, 980, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont4Weight, 570, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont6Weight, 276, 0, 4096, 100)

    SP_TUNABLE_PARAM(directCheckBonus, 10173, 2048, 16384, 716)
    SP_TUNABLE_PARAM(directCheckSeeThreshold, -6, -300, 100, 60)

    SP_TUNABLE_PARAM(maxTtCutoffBonus, 2855, 1024, 4096, 256)
    SP_TUNABLE_PARAM(ttCutoffBonusDepthScale, 269, 128, 512, 32)
    SP_TUNABLE_PARAM(ttCutoffBonusOffset, 381, 0, 768, 64)

    SP_TUNABLE_PARAM(evalPolicyScale, 10, 5, 20, 0.7)
    SP_TUNABLE_PARAM(evalPolicyMin, -1913, -4000, -1000, 150)
    SP_TUNABLE_PARAM(evalPolicyMax, 1862, 1000, 4000, 150)
    SP_TUNABLE_PARAM(evalPolicyOffset, 520, 0, 1200, 60)

    SP_TUNABLE_PARAM(hindsightReductionMargin, 201, 100, 400, 15)

    SP_TUNABLE_PARAM(rfpConstantMargin, 2, -100, 100, 10)
    SP_TUNABLE_PARAM(rfpLinearMargin, 84, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpQuadMargin, 7, 0, 25, 1.2)
    SP_TUNABLE_PARAM(rfpImprovingMargin, 74, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpCorrplexityScale, 63, 16, 128, 5)
    SP_TUNABLE_PARAM(rfpFailFirmT, 700, 0, 1024, 51)

    SP_TUNABLE_PARAM(razoringMargin, 345, 100, 700, 40)

    SP_TUNABLE_PARAM(nmpBetaBaseMargin, 204, 100, 400, 15)
    SP_TUNABLE_PARAM(nmpBetaMarginDepthScale, 1275, 128, 2560, 32)
    SP_TUNABLE_PARAM(nmpBetaImprovingMargin, 41, 10, 80, 3)

    SP_TUNABLE_PARAM(probcutMargin, 308, 150, 400, 13)
    SP_TUNABLE_PARAM(probcutSeeScale, 17, 6, 24, 1)

    SP_TUNABLE_PARAM(searchButterflyWeight, 489, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchPieceToWeight, 435, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont1Weight, 1107, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont2Weight, 1102, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont4Weight, 694, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont6Weight, 281, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrDepthTtpvScale, 718, 384, 1536, 64)

    SP_TUNABLE_PARAM(fpMargin, 274, 120, 400, 20)
    SP_TUNABLE_PARAM(fpScale, 68, 30, 120, 4)
    SP_TUNABLE_PARAM(fpHistoryDivisor, 101, 32, 384, 18)

    SP_TUNABLE_PARAM(quietHistPruningMargin, -2238, -4000, -1000, 175)
    SP_TUNABLE_PARAM(quietHistPruningOffset, -1322, -4000, 4000, 400)

    SP_TUNABLE_PARAM(noisyHistPruningMargin, -1011, -4000, -1000, 175)
    SP_TUNABLE_PARAM(noisyHistPruningOffset, -1176, -4000, 4000, 400)

    SP_TUNABLE_PARAM(seePruningThresholdQuiet, -21, -80, -1, 12)
    SP_TUNABLE_PARAM(seePruningThresholdNoisy, -108, -120, -40, 20)

    SP_TUNABLE_PARAM(seePruningNoisyHistDivisor, 65, 12, 256, 10)

    SP_TUNABLE_PARAM(sBetaBaseMargin, 139, 32, 256, 12)
    SP_TUNABLE_PARAM(sBetaPrevPvMargin, 132, 32, 256, 12)

    SP_TUNABLE_PARAM(doubleExtBaseMargin, 13, -20, 50, 4)
    SP_TUNABLE_PARAM(doubleExtPvMargin, 151, 75, 300, 12)
    SP_TUNABLE_PARAM(doubleExtCorrScale, 872, 0, 8192, 128)
    SP_TUNABLE_PARAM(tripleExtBaseMargin, 98, 30, 150, 8)
    SP_TUNABLE_PARAM(tripleExtPvMargin, 388, 200, 800, 30)
    SP_TUNABLE_PARAM(tripleExtNoisyMargin, 192, 100, 400, 15)
    SP_TUNABLE_PARAM(tripleExtCorrScale, 1015, 0, 8192, 128)

    SP_TUNABLE_PARAM(multicutFailFirmT, 496, 0, 1024, 51)

    SP_TUNABLE_PARAM(ldseMargin, 23, 10, 60, 3)
    SP_TUNABLE_PARAM(ldseDoubleExtMargin, 39, 20, 80, 3)

    SP_TUNABLE_PARAM_CALLBACK(quietLmrBase, 75, 50, 120, 5, updateQuietLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(quietLmrDivisor, 246, 100, 300, 10, updateQuietLmrTable)

    SP_TUNABLE_PARAM_CALLBACK(noisyLmrBase, -12, -50, 75, 5, updateNoisyLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(noisyLmrDivisor, 243, 150, 350, 10, updateNoisyLmrTable)

    SP_TUNABLE_PARAM(lmrButterflyWeight, 436, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrPieceToWeight, 369, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont1Weight, 1160, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont2Weight, 1221, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont4Weight, 601, 0, 4096, 100)
    SP_TUNABLE_PARAM(lmrCont6Weight, 369, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrOffset, -49, -2048, 2048, 50)
    SP_TUNABLE_PARAM(lmrNonPvReductionScale, 1031, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtpvReductionScale, 1178, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrImprovingReductionScale, 1227, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrCheckReductionScale, 851, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrCutnodeReductionScale, 1959, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtpvFailLowReductionScale, 1035, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrAlphaRaiseReductionScale, 586, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrTtMoveNoisyReductionScale, 1045, 256, 3072, 96)
    SP_TUNABLE_PARAM(lmrMoveCountReductionScale, 44, 0, 128, 10)

    SP_TUNABLE_PARAM(lmrQuietHistoryScale, 450, 192, 768, 28)
    SP_TUNABLE_PARAM(lmrNoisyHistoryScale, 418, 192, 768, 28)

    SP_TUNABLE_PARAM(lmrComplexityScale, 488, 256, 1024, 38)

    SP_TUNABLE_PARAM(lmrTtpvExtThreshold, -882, -3072, 0, 152)

    SP_TUNABLE_PARAM(lmrDeeperBase, 43, 20, 100, 6)
    SP_TUNABLE_PARAM(lmrDeeperScale, 4, 3, 12, 1)

    SP_TUNABLE_PARAM(maxPostLmrContBonus, 2700, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContBonusDepthScale, 275, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContBonusOffset, 571, 0, 768, 64)

    SP_TUNABLE_PARAM(maxPostLmrContPenalty, 1034, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContPenaltyDepthScale, 310, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContPenaltyOffset, 244, 0, 768, 64)

    SP_TUNABLE_PARAM(maxButterflyHistory, 16396, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxPieceToHistory, 16236, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxConthist, 16444, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxNoisyHistory, 16322, 8192, 32768, 256)

    SP_TUNABLE_PARAM(maxQuietBonus, 2654, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBonusDepthScale, 340, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBonusOffset, 304, 0, 768, 64)

    SP_TUNABLE_PARAM(maxQuietPenalty, 1163, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietPenaltyDepthScale, 385, 128, 512, 32)
    SP_TUNABLE_PARAM(quietPenaltyOffset, 164, 0, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBonus, 2309, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBonusDepthScale, 261, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBonusOffset, 460, 0, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBmNoisyPenalty, 1749, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyDepthScale, 328, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyOffset, 161, 0, 768, 64)

    SP_TUNABLE_PARAM(maxQuietBmNoisyPenalty, 1611, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyDepthScale, 379, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyOffset, 86, 0, 768, 64)

    SP_TUNABLE_PARAM(maxPcmBonus, 1283, 768, 4096, 256)
    SP_TUNABLE_PARAM(pcmBonusDepthScale, 149, 64, 512, 32)
    SP_TUNABLE_PARAM(pcmBonusOffset, 22, 0, 768, 64)

    SP_TUNABLE_PARAM(pcmBaseWeight, 253, -1024, 1024, 51)
    SP_TUNABLE_PARAM(pcmDepthWeight, 394, 0, 768, 38)
    SP_TUNABLE_PARAM(pcmParentMoveCountWeight, 971, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmParentTtMoveWeight, 969, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmStaticEvalWeight, 1047, 0, 2048, 100)
    SP_TUNABLE_PARAM(pcmParentStaticEvalWeight, 1022, 0, 2048, 100)

    SP_TUNABLE_PARAM(pcmDepthMax, 4032, 2048, 8192, 300)

    SP_TUNABLE_PARAM(pcmStaticEvalThreshold, 125, 60, 240, 9)
    SP_TUNABLE_PARAM(pcmParentStaticEvalThreshold, 126, 60, 240, 9)

    SP_TUNABLE_PARAM(noisyPcmBonus, 53, 0, 2048, 30)

    SP_TUNABLE_PARAM(butterflyUpdateWeight, 971, 512, 2048, 76)
    SP_TUNABLE_PARAM(pieceToUpdateWeight, 884, 512, 2048, 76)

    SP_TUNABLE_PARAM(cont1UpdateWeight, 1072, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont2UpdateWeight, 1131, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont4UpdateWeight, 978, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont6UpdateWeight, 996, 512, 2048, 76)

    SP_TUNABLE_PARAM(contBaseButterflyWeight, 227, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBasePieceToWeight, 107, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont1Weight, 950, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont2Weight, 927, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont4Weight, 525, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont6Weight, 304, 0, 4096, 100)

    SP_TUNABLE_PARAM(standPatFailFirmT, 607, 0, 1024, 51)

    SP_TUNABLE_PARAM(qsearchFpMargin, 137, 50, 400, 17)
    SP_TUNABLE_PARAM(qsearchSeeThreshold, -85, -2000, 200, 30)

    SP_TUNABLE_PARAM(threadWeightScoreOffset, 11, 0, 20, 1)

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
} // namespace stormphrax::tunable
