/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

#include "../../types.h"

#include <array>

#include "../../bitboard.h"
#include "../../core.h"
#include "../util.h"

namespace stormphrax::attacks::black_magic {
    //TODO better magics exist
    constexpr std::array kRookShifts = {
        // clang-format off
        52, 53, 53, 53, 53, 53, 53, 52,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        52, 53, 53, 53, 53, 53, 53, 52,
        // clang-format on
    };

    constexpr std::array kBishopShifts = {
        // clang-format off
        59, 60, 59, 59, 59, 59, 60, 58,
        60, 60, 59, 59, 59, 59, 60, 60,
        59, 59, 57, 57, 57, 57, 59, 59,
        59, 59, 57, 55, 55, 57, 59, 59,
        59, 59, 57, 55, 55, 57, 59, 59,
        59, 59, 57, 57, 57, 57, 59, 60,
        60, 60, 59, 59, 59, 59, 60, 60,
        59, 60, 59, 59, 59, 59, 60, 58,
        // clang-format on
    };

    constexpr std::array kRookMagics = {
        U64(0x2080002040068490), U64(0x06C0021001200C40), U64(0x288009300280A000), U64(0x0100089521003000),
        U64(0x6100040801003082), U64(0x65FFEBC5FFEEE7F0), U64(0x0400080C10219112), U64(0x0200014434060003),
        U64(0x96CD8008C00379D9), U64(0x2A06002101FF81CF), U64(0x7BCA0020802E0641), U64(0xDAE2FFEFFD0020BA),
        U64(0x62E20005E0D200AA), U64(0x2302000830DA0044), U64(0xE81C002CE40A3028), U64(0xC829FFFAFD8BBC06),
        U64(0x12C57E800740089D), U64(0xA574FDFFE13A81FD), U64(0xF331B1FFE0BF79FE), U64(0x0000A1003001010A),
        U64(0x7CD4E2000600264F), U64(0x0299010004000228), U64(0xA36CEBFFAE0FA825), U64(0x9A87E9FFF4408405),
        U64(0x0BAEC0007FF8EB82), U64(0xF81909BDFFE18205), U64(0x0391AF45001FFF01), U64(0xD000900100290021),
        U64(0x2058480080040080), U64(0x6DCDFFA2002C38D0), U64(0xC709C80C00951002), U64(0xB70EE5420008FF84),
        U64(0x6E254003897FFCE6), U64(0xD91D21FE7E003901), U64(0xA0D1EFFF857FE001), U64(0x7C45FFC022001893),
        U64(0x8180818800800400), U64(0x2146001CB20018B0), U64(0x843C20E7DBFF8FEE), U64(0x09283C127A00083F),
        U64(0x01465F8CC0078000), U64(0xA30A50075FFD3FFF), U64(0x39593D8231FE0020), U64(0x8129FE58405E000F),
        U64(0x1140850008010011), U64(0x2302000830DA0044), U64(0xD706971819F400B0), U64(0xA0B2A3BC86E20004),
        U64(0x10FFF67AD3B88200), U64(0x10FFF67AD3B88200), U64(0x5076D15DBDF97E00), U64(0xD861C0D1FFC8DE00),
        U64(0x5CA002003B305E00), U64(0x84FFFFCF19605740), U64(0xD26F0FA80A28AC00), U64(0x342F7E87013BFA00),
        U64(0x63BB9E8FBF01FE7A), U64(0x260ADF40007B9101), U64(0x2013CEFF6000BEF7), U64(0x13AD6200060EBFE6),
        U64(0x2D4DFFFF28F4D9FA), U64(0x766200004B3A92F6), U64(0xB6AE6FF7FE8A070C), U64(0xD065F4839BFC4B02),
    };

    constexpr std::array kBishopMagics = {
        U64(0x69906270549A3405), U64(0xE846197A0E88067F), U64(0x54D7C7FB06DE5827), U64(0xF4380209C8E966FE),
        U64(0xDF33F39ECD91FCF6), U64(0xC580F3DFFCC85DB4), U64(0xC6A89809B600286C), U64(0xC1DE00D4289BFFC0),
        U64(0x7BDA249AC632C811), U64(0x83534631B40CA406), U64(0x6EA35817F035775C), U64(0x6DB23BEF4DF5645E),
        U64(0x5555D3FB9F934CD3), U64(0xE6766DFD0FC609F8), U64(0xFC2EB0C6C58C8021), U64(0x6786D25EACCFDF72),
        U64(0x86E8324A02CA8AEF), U64(0xF91A13391D2D97F1), U64(0x131810CFFD99BE90), U64(0x8537F35C05EFA08B),
        U64(0x5D598243FF5FD71A), U64(0x1D09FFBF00FAD72B), U64(0xD16A319977FC05FD), U64(0x8D6601E599347F90),
        U64(0x4404409F5EC1F3DB), U64(0x25A7EC287E0BB817), U64(0x22F9F7FF5AF54401), U64(0x00200302080070E0),
        U64(0x3D1900D006FFC014), U64(0x3958E700A5FEBEFB), U64(0xD48AA0E6BBFC0214), U64(0x56BBF68FC6CD5C13),
        U64(0xD4CFE69F216FF3C9), U64(0xE46CEF960C704413), U64(0x7985CEB00428057B), U64(0x4900220082080080),
        U64(0x028422C010040100), U64(0x119377F9FFF6BEEB), U64(0x2787B8DA98AC0221), U64(0xCF340AB7795DFC80),
        U64(0x5F4D27A008D84FE9), U64(0x4339FF0FE25ED893), U64(0x88F477A178045010), U64(0x7B293EDFD1015806),
        U64(0x1F61DFF2047F5BFF), U64(0xE2E1B97D1A009100), U64(0x9C9F7BCC878F1A08), U64(0xABFFCA859DA3CDFE),
        U64(0x1CD806CBB423E49B), U64(0x5EE7FB86BD527D9B), U64(0xBB0A8BC1EAB02192), U64(0xB75E295A3FCE452C),
        U64(0x911D2E51E6060430), U64(0x133E017175D1FB87), U64(0xD7C00065234350D1), U64(0x220029F586970AD8),
        U64(0xA6F001938E193FDB), U64(0xDF725BF4FA4505B6), U64(0xE5DE50FA3FDC8C72), U64(0x3CE77ED6760FC3D0),
        U64(0x4CAD71659E41C408), U64(0xE6766DFD0FC609F8), U64(0x45D7FEA873649EA8), U64(0xA8806CA2E576C9E4),
    };

    struct SquareData {
        Bitboard mask;
        u32 offset;
    };

    struct Data {
        std::array<SquareData, Squares::kCount> data;
        u32 tableSize;
    };

    constexpr auto kRookData = [] {
        Data dst{};

        for (u32 i = 0; i < Squares::kCount; ++i) {
            const auto sq = Square::fromRaw(i);

            dst.data[i].mask = boards::kAll;

            for (const auto dir : {offsets::kUp, offsets::kDown, offsets::kLeft, offsets::kRight}) {
                const auto attacks = internal::generateSlidingAttacks(sq, dir, 0);
                dst.data[i].mask &= ~(attacks & ~internal::edges(dir));
            }

            dst.data[i].offset = dst.tableSize;
            dst.tableSize += 1 << (64 - kRookShifts[i]);
        }

        return dst;
    }();

    constexpr auto kBishopData = [] {
        Data dst{};

        for (u32 i = 0; i < 64; ++i) {
            const auto sq = Square::fromRaw(i);

            dst.data[i].mask = boards::kAll;

            for (const auto dir : {offsets::kUpLeft, offsets::kUpRight, offsets::kDownLeft, offsets::kDownRight}) {
                const auto attacks = internal::generateSlidingAttacks(sq, dir, 0);
                dst.data[i].mask &= ~(attacks & ~internal::edges(dir));
            }

            dst.data[i].offset = dst.tableSize;
            dst.tableSize += 1 << (64 - kBishopShifts[i]);
        }

        return dst;
    }();
} // namespace stormphrax::attacks::black_magic
