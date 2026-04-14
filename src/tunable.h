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
    #define SP_EXTERNAL_TUNE 1
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
    SP_TUNABLE_PARAM_F64(incrementScale, 0.83, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(softTimeScale, 0.68, 0.5, 1.0, 0.05, 100)
    SP_TUNABLE_PARAM_F64(hardTimeScale, 0.56, 0.2, 1.0, 0.05, 100)

    SP_TUNABLE_PARAM_F64(nodeTmBase, 2.63, 1.5, 3.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScale, 1.7, 1.0, 2.5, 0.1, 100)
    SP_TUNABLE_PARAM_F64(nodeTmScaleMin, 0.102, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_F64(bmStabilityTmMin, 0.75, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmMax, 2.4, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmScale, 9.11, 2.0, 15.0, 0.65, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmOffset, 0.8, 0.5, 2.0, 0.08, 100)
    SP_TUNABLE_PARAM_F64(bmStabilityTmPower, -2.7, -4.0, -1.5, 0.13, 100)

    SP_TUNABLE_PARAM_F64(scoreTrendTmMin, 0.6, 0.4, 1.0, 0.03, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmMax, 1.7, 1.2, 10.0, 0.4, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScoreScale, 4.58, 0.1, 10.0, 0.5, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmStretch, 0.8, 0.1, 2.0, 0.1, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmScale, 0.41, 0.1, 0.9, 0.04, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmPositiveScale, 1.09, 0.5, 2.0, 0.075, 100)
    SP_TUNABLE_PARAM_F64(scoreTrendTmNegativeScale, 1.04, 0.5, 2.0, 0.075, 100)

    SP_TUNABLE_PARAM_F64(timeScaleMin, 0.07, 0.001, 1.0, 0.1, 1000)

    SP_TUNABLE_PARAM_CALLBACK(seeValuePawn, 100, 50, 200, 7.5, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueKnight, 450, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueBishop, 450, 300, 700, 25, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueRook, 650, 400, 1000, 30, updateSeeTables)
    SP_TUNABLE_PARAM_CALLBACK(seeValueQueen, 1250, 800, 1600, 40, updateSeeTables)

    SP_TUNABLE_PARAM(scalingValueKnight, 450, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueBishop, 450, 300, 700, 25)
    SP_TUNABLE_PARAM(scalingValueRook, 650, 400, 1000, 30)
    SP_TUNABLE_PARAM(scalingValueQueen, 1250, 800, 1600, 40)

    SP_TUNABLE_PARAM(materialScalingBase, 26500, 10000, 40000, 1500)
    SP_TUNABLE_PARAM(optimismBase, 2000, 0, 12000, 400)

    SP_TUNABLE_PARAM(optimismStretch, 100, 50, 200, 8)
    SP_TUNABLE_PARAM(optimismScale, 150, 75, 300, 12)

    SP_TUNABLE_PARAM(pawnCorrhistWeight, 133, 32, 384, 18)
    SP_TUNABLE_PARAM(stmNonPawnCorrhistWeight, 142, 32, 384, 18)
    SP_TUNABLE_PARAM(nstmNonPawnCorrhistWeight, 142, 32, 384, 18)
    SP_TUNABLE_PARAM(majorCorrhistWeight, 129, 32, 384, 18)

    SP_TUNABLE_PARAM(contCorrhist1Weight, 128, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist2Weight, 192, 32, 384, 18)
    SP_TUNABLE_PARAM(contCorrhist4Weight, 192, 32, 384, 18)

    SP_TUNABLE_PARAM(initialAspWindow, 16, 4, 50, 1)
    SP_TUNABLE_PARAM(aspWideningFactor, 17, 1, 24, 1)

    SP_TUNABLE_PARAM(goodNoisySeeOffset, 15, -384, 384, 40)

    SP_TUNABLE_PARAM(movepickButterflyWeight, 512, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickPieceToWeight, 512, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont1Weight, 1024, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont2Weight, 1024, 0, 4096, 100)
    SP_TUNABLE_PARAM(movepickCont4Weight, 512, 0, 4096, 100)

    SP_TUNABLE_PARAM(directCheckBonus, 8192, 2048, 16384, 716)
    SP_TUNABLE_PARAM(directCheckSeeThreshold, -100, -300, 0, 60)

    SP_TUNABLE_PARAM(maxTtCutoffBonus, 2576, 1024, 4096, 256)
    SP_TUNABLE_PARAM(ttCutoffBonusDepthScale, 280, 128, 512, 32)
    SP_TUNABLE_PARAM(ttCutoffBonusOffset, 432, 128, 768, 64)

    SP_TUNABLE_PARAM(evalPolicyScale, 10, 5, 20, 0.7)
    SP_TUNABLE_PARAM(evalPolicyMin, -2100, -4000, -1000, 150)
    SP_TUNABLE_PARAM(evalPolicyMax, 1700, 1000, 4000, 150)
    SP_TUNABLE_PARAM(evalPolicyOffset, 600, 0, 1200, 60)

    SP_TUNABLE_PARAM(hindsightReductionMargin, 200, 100, 400, 15)

    SP_TUNABLE_PARAM(rfpConstantMargin, 0, 0, 100, 5)
    SP_TUNABLE_PARAM(rfpLinearMargin, 71, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpQuadMargin, 0, 0, 25, 1.2)
    SP_TUNABLE_PARAM(rfpImprovingMargin, 71, 25, 150, 5)
    SP_TUNABLE_PARAM(rfpCorrplexityScale, 64, 16, 128, 5)
    SP_TUNABLE_PARAM(rfpFailFirmT, 512, 0, 1024, 51)

    SP_TUNABLE_PARAM(razoringMargin, 315, 100, 350, 40)

    SP_TUNABLE_PARAM(nmpBetaBaseMargin, 200, 100, 400, 15)
    SP_TUNABLE_PARAM(nmpBetaMarginDepthScale, 1280, 128, 2560, 32)
    SP_TUNABLE_PARAM(nmpBetaImprovingMargin, 40, 10, 80, 3)

    SP_TUNABLE_PARAM(probcutMargin, 303, 150, 400, 13)
    SP_TUNABLE_PARAM(probcutSeeScale, 17, 6, 24, 1)

    SP_TUNABLE_PARAM(searchButterflyWeight, 512, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchPieceToWeight, 512, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont1Weight, 1024, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont2Weight, 1024, 0, 4096, 100)
    SP_TUNABLE_PARAM(searchCont4Weight, 512, 0, 4096, 100)

    SP_TUNABLE_PARAM(lmrDepthTtpvScale, 96, 48, 192, 8)

    SP_TUNABLE_PARAM(fpMargin, 261, 120, 400, 20)
    SP_TUNABLE_PARAM(fpScale, 68, 40, 120, 4)
    SP_TUNABLE_PARAM(fpHistoryDivisor, 128, 32, 384, 18)

    SP_TUNABLE_PARAM(quietHistPruningMargin, -2314, -4000, -1000, 175)
    SP_TUNABLE_PARAM(quietHistPruningOffset, -1157, -4000, 4000, 400)

    SP_TUNABLE_PARAM(noisyHistPruningMargin, -1000, -4000, -1000, 175)
    SP_TUNABLE_PARAM(noisyHistPruningOffset, -1000, -4000, 4000, 400)

    SP_TUNABLE_PARAM(seePruningThresholdQuiet, -16, -80, -1, 12)
    SP_TUNABLE_PARAM(seePruningThresholdNoisy, -112, -120, -40, 20)

    SP_TUNABLE_PARAM(seePruningNoisyHistDivisor, 64, 12, 256, 10)

    SP_TUNABLE_PARAM(sBetaBaseMargin, 112, 32, 256, 12)
    SP_TUNABLE_PARAM(sBetaPrevPvMargin, 128, 32, 256, 12)

    SP_TUNABLE_PARAM(doubleExtBaseMargin, 11, -20, 50, 4)
    SP_TUNABLE_PARAM(doubleExtPvMargin, 150, 75, 300, 12)
    SP_TUNABLE_PARAM(doubleExtNoisyMargin, 0, 0, 200, 10)
    SP_TUNABLE_PARAM(tripleExtBaseMargin, 100, 30, 150, 8)
    SP_TUNABLE_PARAM(tripleExtPvMargin, 400, 200, 800, 30)
    SP_TUNABLE_PARAM(tripleExtNoisyMargin, 200, 100, 400, 15)

    SP_TUNABLE_PARAM(multicutFailFirmT, 512, 0, 1024, 51)

    SP_TUNABLE_PARAM(ldseMargin, 26, 10, 60, 3)
    SP_TUNABLE_PARAM(ldseDoubleExtMargin, 40, 20, 80, 3)

    SP_TUNABLE_PARAM_CALLBACK(quietLmrBase, 83, 50, 120, 5, updateQuietLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(quietLmrDivisor, 218, 100, 300, 10, updateQuietLmrTable)

    SP_TUNABLE_PARAM_CALLBACK(noisyLmrBase, -12, -50, 75, 5, updateNoisyLmrTable)
    SP_TUNABLE_PARAM_CALLBACK(noisyLmrDivisor, 248, 150, 350, 10, updateNoisyLmrTable)

    SP_TUNABLE_PARAM(lmrNonPvReductionScale, 131, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrTtpvReductionScale, 130, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrImprovingReductionScale, 148, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrCheckReductionScale, 111, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrCutnodeReductionScale, 257, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrTtpvFailLowReductionScale, 128, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrAlphaRaiseReductionScale, 64, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrTtMoveNoisyReductionScale, 128, 32, 384, 12)
    SP_TUNABLE_PARAM(lmrHighComplexityReductionScale, 128, 32, 384, 12)

    SP_TUNABLE_PARAM(lmrQuietHistoryScale, 387, 192, 768, 28)
    SP_TUNABLE_PARAM(lmrNoisyHistoryScale, 387, 192, 768, 28)

    SP_TUNABLE_PARAM(lmrHighComplexityThreshold, 70, 30, 120, 5)

    SP_TUNABLE_PARAM(lmrTtpvExtThreshold, -128, -384, 0, 19)

    SP_TUNABLE_PARAM(lmrDeeperBase, 38, 20, 100, 6)
    SP_TUNABLE_PARAM(lmrDeeperScale, 4, 3, 12, 1)

    SP_TUNABLE_PARAM(maxPostLmrContBonus, 2576, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContBonusDepthScale, 280, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContBonusOffset, 432, 128, 768, 64)

    SP_TUNABLE_PARAM(maxPostLmrContPenalty, 1239, 1024, 4096, 256)
    SP_TUNABLE_PARAM(postLmrContPenaltyDepthScale, 343, 128, 512, 32)
    SP_TUNABLE_PARAM(postLmrContPenaltyOffset, 161, 128, 768, 64)

    SP_TUNABLE_PARAM(maxButterflyHistory, 16384, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxPieceToHistory, 16384, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxConthist, 16384, 8192, 32768, 256)
    SP_TUNABLE_PARAM(maxNoisyHistory, 16384, 8192, 32768, 256)

    SP_TUNABLE_PARAM(maxQuietBonus, 2576, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBonusDepthScale, 280, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBonusOffset, 432, 128, 768, 64)

    SP_TUNABLE_PARAM(maxQuietPenalty, 1239, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietPenaltyDepthScale, 343, 128, 512, 32)
    SP_TUNABLE_PARAM(quietPenaltyOffset, 161, 128, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBonus, 2576, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBonusDepthScale, 280, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBonusOffset, 432, 128, 768, 64)

    SP_TUNABLE_PARAM(maxNoisyBmNoisyPenalty, 1239, 1024, 4096, 256)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyDepthScale, 343, 128, 512, 32)
    SP_TUNABLE_PARAM(noisyBmNoisyPenaltyOffset, 161, 128, 768, 64)

    SP_TUNABLE_PARAM(maxQuietBmNoisyPenalty, 1239, 1024, 4096, 256)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyDepthScale, 343, 128, 512, 32)
    SP_TUNABLE_PARAM(quietBmNoisyPenaltyOffset, 161, 128, 768, 64)

    SP_TUNABLE_PARAM(butterflyUpdateWeight, 1024, 512, 2048, 76)
    SP_TUNABLE_PARAM(pieceToUpdateWeight, 1024, 512, 2048, 76)

    SP_TUNABLE_PARAM(cont1UpdateWeight, 1024, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont2UpdateWeight, 1024, 512, 2048, 76)
    SP_TUNABLE_PARAM(cont4UpdateWeight, 1024, 512, 2048, 76)

    SP_TUNABLE_PARAM(contBaseButterflyWeight, 256, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBasePieceToWeight, 256, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont1Weight, 1024, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont2Weight, 1024, 0, 4096, 100)
    SP_TUNABLE_PARAM(contBaseCont4Weight, 512, 0, 4096, 100)

    SP_TUNABLE_PARAM(standPatFailFirmT, 512, 0, 1024, 51)

    SP_TUNABLE_PARAM(qsearchFpMargin, 135, 50, 400, 17)
    SP_TUNABLE_PARAM(qsearchSeeThreshold, -97, -2000, 200, 30)

    SP_TUNABLE_PARAM(threadWeightScoreOffset, 10, 0, 20, 1)

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
} // namespace stormphrax::tunable
