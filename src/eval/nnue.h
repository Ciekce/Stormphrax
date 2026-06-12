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

#include "../position.h"
#include "arch.h"
#include "nnue/arch/multilayer.h"
#include "nnue/input.h"
#include "nnue/network.h"

namespace stormphrax::eval {
    using FeatureTransformer = nnue::FeatureTransformer<i16, i8, kL1Size, InputFeatureSet>;
    using Network = nnue::PerspectiveNetwork<FeatureTransformer, OutputBucketing, LayeredArch>;

    using Accumulator = FeatureTransformer::Accumulator;
    using RefreshTable = FeatureTransformer::RefreshTable;

    using NnueUpdates = InputFeatureSet::Updates;

    void init();
    void shutdown();

    [[nodiscard]] bool isNetworkLoaded();

    const Network* getNetwork(u32 numaId);

    [[nodiscard]] std::string_view defaultNetworkName();

    void addThreatFeatures(const Network& network, std::span<i16, kL1Size> acc, Color c, const Position& pos);

    template <bool kAdd>
    void updatePieceThreatsOnChange(NnueUpdates& updates, const Position& pos, Piece piece, Square sq);
    void updatePieceThreatsOnMutate(
        NnueUpdates& updates,
        const Position& pos,
        Piece oldPiece,
        Piece newPiece,
        Square sq
    );
    void updatePieceThreatsOnMove(
        NnueUpdates& updates,
        const Position& pos,
        Piece oldPiece,
        Square src,
        Piece newPiece,
        Square dst
    );
} // namespace stormphrax::eval
