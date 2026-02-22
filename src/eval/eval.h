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

#include "../types.h"

#include <array>

#include "../core.h"
#include "../correction.h"
#include "../position/position.h"
#include "nnue.h"

namespace stormphrax::eval {
    // black, white
    using Contempt = std::array<Score, 2>;

    template <bool kCorrect = true>
    inline Score adjustEval(
        const Position& pos,
        std::span<const u64> keyHistory,
        const CorrectionHistoryTable* correction,
        i32 eval,
        i32* corrDelta = nullptr
    ) {
        if constexpr (kCorrect) {
            const auto corrected = correction->correct(pos, keyHistory, eval);

            if (corrDelta) {
                *corrDelta = std::abs(eval - corrected);
            }

            eval = corrected;
        }

        return std::clamp(eval, -kScoreWin + 1, kScoreWin - 1);
    }

    template <bool kScale>
    inline Score adjustStatic(const Position& pos, const Contempt& contempt, Score eval) {
        SP_UNUSED(kScale);
        eval += contempt[pos.stm().idx()];
        return std::clamp(eval, -kScoreWin + 1, kScoreWin - 1);
    }

    template <bool kScale = true>
    inline Score staticEval(const Position& pos, NnueState& nnueState, const Contempt& contempt = {}) {
        const auto eval = nnueState.evaluate(pos.boards(), pos.kings(), pos.stm());
        return adjustStatic<kScale>(pos, contempt, eval);
    }

    template <bool kCorrect = true>
    inline Score adjustedStaticEval(
        const Position& pos,
        std::span<const u64> keyHistory,
        NnueState& nnueState,
        const CorrectionHistoryTable* correction,
        const Contempt& contempt = {}
    ) {
        const auto eval = staticEval(pos, nnueState, contempt);
        return adjustEval<kCorrect>(pos, keyHistory, correction, eval);
    }

    template <bool kScale = true>
    inline Score staticEvalOnce(const Position& pos, const Contempt& contempt = {}) {
        const auto eval = NnueState::evaluateOnce(pos.boards(), pos.kings(), pos.stm());
        return adjustStatic<kScale>(pos, contempt, eval);
    }
} // namespace stormphrax::eval
