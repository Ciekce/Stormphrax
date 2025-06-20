/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

#include <span>
#include <string_view>

#include "core.h"
#include "move.h"
#include "tunable.h"

namespace stormphrax::uci {
    constexpr auto kContemptRange = util::Range<i32>{-1000, 1000};

    i32 run();

#if SP_EXTERNAL_TUNE
    void printWfTuningParams(std::span<const std::string_view> params);
    void printCttTuningParams(std::span<const std::string_view> params);
    void printObTuningParams(std::span<const std::string_view> params);
#endif
} // namespace stormphrax::uci
