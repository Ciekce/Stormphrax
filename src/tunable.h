/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

#include "util/range.h"

#define SP_TUNE_SEARCH 0

namespace stormphrax::tunable
{
	namespace defaults
	{
		constexpr i32 MinAspDepth = 6;

		constexpr i32 MaxAspReduction = 3;

		constexpr Score InitialAspWindow = 16;
		constexpr Score MaxAspWindow = 500;

		constexpr i32 MinNmpDepth = 3;

		constexpr Score NmpReductionBase = 3;
		constexpr Score NmpReductionDepthScale = 3;
		constexpr Score NmpReductionEvalScale = 200;
		constexpr Score MaxNmpEvalReduction = 3;

		constexpr i32 MinLmrDepth = 3;

		constexpr i32 MaxRfpDepth = 8;
		constexpr Score RfpMargin = 75;

		constexpr i32 MaxSeePruningDepth = 9;

		constexpr Score QuietSeeThreshold = -50;
		constexpr Score NoisySeeThreshold = -90;

		constexpr i32 MinSingularityDepth = 8;

		constexpr i32 SingularityDepthMargin = 3;
		constexpr i32 SingularityDepthScale = 2;

		constexpr Score DoubleExtensionMargin = 22;
		constexpr i32 DoubleExtensionLimit = 5;

		constexpr i32 MaxFpDepth = 8;

		constexpr Score FpMargin = 250;
		constexpr Score FpScale = 60;

		constexpr i32 MinIirDepth = 4;

		constexpr i32 MaxLmpDepth = 8;
		constexpr i32 LmpMinMovesBase = 3;

		constexpr i32 MaxHistory = 16384;
		constexpr i32 MaxHistoryAdjustment = 1536;
		constexpr i32 HistoryDepthScale = 384;
		constexpr i32 HistoryOffset = 384;

		constexpr i32 HistoryLmrDivisor = 8192;
	}

#if SP_TUNE_SEARCH
	struct TunableParam
	{
		std::string name;
		i32 defaultValue;
		i32 value;
		util::Range<i32> range;
		i32 step;
	};

	TunableParam &addTunableParam(const std::string &name, i32 value, i32 min, i32 max, i32 step);

#define SP_TUNABLE_PARAM(DefaultName, Name, Min, Max, Step) \
inline TunableParam &param_##Name = addTunableParam(#Name, defaults::DefaultName, Min, Max, Step); \
inline auto Name() { return param_##Name.value; }
#else
#define SP_TUNABLE_PARAM(DefaultName, Name, Min, Max, Step) constexpr auto Name() { return defaults::DefaultName; }
#endif

	SP_TUNABLE_PARAM(MinAspDepth, minAspDepth, 1, 10, 1)

	SP_TUNABLE_PARAM(MaxAspReduction, maxAspReduction, 0, 5, 1)

	SP_TUNABLE_PARAM(InitialAspWindow, initialAspWindow, 8, 50, 4)
	SP_TUNABLE_PARAM(MaxAspWindow, maxAspWindow, 100, 1000, 100)

	SP_TUNABLE_PARAM(MinNmpDepth, minNmpDepth, 3, 8, 1)

	SP_TUNABLE_PARAM(NmpReductionBase, nmpReductionBase, 2, 5, 1)
	SP_TUNABLE_PARAM(NmpReductionDepthScale, nmpReductionDepthScale, 1, 8, 1)
	SP_TUNABLE_PARAM(NmpReductionEvalScale, nmpReductionEvalScale, 50, 300, 25)
	SP_TUNABLE_PARAM(MaxNmpEvalReduction, maxNmpEvalReduction, 2, 5, 1)

	SP_TUNABLE_PARAM(MinLmrDepth, minLmrDepth, 2, 5, 1)

	SP_TUNABLE_PARAM(MaxRfpDepth, maxRfpDepth, 4, 12, 1)
	SP_TUNABLE_PARAM(RfpMargin, rfpMargin, 25, 150, 5)

	SP_TUNABLE_PARAM(MaxSeePruningDepth, maxSeePruningDepth, 4, 15, 1)

	SP_TUNABLE_PARAM(QuietSeeThreshold, quietSeeThreshold, -120, -20, 10)
	SP_TUNABLE_PARAM(NoisySeeThreshold, noisySeeThreshold, -120, -20, 10)

	SP_TUNABLE_PARAM(MinSingularityDepth, minSingularityDepth, 4, 12, 1)

	SP_TUNABLE_PARAM(SingularityDepthMargin, singularityDepthMargin, 1, 4, 1)
	SP_TUNABLE_PARAM(SingularityDepthScale, singularityDepthScale, 1, 4, 1)

	SP_TUNABLE_PARAM(DoubleExtensionMargin, doubleExtensionMargin, 14, 30, 2)
	SP_TUNABLE_PARAM(DoubleExtensionLimit, doubleExtensionLimit, 3, 8, 1)

	SP_TUNABLE_PARAM(MaxFpDepth, maxFpDepth, 4, 12, 1)

	SP_TUNABLE_PARAM(FpMargin, fpMargin, 120, 350, 15)
	SP_TUNABLE_PARAM(FpScale, fpScale, 40, 80, 5)

	SP_TUNABLE_PARAM(MinIirDepth, minIirDepth, 3, 6, 1)

	SP_TUNABLE_PARAM(MaxLmpDepth, maxLmpDepth, 4, 12, 1)
	SP_TUNABLE_PARAM(LmpMinMovesBase, lmpMinMovesBase, 2, 5, 1)

	SP_TUNABLE_PARAM(MaxHistory, maxHistory, 8192, 32768, 256)
	SP_TUNABLE_PARAM(MaxHistoryAdjustment, maxHistoryAdjustment, 1024, 3072, 256)
	SP_TUNABLE_PARAM(HistoryDepthScale, historyDepthScale, 128, 512, 32)
	SP_TUNABLE_PARAM(HistoryOffset, historyOffset, 128, 768, 64)

	SP_TUNABLE_PARAM(HistoryLmrDivisor, historyLmrDivisor, 4096, 16384, 512)

#undef SP_TUNABLE_PARAM
}
