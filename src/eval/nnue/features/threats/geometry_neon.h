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
#include <span>
#include <tuple>

#include <arm_neon.h>

namespace stormphrax::eval::nnue::features::threats::geometry {
    struct Vector {
        uint8x16x4_t raw;

        explicit Vector(uint8x16x4_t value) :
                raw(value) {}
        explicit Vector(uint8x16_t value0, uint8x16_t value1, uint8x16_t value2, uint8x16_t value3) :
                raw({value0, value1, value2, value3}) {}

        [[nodiscard]] Vector flip() const {
            return Vector{raw.val[2], raw.val[3], raw.val[0], raw.val[1]};
        }

        [[nodiscard]] Bitrays toMask() const {
            const uint8x16_t
                mask{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
            const auto v = vpaddq_u8(
                vpaddq_u8(vandq_u8(raw.val[0], mask), vandq_u8(raw.val[1], mask)),
                vpaddq_u8(vandq_u8(raw.val[2], mask), vandq_u8(raw.val[3], mask))
            );
            return vgetq_lane_u64(vreinterpretq_u64_u8(vpaddq_u8(v, v)), 0);
        }

        [[nodiscard]] static Vector load(const void* ptr) {
            return Vector{vld1q_u8_x4(static_cast<const u8*>(ptr))};
        }

        template <typename T>
        [[nodiscard]] static Vector cast(const T& v)
            requires(sizeof(T) == sizeof(uint8x16x4_t))
        {
            return load(&v);
        }

        [[nodiscard]] uint8x16_t& operator[](int index) {
            return raw.val[index];
        }

        [[nodiscard]] uint8x16_t operator[](int index) const {
            return raw.val[index];
        }
    };

    struct Permutation {
        Vector indexes;
        Vector valid;
    };

    [[nodiscard]] inline Permutation permutationFor(Square focus) {
        const auto indexes = Vector::load(kPermutationTable[focus.idx()].data());
        const auto valid = Vector{
            vmvnq_u8(vshrq_n_s8(indexes[0], 7)),
            vmvnq_u8(vshrq_n_s8(indexes[1], 7)),
            vmvnq_u8(vshrq_n_s8(indexes[2], 7)),
            vmvnq_u8(vshrq_n_s8(indexes[3], 7)),
        };
        return Permutation{indexes, valid};
    }

    [[nodiscard]] inline std::tuple<Vector, Vector> permuteMailbox(const Permutation& permutation, Vector mailbox) {
        const auto lut = vld1q_u8(reinterpret_cast<const u8*>(kPieceToBitTable.data()));

        const Vector permuted{
            vqtbl4q_u8(mailbox.raw, permutation.indexes[0]),
            vqtbl4q_u8(mailbox.raw, permutation.indexes[1]),
            vqtbl4q_u8(mailbox.raw, permutation.indexes[2]),
            vqtbl4q_u8(mailbox.raw, permutation.indexes[3]),
        };
        const Vector bits{
            vandq_u8(vqtbl1q_u8(lut, permuted[0]), permutation.valid[0]),
            vandq_u8(vqtbl1q_u8(lut, permuted[1]), permutation.valid[1]),
            vandq_u8(vqtbl1q_u8(lut, permuted[2]), permutation.valid[2]),
            vandq_u8(vqtbl1q_u8(lut, permuted[3]), permutation.valid[3]),
        };
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
        const auto noneVec = vdupq_n_u8(static_cast<u8>(Pieces::kNone.idx()));
        const auto ignoreVec = vdupq_n_u8(static_cast<u8>(ignore.idx()));

        const auto mb = Vector::load(mailbox.data());
        const Vector maskedMailbox{
            vbslq_u8(vceqq_u8(iota[0], ignoreVec), noneVec, mb[0]),
            vbslq_u8(vceqq_u8(iota[1], ignoreVec), noneVec, mb[1]),
            vbslq_u8(vceqq_u8(iota[2], ignoreVec), noneVec, mb[2]),
            vbslq_u8(vceqq_u8(iota[3], ignoreVec), noneVec, mb[3]),
        };
        return permuteMailbox(permutation, maskedMailbox);
    }

    [[nodiscard]] inline Bitrays closestOccupied(Vector bits) {
        const Vector occupiedVec{
            vtstq_u8(bits[0], bits[0]),
            vtstq_u8(bits[1], bits[1]),
            vtstq_u8(bits[2], bits[2]),
            vtstq_u8(bits[3], bits[3]),
        };
        const auto occupied = occupiedVec.toMask();
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
        const auto mask = Vector::load(kIncomingThreatsMask.data());
        const Vector v{
            vtstq_u8(bits[0], mask[0]),
            vtstq_u8(bits[1], mask[1]),
            vtstq_u8(bits[2], mask[2]),
            vtstq_u8(bits[3], mask[3]),
        };
        return v.toMask() & closest;
    }

    [[nodiscard]] inline Bitrays incomingSliders(Vector bits, Bitrays closest) {
        const auto mask = Vector::load(kIncomingSlidersMask.data());
        const Vector v{
            vtstq_u8(bits[0], mask[0]),
            vtstq_u8(bits[1], mask[1]),
            vtstq_u8(bits[2], mask[2]),
            vtstq_u8(bits[3], mask[3]),
        };
        return v.toMask() & closest & 0xFEFEFEFEFEFEFEFE;
    }
} // namespace stormphrax::eval::nnue::features::threats::geometry
