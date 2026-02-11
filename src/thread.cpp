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

#include "thread.h"

#include <algorithm>

namespace stormphrax::search {
    std::pair<Position, ThreadPosGuard<false>> ThreadData::applyNullmove(const Position& pos, i32 ply) {
        assert(ply <= kMaxDepth);

        stack[ply].move = kNullMove;
        conthist[ply] = &history.contTable(Pieces::kWhitePawn, Squares::kA1);
        contMoves[ply] = {Pieces::kNone, Squares::kNone};

        keyHistory.push_back(pos.key());

        return std::pair<Position, ThreadPosGuard<false>>{
            std::piecewise_construct,
            std::forward_as_tuple(pos.applyNullMove()),
            std::forward_as_tuple(keyHistory, nnueState)
        };
    }

    std::pair<Position, ThreadPosGuard<true>> ThreadData::applyMove(const Position& pos, i32 ply, Move move) {
        assert(ply <= kMaxDepth);

        const auto moving = pos.boards().pieceOn(move.fromSq());

        stack[ply].move = move;
        conthist[ply] = &history.contTable(moving, move.toSq());
        contMoves[ply] = {moving, move.toSq()};

        keyHistory.push_back(pos.key());

        return std::pair<Position, ThreadPosGuard<true>>{
            std::piecewise_construct,
            std::forward_as_tuple(pos.applyMove<NnueUpdateAction::kQueue>(move, &nnueState)),
            std::forward_as_tuple(keyHistory, nnueState)
        };
    }

    RootMove* ThreadData::findRootMove(Move move) {
        for (u32 idx = pvIdx; idx < rootMoves.size(); ++idx) {
            auto& rootMove = rootMoves[idx];
            assert(rootMove.pv.length > 0);

            if (move == rootMove.pv.moves[0]) {
                return &rootMove;
            }
        }

        return nullptr;
    }

    void ThreadData::sortRootMoves() {
        std::ranges::stable_sort(rootMoves, [](const RootMove& a, const RootMove& b) { return a.score > b.score; });
    }

    void ThreadData::sortRemainingRootMoves() {
        std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.end(), [](const RootMove& a, const RootMove& b) {
            return a.score > b.score;
        });
    }
} // namespace stormphrax::search
