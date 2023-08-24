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

	struct TunableData
	{
		i32 minAspDepth{defaults::MinAspDepth};

		i32 maxAspReduction{defaults::MaxAspReduction};

		Score initialAspWindow{defaults::InitialAspWindow};
		Score maxAspWindow{defaults::MaxAspWindow};

		i32 minNmpDepth{defaults::MinNmpDepth};

		Score nmpReductionBase{defaults::NmpReductionBase};
		Score nmpReductionDepthScale{defaults::NmpReductionDepthScale};
		Score nmpReductionEvalScale{defaults::NmpReductionEvalScale};
		Score maxNmpEvalReduction{defaults::MaxNmpEvalReduction};

		i32 minLmrDepth{defaults::MinLmrDepth};

		i32 maxRfpDepth{defaults::MaxRfpDepth};
		Score rfpMargin{defaults::RfpMargin};

		i32 maxSeePruningDepth{defaults::MaxSeePruningDepth};

		Score quietSeeThreshold{defaults::QuietSeeThreshold};
		Score noisySeeThreshold{defaults::NoisySeeThreshold};

		i32 minSingularityDepth{defaults::MinSingularityDepth};

		i32 singularityDepthMargin{defaults::SingularityDepthMargin};
		i32 singularityDepthScale{defaults::SingularityDepthScale};

		i32 doubleExtensionMargin{defaults::DoubleExtensionMargin};
		i32 doubleExtensionLimit{defaults::DoubleExtensionLimit};

		i32 maxFpDepth{defaults::MaxFpDepth};

		Score fpMargin{defaults::FpMargin};
		Score fpScale{defaults::FpScale};

		i32 minIirDepth{defaults::MinIirDepth};

		i32 maxLmpDepth{defaults::MaxLmpDepth};
		i32 lmpMinMovesBase{defaults::LmpMinMovesBase};

		i32 maxHistory{defaults::MaxHistory};
		i32 maxHistoryAdjustment{defaults::MaxHistoryAdjustment};
		i32 historyDepthScale{defaults::HistoryDepthScale};
		i32 historyOffset{defaults::HistoryOffset};

		i32 historyLmrDivisor{defaults::HistoryLmrDivisor};
	};

#if SP_TUNE_SEARCH
	extern const TunableData &g_tunable;

#define SP_TUNABLE_PARAM(DefaultName, Name) inline auto Name() { return g_tunable.Name; }
#else
#define SP_TUNABLE_PARAM(DefaultName, Name) constexpr auto Name() { return defaults::DefaultName; }
#endif

	SP_TUNABLE_PARAM(MinAspDepth, minAspDepth)

	SP_TUNABLE_PARAM(MaxAspReduction, maxAspReduction)

	SP_TUNABLE_PARAM(InitialAspWindow, initialAspWindow)
	SP_TUNABLE_PARAM(MaxAspWindow, maxAspWindow)

	SP_TUNABLE_PARAM(MinNmpDepth, minNmpDepth)

	SP_TUNABLE_PARAM(NmpReductionBase, nmpReductionBase)
	SP_TUNABLE_PARAM(NmpReductionDepthScale, nmpReductionDepthScale)
	SP_TUNABLE_PARAM(NmpReductionEvalScale, nmpReductionEvalScale)
	SP_TUNABLE_PARAM(MaxNmpEvalReduction, maxNmpEvalReduction)

	SP_TUNABLE_PARAM(MinLmrDepth, minLmrDepth)

	SP_TUNABLE_PARAM(MaxRfpDepth, maxRfpDepth)
	SP_TUNABLE_PARAM(RfpMargin, rfpMargin)

	SP_TUNABLE_PARAM(MaxSeePruningDepth, maxSeePruningDepth)

	SP_TUNABLE_PARAM(QuietSeeThreshold, quietSeeThreshold)
	SP_TUNABLE_PARAM(NoisySeeThreshold, noisySeeThreshold)

	SP_TUNABLE_PARAM(MinSingularityDepth, minSingularityDepth)

	SP_TUNABLE_PARAM(SingularityDepthMargin, singularityDepthMargin)
	SP_TUNABLE_PARAM(SingularityDepthScale, singularityDepthScale)

	SP_TUNABLE_PARAM(DoubleExtensionMargin, doubleExtensionMargin)
	SP_TUNABLE_PARAM(DoubleExtensionLimit, doubleExtensionLimit)

	SP_TUNABLE_PARAM(MaxFpDepth, maxFpDepth)

	SP_TUNABLE_PARAM(FpMargin, fpMargin)
	SP_TUNABLE_PARAM(FpScale, fpScale)

	SP_TUNABLE_PARAM(MinIirDepth, minIirDepth)

	SP_TUNABLE_PARAM(MaxLmpDepth, maxLmpDepth)
	SP_TUNABLE_PARAM(LmpMinMovesBase, lmpMinMovesBase)

	SP_TUNABLE_PARAM(MaxHistory, maxHistory)
	SP_TUNABLE_PARAM(MaxHistoryAdjustment, maxHistoryAdjustment)
	SP_TUNABLE_PARAM(HistoryDepthScale, historyDepthScale)
	SP_TUNABLE_PARAM(HistoryOffset, historyOffset)

	SP_TUNABLE_PARAM(HistoryLmrDivisor, historyLmrDivisor)

#undef SP_TUNABLE_PARAM
}
