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

#include "../../../../types.h"

#include <array>
#include <bit>
#include <tuple>

#include <immintrin.h>

namespace stormphrax::eval::nnue::features::threats::geometry {
    struct Vector {
        std::array<__m256i, 2> raw;

        [[nodiscard]] Vector flip() const {
            return {{raw[1], raw[0]}};
        }

        [[nodiscard]] Bitrays toMask() const {
            return static_cast<u32>(_mm256_movemask_epi8(raw[0]))
                 | (static_cast<Bitrays>(_mm256_movemask_epi8(raw[1])) << 32);
        }

        [[nodiscard]] static Vector load(const void* ptr) {
            return Vector{
                .raw = {
                    _mm256_loadu_si256(static_cast<const __m256i*>(ptr) + 0),
                    _mm256_loadu_si256(static_cast<const __m256i*>(ptr) + 1),
                }
            };
        }

        template <typename T>
        [[nodiscard]] static Vector cast(const T& v)
            requires(sizeof(T) == sizeof(__m256i) * 2)
        {
            return load(&v);
        }
    };

    struct Permutation {
        Vector indexes;
        Vector invalid;
    };

    [[nodiscard]] inline Permutation permutationFor(Square focus) {
        const auto indexes = Vector::cast(kPermutationTable[focus.idx()]);
        const Vector valid{{
            _mm256_cmpeq_epi8(indexes.raw[0], _mm256_set1_epi8(0x80)),
            _mm256_cmpeq_epi8(indexes.raw[1], _mm256_set1_epi8(0x80)),
        }};
        return Permutation{indexes, valid};
    }

    [[nodiscard]] inline std::tuple<Vector, Vector> permuteMailbox(
        const Permutation& permutation,
        Vector maskedMailbox
    ) {
        const auto lut =
            _mm256_broadcastsi128_si256(_mm_loadu_si128(reinterpret_cast<const __m128i*>(kPieceToBitTable.data())));

        const auto halfSwizzler = [](__m256i bytes0, __m256i bytes1, __m256i idxs) {
            const auto mask0 = _mm256_slli_epi64(idxs, 2);
            const auto mask1 = _mm256_slli_epi64(idxs, 3);

            const auto lolo0 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x00), idxs);
            const auto hihi0 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x11), idxs);
            const auto x = _mm256_blendv_epi8(lolo0, hihi0, mask1);

            const auto lolo1 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x00), idxs);
            const auto hihi1 = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x11), idxs);
            const auto y = _mm256_blendv_epi8(lolo1, hihi1, mask1);

            return _mm256_blendv_epi8(x, y, mask0);
        };

        const Vector permuted{{
            halfSwizzler(maskedMailbox.raw[0], maskedMailbox.raw[1], permutation.indexes.raw[0]),
            halfSwizzler(maskedMailbox.raw[0], maskedMailbox.raw[1], permutation.indexes.raw[1]),
        }};
        const Vector bits{{
            _mm256_andnot_si256(permutation.invalid.raw[0], _mm256_shuffle_epi8(lut, permuted.raw[0])),
            _mm256_andnot_si256(permutation.invalid.raw[1], _mm256_shuffle_epi8(lut, permuted.raw[1])),
        }};

        return {permuted, bits};
    }

    [[nodiscard]] inline std::tuple<Vector, Vector> permuteMailbox(
        const Permutation& permutation,
        const std::span<const Piece, Squares::kCount> mailbox
    ) {
        return permuteMailbox(permutation, Vector::load(mailbox.data()));
    }

    [[nodiscard]] inline std::tuple<Vector, Vector> permuteMailbox(
        const Permutation& permutation,
        const std::span<const Piece, Squares::kCount> mailbox,
        Square ignore
    ) {
        const auto iota = Vector::cast(
            std::array<u8, 64>{{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
                                44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63}}
        );
        const auto ignoreVec = _mm256_set1_epi8(static_cast<i8>(ignore.idx()));
        const auto noneVec = _mm256_set1_epi8(static_cast<i8>(Pieces::kNone.idx()));
        const auto mb = Vector::load(mailbox.data());
        const Vector maskedMailbox{
            _mm256_blendv_epi8(mb.raw[0], noneVec, _mm256_cmpeq_epi8(iota.raw[0], ignoreVec)),
            _mm256_blendv_epi8(mb.raw[1], noneVec, _mm256_cmpeq_epi8(iota.raw[1], ignoreVec)),
        };
        return permuteMailbox(permutation, maskedMailbox);
    }

    [[nodiscard]] inline Bitrays closestOccupied(Vector bits) {
        const Vector unoccupied{
            _mm256_cmpeq_epi8(bits.raw[0], _mm256_setzero_si256()),
            _mm256_cmpeq_epi8(bits.raw[1], _mm256_setzero_si256()),
        };
        const Bitrays occupied = ~unoccupied.toMask();
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
        const auto mask = Vector::cast(kIncomingThreatsMask);
        const Vector v{{
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()),
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()),
        }};
        return ~v.toMask() & closest;
    }

    [[nodiscard]] inline Bitrays incomingSliders(Vector bits, Bitrays closest) {
        const auto mask = Vector::cast(kIncomingSlidersMask);
        const Vector v{{
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()),
            _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()),
        }};
        return ~v.toMask() & closest & 0xFEFEFEFEFEFEFEFE;
    }
} // namespace stormphrax::eval::nnue::features::threats::geometry
