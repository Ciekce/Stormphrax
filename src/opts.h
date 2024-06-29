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

#include "wdl.h"
#include "util/range.h"

namespace stormphrax
{
	namespace opts
	{
		constexpr u32 DefaultThreadCount = 1;
		constexpr auto ThreadCountRange = util::Range<u32>{1,  2048};

		constexpr i32 DefaultNormalizedContempt = 0;

		struct GlobalOptions
		{
			u32 threads{DefaultThreadCount};

			bool chess960{false};
			bool showWdl{true};
			bool showCurrMove{false};

			bool enableWeirdTcs{false};

			bool syzygyEnabled{false};
			i32 syzygyProbeDepth{1};
			i32 syzygyProbeLimit{7};

			i32 contempt{wdl::unnormalizeScoreMove32(DefaultNormalizedContempt)};
		};

		auto mutableOpts() -> GlobalOptions &;
	}

	extern const opts::GlobalOptions &g_opts;
}
