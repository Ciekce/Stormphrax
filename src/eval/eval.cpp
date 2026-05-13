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

#include "eval.h"

#include "../tunable.h"

namespace stormphrax::eval {
    namespace {
        [[nodiscard]] Score adjustStatic(const Position& pos, const Contempt& contempt, Score eval) {
            eval += contempt[pos.stm().idx()];
            return std::clamp(eval, -kScoreWin + 1, kScoreWin - 1);
        }
    } // namespace

    template <bool kCorrect>
    [[nodiscard]] Score adjustEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        const CorrectionHistoryTable* correction,
        i32 eval,
        i32* corrDelta
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

    template Score adjustEval<
        false>(const Position&, const Optimism&, std::span<const u64>, const CorrectionHistoryTable*, i32, i32*);
    template Score adjustEval<
        true>(const Position&, const Optimism&, std::span<const u64>, const CorrectionHistoryTable*, i32, i32*);

    Score staticEval(const Position& pos, NnueState& nnueState, const Contempt& contempt) {
        const auto eval = nnueState.evaluate(pos, pos.stm());
        return adjustStatic(pos, contempt, eval);
    }

    template <bool kCorrect>
    Score adjustedStaticEval(
        const Position& pos,
        const Optimism& optimism,
        std::span<const u64> keyHistory,
        NnueState& nnueState,
        const CorrectionHistoryTable* correction,
        const Contempt& contempt
    ) {
        const auto eval = staticEval(pos, nnueState, contempt);
        return adjustEval<kCorrect>(pos, optimism, keyHistory, correction, eval);
    }

    template Score adjustedStaticEval<false>(
        const Position&,
        const Optimism&,
        std::span<const u64>,
        NnueState&,
        const CorrectionHistoryTable*,
        const Contempt&
    );
    template Score adjustedStaticEval<true>(
        const Position&,
        const Optimism&,
        std::span<const u64>,
        NnueState&,
        const CorrectionHistoryTable*,
        const Contempt&
    );

    Score staticEvalOnce(const Position& pos, const Contempt& contempt) {
        const auto eval = NnueState::evaluateOnce(pos, pos.stm());
        return adjustStatic(pos, contempt, eval);
    }
} // namespace stormphrax::eval
