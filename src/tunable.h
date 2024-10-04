/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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

#include <string>
#include <array>
#include <functional>

#include "util/range.h"
#include "util/multi_array.h"

#ifndef SP_EXTERNAL_TUNE
	#define SP_EXTERNAL_TUNE 0
#endif

namespace stormphrax::tunable
{
	auto init() -> void;

	// [noisy][depth][legal moves]
	extern util::MultiArray<i32, 2, 256, 256> g_lmrTable;

	auto updateQuietLmrTable() -> void;
	auto updateNoisyLmrTable() -> void;

	// [improving][clamped depth]
	extern util::MultiArray<i32, 2, 16> g_lmpTable;
	auto updateLmpTable() -> void;

#define SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
	static_assert(Default >= Min); \
	static_assert(Default <= Max); \
	static_assert(Min < Max); \
	static_assert(Min + Step <= Max); \
	static_assert(Step >= 0.5);

#if SP_EXTERNAL_TUNE
	struct TunableParam
	{
		std::string name;
		std::string lowerName;
		i32 defaultValue;
		i32 value;
		util::Range<i32> range;
		f64 step;
		std::function<void()> callback;
	};

	auto addTunableParam(const std::string &name, i32 value,
		i32 min, i32 max, f64 step, std::function<void()> callback) -> TunableParam &;

	#define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) \
	    SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
		inline TunableParam &param_##Name = addTunableParam(#Name, Default, Min, Max, Step, nullptr); \
		inline auto Name() { return param_##Name.value; }

	#define SP_TUNABLE_PARAM_CALLBACK(Name, Default, Min, Max, Step, Callback) \
	    SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
		inline TunableParam &param_##Name = addTunableParam(#Name, Default, Min, Max, Step, Callback); \
		inline auto Name() { return param_##Name.value; }
#else
	#define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) \
		SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
		constexpr auto Name() -> i32 { return Default; }
	#define SP_TUNABLE_PARAM_CALLBACK(Name, Default, Min, Max, Step, Callback) \
		SP_TUNABLE_PARAM(Name, Default, Min, Max, Step)
#endif

	SP_TUNABLE_PARAM(defaultMovesToGo, 20, 12, 40, 1)
	SP_TUNABLE_PARAM(incrementScale, 83, 50, 100, 5)
	SP_TUNABLE_PARAM(softTimeScale, 69, 50, 100, 5)
	SP_TUNABLE_PARAM(hardTimeScale, 46, 20, 100, 5)

	SP_TUNABLE_PARAM(nodeTimeBase, 148, 100, 250, 10)
	SP_TUNABLE_PARAM(nodeTimeScale, 174, 100, 250, 10)
	SP_TUNABLE_PARAM(nodeTimeScaleMin, 108, 1, 1000, 100)

	SP_TUNABLE_PARAM(scoreTrendScoreScale, 400, 10, 1000, 50)
	SP_TUNABLE_PARAM(scoreTrendStretch, 72, 10, 200, 10)
	SP_TUNABLE_PARAM(scoreTrendScale, 40, 10, 90, 4)

	SP_TUNABLE_PARAM(timeScaleMin, 126, 1, 1000, 100)

	SP_TUNABLE_PARAM(pawnCorrhistWeight, 158, 32, 384, 18)
	SP_TUNABLE_PARAM(stmNonPawnCorrhistWeight, 150, 32, 384, 18)
	SP_TUNABLE_PARAM(nstmNonPawnCorrhistWeight, 98, 32, 384, 18)
	SP_TUNABLE_PARAM(majorCorrhistWeight, 109, 32, 384, 18)
	SP_TUNABLE_PARAM(contCorrhistWeight, 208, 32, 384, 18)

	SP_TUNABLE_PARAM(minAspDepth, 7, 1, 10, 1)
	SP_TUNABLE_PARAM(initialAspWindow, 4, 4, 50, 4)
	SP_TUNABLE_PARAM(aspWideningFactor, 15, 1, 24, 1)

	SP_TUNABLE_PARAM(maxAspFailHighReduction, 3, 1, 5, 1)

	SP_TUNABLE_PARAM(goodNoisySeeOffset, -47, -384, 384, 40)

	SP_TUNABLE_PARAM(ttReplacementDepthOffset, 3, 0, 6, 0.5)
	SP_TUNABLE_PARAM(ttReplacementPvOffset, 2, 0, 6, 0.5)

	SP_TUNABLE_PARAM(maxTtNonCutoffExtDepth, 9, 0, 12, 0.5)

	SP_TUNABLE_PARAM(minIirDepth, 6, 3, 6, 0.5)
	SP_TUNABLE_PARAM(iirBadEntryDepthOffset, 6, 2, 6, 0.5)

	SP_TUNABLE_PARAM(maxRfpDepth, 6, 4, 12, 0.5)
	SP_TUNABLE_PARAM(rfpMargin, 49, 25, 150, 5)

	SP_TUNABLE_PARAM(maxRazoringDepth, 3, 2, 6, 0.5)
	SP_TUNABLE_PARAM(razoringMargin, 335, 100, 350, 40)

	SP_TUNABLE_PARAM(minNmpDepth, 3, 3, 8, 0.5)
	SP_TUNABLE_PARAM(nmpBaseReduction, 3, 2, 5, 0.5)
	SP_TUNABLE_PARAM(nmpDepthReductionDiv, 6, 1, 8, 1)
	SP_TUNABLE_PARAM(nmpEvalReductionScale, 208, 50, 300, 25)
	SP_TUNABLE_PARAM(maxNmpEvalReduction, 4, 2, 5, 1)

	SP_TUNABLE_PARAM(minProbcutDepth, 7, 4, 8, 0.5)
	SP_TUNABLE_PARAM(probcutMargin, 304, 150, 400, 13)
	SP_TUNABLE_PARAM(probcutReduction, 2, 2, 5, 0.5)
	SP_TUNABLE_PARAM(probcutSeeScale, 12, 6, 24, 1)

	SP_TUNABLE_PARAM_CALLBACK(lmpBaseMoves, 5, 2, 5, 0.5, updateLmpTable)

	SP_TUNABLE_PARAM(maxFpDepth, 7, 4, 12, 0.5)
	SP_TUNABLE_PARAM(fpMargin, 284, 120, 350, 45)
	SP_TUNABLE_PARAM(fpScale, 50, 40, 80, 8)

	SP_TUNABLE_PARAM(maxHistoryPruningDepth, 3, 2, 8, 0.5)
	SP_TUNABLE_PARAM(historyPruningMargin, -2751, -4000, -1000, 175)
	SP_TUNABLE_PARAM(historyPruningOffset, -2083, -4000, 4000, 400)

	SP_TUNABLE_PARAM(seePruningThresholdQuiet, -28, -80, -15, 12)
	SP_TUNABLE_PARAM(seePruningThresholdNoisy, -115, -120, -40, 20)

	SP_TUNABLE_PARAM(minSeDepth, 7, 4, 10, 0.5)
	SP_TUNABLE_PARAM(seTtDepthMargin, 6, 2, 6, 1)
	SP_TUNABLE_PARAM(sBetaMargin, 12, 4, 64, 12)

	SP_TUNABLE_PARAM(multiExtLimit, 10, 4, 24, 4)

	SP_TUNABLE_PARAM(doubleExtMargin, 2, 0, 32, 5)
	SP_TUNABLE_PARAM(tripleExtMargin, 102, 10, 150, 7)

	SP_TUNABLE_PARAM(minLmrDepth, 4, 2, 5, 1)
	SP_TUNABLE_PARAM(lmrMinMovesRoot, 5, 0, 5, 1)
	SP_TUNABLE_PARAM(lmrMinMovesPv, 5, 0, 5, 1)
	SP_TUNABLE_PARAM(lmrMinMovesNonPv, 5, 0, 5, 1)

	SP_TUNABLE_PARAM_CALLBACK(quietLmrBase, 53, 50, 120, 15, updateQuietLmrTable)
	SP_TUNABLE_PARAM_CALLBACK(quietLmrDivisor, 239, 100, 300, 10, updateQuietLmrTable)

	SP_TUNABLE_PARAM_CALLBACK(noisyLmrBase, -47, -50, 75, 10, updateNoisyLmrTable)
	SP_TUNABLE_PARAM_CALLBACK(noisyLmrDivisor, 241, 150, 350, 10, updateNoisyLmrTable)

	SP_TUNABLE_PARAM(lmrHistoryDivisor, 8591, 4096, 16384, 650)

	SP_TUNABLE_PARAM(lmrDeeperBase, 29, 20, 100, 6)
	SP_TUNABLE_PARAM(lmrDeeperScale, 6, 3, 12, 1)

	SP_TUNABLE_PARAM(maxHistory, 16131, 8192, 32768, 256)

	SP_TUNABLE_PARAM(maxHistoryBonus, 2620, 1024, 4096, 256)
	SP_TUNABLE_PARAM(historyBonusDepthScale, 431, 128, 512, 32)
	SP_TUNABLE_PARAM(historyBonusOffset, 231, 128, 768, 64)

	SP_TUNABLE_PARAM(maxHistoryPenalty, 1056, 1024, 4096, 256)
	SP_TUNABLE_PARAM(historyPenaltyDepthScale, 431, 128, 512, 32)
	SP_TUNABLE_PARAM(historyPenaltyOffset, 134, 128, 768, 64)

	SP_TUNABLE_PARAM(qsearchFpMargin, 133, 50, 400, 17)
	SP_TUNABLE_PARAM(qsearchMaxMoves, 3, 1, 5, 0.5)
	SP_TUNABLE_PARAM(qsearchSeeThreshold, -147, -2000, 200, 100)

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
}
