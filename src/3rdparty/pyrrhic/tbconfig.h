/*
 * (c) 2015 basil, all rights reserved,
 * Modifications Copyright (c) 2016-2019 by Jon Dart
 * Modifications Copyright (c) 2020-2024 by Andrew Grant
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "../../core.h"
#include "../../util/bits.h"
#include "../../attacks/attacks.h"
#include "../../search.h"

namespace stormphrax::util::detail {
    constexpr i32 popLsb(u64* x) {
        const auto lsb = util::ctz(*x);
        *x = util::resetLsb(*x);
        return lsb;
    }
}

#define PYRRHIC_POPCOUNT(x)              (std::popcount(x))
#define PYRRHIC_LSB(x)                   (stormphrax::util::ctz(x))
#define PYRRHIC_POPLSB(x)                (stormphrax::util::detail::popLsb(x))

#define PYRRHIC_PAWN_ATTACKS(sq, c)      (stormphrax::attacks::getPawnAttacks(static_cast<stormphrax::Square>(sq), stormphrax::Color::fromRaw(c)))
#define PYRRHIC_KNIGHT_ATTACKS(sq)       (stormphrax::attacks::getKnightAttacks(static_cast<stormphrax::Square>(sq)))
#define PYRRHIC_BISHOP_ATTACKS(sq, occ)  (stormphrax::attacks::getBishopAttacks(static_cast<stormphrax::Square>(sq), occ))
#define PYRRHIC_ROOK_ATTACKS(sq, occ)    (stormphrax::attacks::getRookAttacks(static_cast<stormphrax::Square>(sq), occ))
#define PYRRHIC_QUEEN_ATTACKS(sq, occ)   (stormphrax::attacks::getQueenAttacks(static_cast<stormphrax::Square>(sq), occ))
#define PYRRHIC_KING_ATTACKS(sq)         (stormphrax::attacks::getKingAttacks(static_cast<stormphrax::Square>(sq)))
