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

#include "threats.h"

#include <limits>
#include <utility>

#include "../../../attacks/attacks.h"
#include "../../../core.h"
#include "../../../util/multi_array.h"
#include "../../arch.h"

namespace stormphrax::eval::nnue::features::threats {
    namespace {
        constexpr util::MultiArray<i32, PieceTypes::kCount, PieceTypes::kCount> kPieceTargetMapPpThreats = {{
            // clang-format off
            { 0,  1, -1,  2, -1, -1},
            { 0,  1,  2,  3,  4, -1},
            { 0,  1,  2,  3, -1, -1},
            { 0,  1,  2,  3, -1, -1},
            { 0,  1,  2,  3,  4, -1},
            {-1, -1, -1, -1, -1, -1},
            // clang-format on
        }};

        constexpr util::MultiArray<i32, PieceTypes::kCount, PieceTypes::kCount> kPieceTargetMapNoPpThreats = {{
            // clang-format off
            {-1,  0, -1,  2, -1, -1},
            { 0,  1,  2,  3,  4, -1},
            { 0,  1,  2,  3, -1, -1},
            { 0,  1,  2,  3, -1, -1},
            { 0,  1,  2,  3,  4, -1},
            {-1, -1, -1, -1, -1, -1},
            // clang-format on
        }};

        constexpr auto kPieceTargetMap =
            InputFeatureSet::kPawnPawnThreats ? kPieceTargetMapPpThreats : kPieceTargetMapNoPpThreats;

        constexpr std::array kPieceTargetCount = [] {
            std::array<i32, PieceTypes::kCount> counts{};

            for (usize src = 0; src < PieceTypes::kCount; ++src) {
                i32 count = 0;

                for (usize dst = 0; dst < PieceTypes::kCount; ++dst) {
                    if (kPieceTargetMap[src][dst] >= 0) {
                        ++count;
                    }
                }

                counts[src] = 2 * count;
            }

            return counts;
        }();

        [[nodiscard]] constexpr util::MultiArray<u8, Squares::kCount, Squares::kCount> generatePieceIndices(
            Piece piece
        ) {
            util::MultiArray<u8, Squares::kCount, Squares::kCount> dst{};

            for (u8 fromIdx = 0; fromIdx < Squares::kCount; ++fromIdx) {
                const auto from = Square::fromRaw(fromIdx);
                const auto pseudoAttacks = attacks::getPseudoAttacks(piece, from);
                for (u8 toIdx = 0; toIdx < Squares::kCount; ++toIdx) {
                    const auto to = Square::fromRaw(toIdx);
                    const auto mask = pseudoAttacks & (to.bit() - 1);
                    dst[from.idx()][to.idx()] = mask.popcount();
                }
            }

            return dst;
        }

        constexpr auto kPieceIndices = [] {
            util::MultiArray<u8, Pieces::kCount, Squares::kCount, Squares::kCount> dst{};

            dst[Pieces::kBlackPawn.idx()] = generatePieceIndices(Pieces::kBlackPawn);
            dst[Pieces::kWhitePawn.idx()] = generatePieceIndices(Pieces::kWhitePawn);

            for (u32 ptIdx = 1; ptIdx < PieceTypes::kCount; ++ptIdx) {
                const auto pt = PieceType::fromRaw(ptIdx);
                const auto indices = generatePieceIndices(pt.withColor(Colors::kBlack));
                dst[pt.withColor(Colors::kBlack).idx()] = indices;
                dst[pt.withColor(Colors::kWhite).idx()] = indices;
            }

            return dst;
        }();

        constexpr auto kOffsets = [] {
            struct {
                std::array<std::pair<i32, i32>, Pieces::kCount> indices{};
                util::MultiArray<u32, Pieces::kCount, Squares::kCount> offsets{};
            } dst{};

            i32 offset{};

            for (const auto color : {Colors::kWhite, Colors::kBlack}) {
                for (u8 ptIdx = 0; ptIdx < PieceTypes::kCount; ++ptIdx) {
                    const auto piece = PieceType::fromRaw(ptIdx).withColor(color);

                    i32 pieceOffset{};
                    for (u8 sqIdx = 0; sqIdx < Squares::kCount; ++sqIdx) {
                        const auto sq = Square::fromRaw(sqIdx);
                        dst.offsets[piece.idx()][sq.idx()] = pieceOffset;
                        if (piece.type() != PieceTypes::kPawn || (sq.rank() > kRank1 && sq.rank() < kRank8)) {
                            const auto attacks = attacks::getPseudoAttacks(piece.flipColor(), sq);
                            pieceOffset += attacks.popcount();
                        }
                    }

                    dst.indices[piece.idx()] = {pieceOffset, offset};
                    offset += kPieceTargetCount[piece.type().idx()] * pieceOffset;
                }
            }

            return dst;
        }();

        constexpr auto kAttackIndices = [] {
            util::MultiArray<i32, Pieces::kCount, Pieces::kCount, 2> dst{};

            for (u8 attackerIdx = 0; attackerIdx < Pieces::kCount; ++attackerIdx) {
                const auto attacker = Piece::fromRaw(attackerIdx);
                for (u8 attackedIdx = 0; attackedIdx < Pieces::kCount; ++attackedIdx) {
                    const auto attacked = Piece::fromRaw(attackedIdx);

                    const bool enemy = attacker.color() != attacked.color();
                    const auto map = kPieceTargetMap[attacker.type().idx()][attacked.type().idx()];

                    const bool semiExcluded =
                        attacker.type() == attacked.type() && (enemy || attacker.type() != PieceTypes::kPawn);
                    const bool excluded = map < 0;

                    const auto [pieceOffset, offset] = kOffsets.indices[attacker.idx()];

                    const auto feature =
                        offset
                        + (attacked.color().flip().raw() * (kPieceTargetCount[attacker.type().idx()] / 2) + map)
                              * pieceOffset;

                    dst[attacker.idx()][attacked.idx()][0] = excluded ? std::numeric_limits<i32>::min() : feature;
                    dst[attacker.idx()][attacked.idx()][1] =
                        excluded || semiExcluded ? std::numeric_limits<i32>::min() : feature;
                }
            }

            return dst;
        }();
    } // namespace

    i32 threatFeatureIndex(
        Color c,
        Square kingSq,
        Piece attacker,
        Square attackerSq,
        Piece attacked,
        Square attackedSq
    ) {
        if (c == Colors::kBlack) {
            attacker = attacker.flipColor();
            attacked = attacked.flipColor();

            attackerSq = attackerSq.flipRank();
            attackedSq = attackedSq.flipRank();
        }

        if (kingSq.file() >= kFileE) {
            attackerSq = attackerSq.flipFile();
            attackedSq = attackedSq.flipFile();
        }

        const bool forwards = attackerSq.raw() < attackedSq.raw();

        const auto attackIdx = kAttackIndices[attacker.idx()][attacked.idx()][forwards];
        const auto offset = kOffsets.offsets[attacker.idx()][attackerSq.idx()];
        const auto pieceIdx = kPieceIndices[attacker.idx()][attackerSq.idx()][attackedSq.idx()];

        return InputFeatureSet::kThreatOffset + attackIdx + offset + pieceIdx;
    }

    i32 ppFeatureIndex(Color c, Square kingSq, Color a, Square aSq, Color b, Square bSq) {
        if (c == Colors::kBlack) {
            aSq = aSq.flipRank();
            bSq = bSq.flipRank();
        }

        if (kingSq.file() >= kFileE) {
            aSq = aSq.flipFile();
            bSq = bSq.flipFile();
        }

        const auto pawnId = [&](Color pawnColor, Square sq) -> i32 {
            const auto offset = c != pawnColor ? 48 : 0;
            return offset + sq.idx() - 8;
        };

        const auto aId = pawnId(a, aSq);
        const auto bId = pawnId(b, bSq);

        const auto hi = std::max(aId, bId);
        const auto lo = std::min(aId, bId);

        return hi * (hi - 1) / 2 + lo;
    }
} // namespace stormphrax::eval::nnue::features::threats
