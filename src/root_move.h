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

#include "types.h"

#include "core.h"
#include "pv.h"
#include "util/range.h"

namespace stormphrax::search {
    enum class GameResult {
        kNone,
        kWin,
        kDraw,
        kLoss,
    };

    struct RootMove {
        explicit RootMove(Move move) {
            pv.length = 1;
            pv.moves[0] = move;
        }

        Score score{-kScoreInf};
        Score windowScore{-kScoreInf};

        Score averageScore{-kScoreInf};
        Score averageSquaredScore{-kScoreInf};

        Score displayScore{-kScoreInf};
        Score previousScore{-kScoreInf};

        bool upperbound{false};
        bool lowerbound{false};

        GameResult tbWdl{GameResult::kNone};
        i32 tbRank{0};

        util::Range<Score> tbRange{-kScoreInf, kScoreInf};

        i32 searchedDepth{1};
        i32 seldepth{};

        PvList pv{};

        usize nodes{};

        [[nodiscard]] inline Move move() const {
            assert(pv.length > 0);
            return pv.moves[0];
        }

        inline void setTbStatus(GameResult wdl, i32 rank) {
            assert(wdl != GameResult::kNone);

            tbWdl = wdl;
            tbRank = rank;

            switch (wdl) {
                case GameResult::kWin:
                    tbRange = {kScoreTbWin, kScoreInf};
                    break;
                case GameResult::kDraw:
                    tbRange = {0, 0};
                    break;
                case GameResult::kLoss:
                    tbRange = {-kScoreInf, -kScoreTbWin};
                    break;
                default:
                    break;
            }
        }
    };
} // namespace stormphrax::search
