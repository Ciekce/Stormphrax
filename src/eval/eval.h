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
#include "../position/position.h"
#include "../see.h"
#include "../tunable.h"
#include "nnue.h"

namespace stormphrax::eval {
    using Contempt = std::array<Score, Colors::kCount>;
    using Optimism = std::array<i32, Colors::kCount>;

    template <bool kCorrect = true>
    inline Score adjustEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        const CorrectionHistoryTable* correction,
        i32 eval,
        i32* corrDelta = nullptr
    ) {
        using namespace tunable;

        const auto bbs = pos.bbs();

        const auto npMaterial = scalingValuePawn() * bbs.pawns().popcount()     //
                              + scalingValueKnight() * bbs.knights().popcount() //
                              + scalingValueBishop() * bbs.bishops().popcount() //
                              + scalingValueRook() * bbs.rooks().popcount()     //
                              + scalingValueQueen() * bbs.queens().popcount();

        eval = (eval * (materialScalingBase() + npMaterial) + optimism[pos.stm().idx()] * (optimismBase() + npMaterial))
             / 32768;

        eval = eval * (200 - pos.halfmove()) / 200;

        if constexpr (kCorrect) {
            const auto corrected = correction->correct(pos, keyHistory, eval);

            if (corrDelta) {
                *corrDelta = std::abs(eval - corrected);
            }

            eval = corrected;
        }

        return std::clamp(eval, -kScoreWin + 1, kScoreWin - 1);
    }

    inline Score adjustStatic(const Position& pos, const Contempt& contempt, Score eval) {
        eval += contempt[pos.stm().idx()];
        return std::clamp(eval, -kScoreWin + 1, kScoreWin - 1);
    }

    inline Score staticEval(const Position& pos, NnueState& nnueState, const Contempt& contempt = {}) {
        const auto eval = nnueState.evaluate(pos.boards(), pos.kings(), pos.stm());
        return adjustStatic(pos, contempt, eval);
    }

    template <bool kCorrect = true>
    inline Score adjustedStaticEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        NnueState& nnueState,
        const CorrectionHistoryTable* correction,
        const Contempt& contempt = {}
    ) {
        const auto eval = staticEval(pos, nnueState, contempt);
        return adjustEval<kCorrect>(pos, optimism, keyHistory, correction, eval);
    }

    inline Score staticEvalOnce(const Position& pos, const Contempt& contempt = {}) {
        const auto eval = NnueState::evaluateOnce(pos.boards(), pos.kings(), pos.stm());
        return adjustStatic(pos, contempt, eval);
    }
} // namespace stormphrax::eval
