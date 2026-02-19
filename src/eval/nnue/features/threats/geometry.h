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

#include "../../../../arch.h"
#include "../../../../core.h"
#include "../../../../types.h"

namespace stormphrax::eval::nnue::features::threats::geometry {

    using Bit = u8;

    struct Bits {
        inline static constexpr Bit kBlackPawn = 0x01;
        inline static constexpr Bit kWhitePawn = 0x02;
        inline static constexpr Bit kKnight = 0x04;
        inline static constexpr Bit kBishop = 0x08;
        inline static constexpr Bit kRook = 0x10;
        inline static constexpr Bit kQueen = 0x20;
        inline static constexpr Bit kKing = 0x40;
    };

    using Bitrays = u64;

    inline constexpr std::array<std::array<u8, Squares::kCount>, Squares::kCount> kPermutationTable = []() {
        constexpr std::array<u8, Squares::kCount> offsets{{
            0x1F, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, // N
            0x21, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, // NE
            0x12, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // E
            0xF2, 0xF1, 0xE2, 0xD3, 0xC4, 0xB5, 0xA6, 0x97, // SE
            0xE1, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90, // S
            0xDF, 0xEF, 0xDE, 0xCD, 0xBC, 0xAB, 0x9A, 0x89, // SW
            0xEE, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, // W
            0x0E, 0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69, // NW
        }};

        std::array<std::array<u8, Squares::kCount>, Squares::kCount> permutations{};
        for (u8 focus = 0; focus < Squares::kCount; focus++) {
            for (u8 i = 0; i < Squares::kCount; i++) {
                const u8 wideFocus = focus + (focus & 0x38);
                const u8 wideResult = offsets[i] + wideFocus;
                const u8 result = ((wideResult & 0x70) >> 1) | (wideResult & 0x07);
                const bool valid = (wideResult & 0x88) == 0;
                permutations[focus][i] = valid ? result : 0x80;
            }
        }
        return permutations;
    }();

    inline constexpr __m128i kPieceToBitTable = []() {
        std::array<u8, 16> lut{};

        lut[Pieces::kBlackPawn.idx()] = Bits::kBlackPawn;
        lut[Pieces::kWhitePawn.idx()] = Bits::kWhitePawn;
        lut[Pieces::kBlackKnight.idx()] = Bits::kKnight;
        lut[Pieces::kWhiteKnight.idx()] = Bits::kKnight;
        lut[Pieces::kBlackBishop.idx()] = Bits::kBishop;
        lut[Pieces::kWhiteBishop.idx()] = Bits::kBishop;
        lut[Pieces::kBlackRook.idx()] = Bits::kRook;
        lut[Pieces::kWhiteRook.idx()] = Bits::kRook;
        lut[Pieces::kBlackQueen.idx()] = Bits::kQueen;
        lut[Pieces::kWhiteQueen.idx()] = Bits::kQueen;
        lut[Pieces::kBlackKing.idx()] = Bits::kKing;
        lut[Pieces::kWhiteKing.idx()] = Bits::kKing;
        lut[Pieces::kNone.idx()] = 0;

        return std::bit_cast<__m128i>(lut);
    }();

    inline constexpr std::array<Bitrays, Pieces::kCount> kOutgoingThreatsTable = []() {
        std::array<Bitrays, Pieces::kCount> lut{};
        lut[Pieces::kWhitePawn.idx()] = 0x02'00'00'00'00'00'02'00;
        lut[Pieces::kBlackPawn.idx()] = 0x00'00'02'00'02'00'00'00;
        lut[Pieces::kWhiteKnight.idx()] = 0x01'01'01'01'01'01'01'01;
        lut[Pieces::kBlackKnight.idx()] = 0x01'01'01'01'01'01'01'01;
        lut[Pieces::kWhiteBishop.idx()] = 0xFE'00'FE'00'FE'00'FE'00;
        lut[Pieces::kBlackBishop.idx()] = 0xFE'00'FE'00'FE'00'FE'00;
        lut[Pieces::kWhiteRook.idx()] = 0x00'FE'00'FE'00'FE'00'FE;
        lut[Pieces::kBlackRook.idx()] = 0x00'FE'00'FE'00'FE'00'FE;
        lut[Pieces::kWhiteQueen.idx()] = 0xFE'FE'FE'FE'FE'FE'FE'FE;
        lut[Pieces::kBlackQueen.idx()] = 0xFE'FE'FE'FE'FE'FE'FE'FE;
        lut[Pieces::kWhiteKing.idx()] = 0x02'02'02'02'02'02'02'02;
        lut[Pieces::kBlackKing.idx()] = 0x02'02'02'02'02'02'02'02;
        return lut;
    }();

    inline constexpr std::array<Bit, 64> kIncomingThreatsMask = []() {
        constexpr Bit horse = Bits::kKnight;
        constexpr Bit orth = Bits::kQueen | Bits::kRook;
        constexpr Bit diag = Bits::kQueen | Bits::kBishop;
        constexpr Bit orthoNear = Bits::kKing | orth;
        constexpr Bit wPawnNear = Bits::kWhitePawn | Bits::kKing | diag;
        constexpr Bit bPawnNear = Bits::kBlackPawn | Bits::kKing | diag;

        return std::array<Bit, 64>{{
            horse, orthoNear, orth, orth, orth, orth, orth, orth, // N
            horse, bPawnNear, diag, diag, diag, diag, diag, diag, // NE
            horse, orthoNear, orth, orth, orth, orth, orth, orth, // E
            horse, wPawnNear, diag, diag, diag, diag, diag, diag, // SE
            horse, orthoNear, orth, orth, orth, orth, orth, orth, // S
            horse, wPawnNear, diag, diag, diag, diag, diag, diag, // SW
            horse, orthoNear, orth, orth, orth, orth, orth, orth, // W
            horse, bPawnNear, diag, diag, diag, diag, diag, diag, // NW
        }};
    }();

    inline constexpr std::array<Bit, 64> kIncomingSlidersMask = []() {
        constexpr Bit orth = Bits::kQueen | Bits::kRook;
        constexpr Bit diag = Bits::kQueen | Bits::kBishop;

        return std::array<Bit, 64>{{
            0x80, orth, orth, orth, orth, orth, orth, orth, // N
            0x80, diag, diag, diag, diag, diag, diag, diag, // NE
            0x80, orth, orth, orth, orth, orth, orth, orth, // E
            0x80, diag, diag, diag, diag, diag, diag, diag, // SE
            0x80, orth, orth, orth, orth, orth, orth, orth, // S
            0x80, diag, diag, diag, diag, diag, diag, diag, // SW
            0x80, orth, orth, orth, orth, orth, orth, orth, // W
            0x80, diag, diag, diag, diag, diag, diag, diag, // NW
        }};
    }();

} // namespace stormphrax::eval::nnue::features::threats::geometry

#if SP_HAS_VBMI
    #include "geometry_vbmi.h"
#else
    #include "geometry_default.h"
#endif

