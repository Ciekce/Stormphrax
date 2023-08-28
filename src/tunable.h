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

#define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) \
inline TunableParam &param_##Name = addTunableParam(#Name, Default, Min, Max, Step); \
inline auto Name() { return param_##Name.value; }
#else
#define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) constexpr auto Name() -> i32 { return Default; }
#endif

	SP_TUNABLE_PARAM(minAspDepth, 6, 1, 10, 1)

	SP_TUNABLE_PARAM(maxAspReduction, 3, 0, 5, 1)

	SP_TUNABLE_PARAM(initialAspWindow, 16, 8, 50, 4)
	SP_TUNABLE_PARAM(maxAspWindow, 500, 100, 1000, 100)

	SP_TUNABLE_PARAM(minNmpDepth, 3, 3, 8, 1)

	SP_TUNABLE_PARAM(nmpReductionBase, 3, 2, 5, 1)
	SP_TUNABLE_PARAM(nmpReductionDepthScale, 3, 1, 8, 1)
	SP_TUNABLE_PARAM(nmpReductionEvalScale, 200, 50, 300, 25)
	SP_TUNABLE_PARAM(maxNmpEvalReduction, 3, 2, 5, 1)

	SP_TUNABLE_PARAM(minLmrDepth, 3, 2, 5, 1)

	SP_TUNABLE_PARAM(maxRfpDepth, 8, 4, 12, 1)
	SP_TUNABLE_PARAM(rfpMargin, 75, 25, 150, 5)

	SP_TUNABLE_PARAM(maxSeePruningDepth, 9, 4, 15, 1)

	SP_TUNABLE_PARAM(quietSeeThreshold, -50, -120, -20, 10)
	SP_TUNABLE_PARAM(noisySeeThreshold, -90, -120, -20, 10)

	SP_TUNABLE_PARAM(minSingularityDepth, 8, 4, 12, 1)

	SP_TUNABLE_PARAM(singularityDepthMargin, 3, 1, 4, 1)
	SP_TUNABLE_PARAM(singularityDepthScale, 2, 1, 4, 1)

	SP_TUNABLE_PARAM(doubleExtensionMargin, 22, 14, 30, 2)
	SP_TUNABLE_PARAM(doubleExtensionLimit, 5, 3, 8, 1)

	SP_TUNABLE_PARAM(maxFpDepth, 8, 4, 12, 1)

	SP_TUNABLE_PARAM(fpMargin, 250, 120, 350, 15)
	SP_TUNABLE_PARAM(fpScale, 60, 40, 80, 5)

	SP_TUNABLE_PARAM(minIirDepth, 4, 3, 6, 1)

	SP_TUNABLE_PARAM(maxLmpDepth, 8, 4, 12, 1)
	SP_TUNABLE_PARAM(lmpMinMovesBase, 3, 2, 5, 1)

	SP_TUNABLE_PARAM(maxHistory, 16384, 8192, 32768, 256)
	SP_TUNABLE_PARAM(maxHistoryAdjustment, 1536, 1024, 3072, 256)
	SP_TUNABLE_PARAM(historyDepthScale, 384, 128, 512, 32)
	SP_TUNABLE_PARAM(historyOffset, 384, 128, 768, 64)

	SP_TUNABLE_PARAM(historyLmrDivisor, 8192, 4096, 16384, 512)

#undef SP_TUNABLE_PARAM
}
