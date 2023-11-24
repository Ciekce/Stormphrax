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
#include <array>
#include <functional>

#include "util/range.h"

#ifndef SP_EXTERNAL_TUNE
	#define SP_EXTERNAL_TUNE 0
#endif

namespace stormphrax::tunable
{
	extern std::array<std::array<i32, 256>, 256> g_lmrTable;
	auto updateLmrTable() -> void;

	auto init() -> void;

#define SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
	static_assert(Default >= Min); \
	static_assert(Default <= Max); \
	static_assert(Min < Max); \
	static_assert(Min + Step <= Max);

#if SP_EXTERNAL_TUNE
	struct TunableParam
	{
		std::string name;
		i32 defaultValue;
		i32 value;
		util::Range<i32> range;
		i32 step;
		std::function<void()> callback;
	};

	auto addTunableParam(const std::string &name, i32 value,
		i32 min, i32 max, i32 step, std::function<void()> callback) -> TunableParam &;

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
	SP_TUNABLE_PARAM(incrementScale, 74, 50, 100, 5)
	SP_TUNABLE_PARAM(softTimeScale, 60, 50, 100, 5)
	SP_TUNABLE_PARAM(hardTimeScale, 50, 20, 100, 5)

	SP_TUNABLE_PARAM(nodeTimeBase, 162, 100, 250, 10)
	SP_TUNABLE_PARAM(nodeTimeScale, 148, 100, 250, 10)

	SP_TUNABLE_PARAM(minAspDepth, 6, 1, 10, 1)

	SP_TUNABLE_PARAM(maxAspReduction, 3, 0, 5, 1)

	SP_TUNABLE_PARAM(initialAspWindow, 10, 8, 50, 4)
	SP_TUNABLE_PARAM(maxAspWindow, 371, 100, 1000, 100)
	SP_TUNABLE_PARAM(aspWideningFactor, 8, 1, 24, 1)

	SP_TUNABLE_PARAM(minNmpDepth, 3, 3, 8, 1)

	SP_TUNABLE_PARAM(nmpReductionBase, 3, 2, 5, 1)
	SP_TUNABLE_PARAM(nmpReductionDepthScale, 3, 1, 8, 1)
	SP_TUNABLE_PARAM(nmpReductionEvalScale, 200, 50, 300, 25)
	SP_TUNABLE_PARAM(maxNmpEvalReduction, 3, 2, 5, 1)

	SP_TUNABLE_PARAM(minNmpVerifDepth, 14, 6, 18, 1)
	SP_TUNABLE_PARAM(nmpVerifDepthFactor, 12, 8, 14, 1)

	SP_TUNABLE_PARAM(minLmrDepth, 3, 2, 5, 1)

	SP_TUNABLE_PARAM(lmrMinMovesPv, 3, 0, 5, 1)
	SP_TUNABLE_PARAM(lmrMinMovesNonPv, 2, 0, 5, 1)

	SP_TUNABLE_PARAM(maxRfpDepth, 8, 4, 12, 1)
	SP_TUNABLE_PARAM(rfpMargin, 75, 25, 150, 5)
	SP_TUNABLE_PARAM(rfpHistoryMargin, 256, 64, 1024, 64)

	SP_TUNABLE_PARAM(maxSeePruningDepth, 9, 4, 15, 1)

	SP_TUNABLE_PARAM(quietSeeThreshold, -50, -120, -20, 10)
	SP_TUNABLE_PARAM(noisySeeThreshold, -90, -120, -20, 10)

	SP_TUNABLE_PARAM(minSingularityDepth, 8, 4, 12, 1)

	SP_TUNABLE_PARAM(singularityDepthMargin, 3, 1, 4, 1)
	SP_TUNABLE_PARAM(singularityDepthScale, 32, 8, 32, 2)

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

	SP_TUNABLE_PARAM_CALLBACK(lmrBase, 77, 50, 120, 5, updateLmrTable)
	SP_TUNABLE_PARAM_CALLBACK(lmrDivisor, 236, 100, 300, 10, updateLmrTable)

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
}
