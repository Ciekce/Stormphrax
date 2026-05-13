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

#include "../types.h"

#include <array>

#include "../core.h"
#include "../correction.h"
#include "../position.h"
#include "nnue_state.h"

namespace stormphrax::eval {
    using Contempt = std::array<Score, Colors::kCount>;
    using Optimism = std::array<i32, Colors::kCount>;

    template <bool kCorrect = true>
    [[nodiscard]] Score adjustEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        const CorrectionHistoryTable* correction,
        i32 eval,
        i32* corrDelta = nullptr
    );

    [[nodiscard]] Score staticEval(const Position& pos, NnueState& nnueState, const Contempt& contempt = {});

    template <bool kCorrect = true>
    [[nodiscard]] Score adjustedStaticEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        NnueState& nnueState,
        const CorrectionHistoryTable* correction,
        const Contempt& contempt = {}
    );

    [[nodiscard]] Score staticEvalOnce(const Position& pos, const Contempt& contempt = {});
} // namespace stormphrax::eval
