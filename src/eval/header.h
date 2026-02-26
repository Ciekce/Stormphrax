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

#include <array>

namespace stormphrax::eval {
    SP_ENUM_FLAGS(u16, NetworkFlags){
        kNone = 0x0000,
        kZstdCompressed = 0x0001,
        kHorizontallyMirrored = 0x0002,
        kMergedKings = 0x0004,
        kPairwiseMul = 0x0008,
    };

    constexpr u16 kExpectedHeaderVersion = 1;

    struct __attribute__((packed)) NetworkHeader {
        std::array<char, 4> magic{};
        u16 version{};
        NetworkFlags flags{};
        [[maybe_unused]] u8 padding{};
        u8 arch{};
        u8 activation{};
        u16 hiddenSize{};
        u8 inputBuckets{};
        u8 outputBuckets{};
        u8 nameLen{};
        std::array<char, 48> name{};
    };

    static_assert(sizeof(NetworkHeader) == 64);
} // namespace stormphrax::eval
