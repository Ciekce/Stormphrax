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

#define PS_TUNE_SEARCH 0

namespace polaris::tunable
{
	namespace defaults
	{
		constexpr i32 MaxRfpDepth = 8;
		constexpr i32 RfpMargin = 75;
	}

	struct TunableData
	{
		i32 maxRfpDepth{defaults::MaxRfpDepth};
		i32 rfpMargin{defaults::RfpMargin};
	};

#if PS_TUNE_SEARCH
	inline i32 maxRfpDepth(const TunableData &data)
	{
		return data.maxRfpDepth;
	}

	inline i32 rfpMargin(const TunableData &data)
	{
		return data.rfpMargin;
	}
#else
	inline i32 maxRfpDepth([[maybe_unused]] const TunableData &data)
	{
		return defaults::MaxRfpDepth;
	}

	inline i32 rfpMargin([[maybe_unused]] const TunableData &data)
	{
		return defaults::RfpMargin;
	}
#endif
}
