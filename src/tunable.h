/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"

#define PS_TUNE_SEARCH 1

namespace polaris::tunable
{
	namespace defaults
	{
		constexpr i32 MinAspDepth = 6;

		constexpr i32 MaxAspReduction = 3;

		constexpr Score InitialAspWindow = 10;
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

		constexpr i32 MaxFpDepth = 8;

		constexpr Score FpMargin = 250;
		constexpr Score FpScale = 60;

		constexpr i32 MinIirDepth = 4;
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

		i32 maxFpDepth{defaults::MaxFpDepth};

		Score fpMargin{defaults::FpMargin};
		Score fpScale{defaults::FpScale};

		i32 minIirDepth{defaults::MinIirDepth};
	};

#if PS_TUNE_SEARCH
	extern const TunableData &g_tunable;

#define PS_TUNABLE_PARAM(DefaultName, Name) inline auto Name() { return g_tunable.Name; }
#else
#define PS_TUNABLE_PARAM(DefaultName, Name) constexpr auto Name() { return defaults::DefaultName; }
#endif

	PS_TUNABLE_PARAM(MinAspDepth, minAspDepth)

	PS_TUNABLE_PARAM(MaxAspReduction, maxAspReduction)

	PS_TUNABLE_PARAM(InitialAspWindow, initialAspWindow)
	PS_TUNABLE_PARAM(MaxAspWindow, maxAspWindow)

	PS_TUNABLE_PARAM(MinNmpDepth, minNmpDepth)

	PS_TUNABLE_PARAM(NmpReductionBase, nmpReductionBase)
	PS_TUNABLE_PARAM(NmpReductionDepthScale, nmpReductionDepthScale)
	PS_TUNABLE_PARAM(NmpReductionEvalScale, nmpReductionEvalScale)
	PS_TUNABLE_PARAM(MaxNmpEvalReduction, maxNmpEvalReduction)

	PS_TUNABLE_PARAM(MinLmrDepth, minLmrDepth)

	PS_TUNABLE_PARAM(MaxRfpDepth, maxRfpDepth)
	PS_TUNABLE_PARAM(RfpMargin, rfpMargin)

	PS_TUNABLE_PARAM(MaxSeePruningDepth, maxSeePruningDepth)

	PS_TUNABLE_PARAM(QuietSeeThreshold, quietSeeThreshold)
	PS_TUNABLE_PARAM(NoisySeeThreshold, noisySeeThreshold)

	PS_TUNABLE_PARAM(MinSingularityDepth, minSingularityDepth)

	PS_TUNABLE_PARAM(SingularityDepthMargin, singularityDepthMargin)
	PS_TUNABLE_PARAM(SingularityDepthScale, singularityDepthScale)

	PS_TUNABLE_PARAM(MaxFpDepth, maxFpDepth)

	PS_TUNABLE_PARAM(FpMargin, fpMargin)
	PS_TUNABLE_PARAM(FpScale, fpScale)

	PS_TUNABLE_PARAM(MinIirDepth, minIirDepth)

#undef PS_TUNABLE_PARAM
}
