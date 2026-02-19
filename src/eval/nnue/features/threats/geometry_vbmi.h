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

#include <array>
#include <bit>
#include <tuple>

#include <immintrin.h>

#include "../../../../types.h"

namespace stormphrax::eval::nnue::features::threats::geometry {

    struct Vector {
        __m512i raw;

        [[nodiscard]] Vector flip() const {
            return {_mm512_shuffle_i64x2(raw, raw, 0b01001110)};
        }
    };

    struct Permutation {
        Vector indexes;
        Bitrays valid;
    };

    [[nodiscard]] inline Permutation permutationFor(Square focus) {
        static constexpr std::array<Vector, Squares::kCount> permutations = generatePermutations<Vector>();

        const auto indexes = permutations[focus.idx()];
        const auto valid = _mm512_testn_epi8_mask(indexes.raw, _mm512_set1_epi8(0x80));
        return Permutation{indexes, valid};
    }

    [[nodiscard]] inline std::tuple<Vector, Vector> permuteMailbox(
        const Permutation& permutation,
        const std::array<Piece, Squares::kCount>& mailbox
    ) {
        const auto lut = _mm512_broadcast_i32x4(kPieceToBitTable);

        const auto maskedMailbox = std::bit_cast<__m512i>(mailbox);
        const auto permuted = _mm512_permutexvar_epi8(permutation.indexes.raw, maskedMailbox);
        const auto bits = _mm512_maskz_shuffle_epi8(permutation.valid, lut, permuted);
        return {{permuted}, {bits}};
    }

    [[nodiscard]] inline std::tuple<Vector, Vector> permuteMailbox(
        const Permutation& permutation,
        const std::array<Piece, Squares::kCount>& mailbox,
        Square ignore
    ) {
        const auto lut = _mm512_broadcast_i32x4(kPieceToBitTable);

        const auto maskedMailbox = _mm512_mask_blend_epi8(
            ignore.bit(),
            std::bit_cast<__m512i>(mailbox),
            _mm512_set1_epi8(Pieces::kNone.idx())
        );
        const auto permuted = _mm512_permutexvar_epi8(permutation.indexes.raw, maskedMailbox);
        const auto bits = _mm512_maskz_shuffle_epi8(permutation.valid, lut, permuted);
        return {{permuted}, {bits}};
    }

    [[nodiscard]] inline Bitrays closestOccupied(Vector bits) {
        const Bitrays occupied = _mm512_test_epi8_mask(bits.raw, bits.raw);
        const Bitrays o = occupied | 0x8181818181818181;
        return (o ^ (o - 0x0303030303030303)) & occupied;
    }

    [[nodiscard]] inline Bitrays rayFill(Bitrays br) {
        br = (br + 0x7E7E7E7E7E7E7E7E) & 0x8080808080808080;
        return br - (br >> 7);
    }

    [[nodiscard]] inline Bitrays outgoingThreats(Piece piece, Bitrays closest) {
        return kOutgoingThreatsTable[piece.idx()] & closest;
    }

    [[nodiscard]] inline Bitrays incomingAttackers(Vector bits, Bitrays closest) {
        constexpr auto mask = kIncomingThreatsMask<__m512i>;
        return _mm512_test_epi8_mask(bits.raw, mask) & closest;
    }

    [[nodiscard]] inline Bitrays incomingSliders(Vector bits, Bitrays closest) {
        constexpr auto mask = kIncomingSlidersMask<__m512i>;
        return _mm512_test_epi8_mask(bits.raw, mask) & closest & 0xFEFEFEFEFEFEFEFE;
    }

} // namespace stormphrax::eval::nnue::features::threats::geometry
