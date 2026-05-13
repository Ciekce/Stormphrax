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

#include <vector>

#include "nnue.h"

namespace stormphrax::eval {
    struct UpdateContext {
        NnueUpdates updates{};
        KingPair kings{};
    };

    struct BoardObserver {
        UpdateContext& ctx;

        void prepareKingMove(Color c, Square src, Square dst);

        void pieceAdded(const Position& pos, Piece piece, Square sq);
        void pieceRemoved(const Position& pos, Piece piece, Square sq);
        void pieceMutated(const Position& pos, Piece oldPiece, Piece newPiece, Square sq);
        void pieceMoved(const Position& pos, Piece piece, Square src, Square dst);
        void piecePromoted(const Position& pos, Piece oldPiece, Square src, Piece newPiece, Square dst);

        void finalize(const Position& pos);
    };

    struct UpdatableAccumulator {
        Accumulator psqAcc{};
        std::array<Accumulator, InputFeatureSet::kThreatInputs> threatAcc{};

        UpdateContext ctx{};

        std::array<bool, 2> psqDirty{};
        std::array<bool, 2> threatDirty{};

        inline void setPsqDirty() {
            psqDirty[0] = psqDirty[1] = true;
        }

        inline void setPsqUpdated(Color c) {
            assert(c != Colors::kNone);
            psqDirty[c.idx()] = false;
        }

        [[nodiscard]] inline bool isPsqDirty(Color c) const {
            assert(c != Colors::kNone);
            return psqDirty[c.idx()];
        }

        inline void setThreatDirty() {
            threatDirty[0] = threatDirty[1] = true;
        }

        inline void setThreatUpdated(Color c) {
            assert(c != Colors::kNone);
            threatDirty[c.idx()] = false;
        }

        [[nodiscard]] inline bool isThreatDirty(Color c) const {
            assert(c != Colors::kNone);
            return threatDirty[c.idx()];
        }
    };

    class NnueState {
    public:
        NnueState() {
            m_accumulatorStack.resize(256);
        }

        inline void setNetwork(const Network* network) {
            assert(network);
            m_network = network;
        }

        void reset(const Position& pos);

        [[nodiscard]] BoardObserver push();
        void pop();

        void applyImmediately(const UpdateContext& ctx, const Position& pos);

        [[nodiscard]] i32 evaluate(const Position& pos, Color stm);

        [[nodiscard]] static i32 evaluateOnce(const Position& pos, Color stm);

    private:
        std::vector<UpdatableAccumulator> m_accumulatorStack{};
        UpdatableAccumulator* m_top{};

        RefreshTable m_refreshTable{};

        const Network* m_network{};

        void ensureUpToDate(const Position& pos);
    };

    inline void BoardObserver::prepareKingMove(Color c, Square src, Square dst) {
        if (InputFeatureSet::refreshRequired(c, src, dst)) {
            ctx.updates.setPsqRefresh(c);
        }

        if constexpr (InputFeatureSet::kThreatInputs) {
            if ((src.file() >= kFileE) != (dst.file() >= kFileE)) {
                ctx.updates.setThreatRefresh(c);
            }
        }
    }

    inline void BoardObserver::pieceAdded(const Position& pos, Piece piece, Square sq) {
        ctx.updates.pushAdd(piece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnChange<true>(ctx.updates, pos, piece, sq);
        }
    }

    inline void BoardObserver::pieceRemoved(const Position& pos, Piece piece, Square sq) {
        ctx.updates.pushSub(piece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnChange<false>(ctx.updates, pos, piece, sq);
        }
    }

    inline void BoardObserver::pieceMutated(const Position& pos, Piece oldPiece, Piece newPiece, Square sq) {
        ctx.updates.pushSub(oldPiece, sq);
        ctx.updates.pushAdd(newPiece, sq);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnMutate(ctx.updates, pos, oldPiece, newPiece, sq);
        }
    }

    inline void BoardObserver::pieceMoved(const Position& pos, Piece piece, Square src, Square dst) {
        ctx.updates.pushSub(piece, src);
        ctx.updates.pushAdd(piece, dst);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnMove(ctx.updates, pos, piece, src, piece, dst);
        }
    }

    inline void BoardObserver::piecePromoted(
        const Position& pos,
        Piece oldPiece,
        Square src,
        Piece newPiece,
        Square dst
    ) {
        ctx.updates.pushSub(oldPiece, src);
        ctx.updates.pushAdd(newPiece, dst);
        if constexpr (InputFeatureSet::kThreatInputs) {
            updatePieceThreatsOnMove(ctx.updates, pos, oldPiece, src, newPiece, dst);
        }
    }

    inline void BoardObserver::finalize(const Position& pos) {
        ctx.kings = pos.kings();
    }
} // namespace stormphrax::eval
