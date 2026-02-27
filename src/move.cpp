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

#include "move.h"

#include "opts.h"
#include "position/position.h"

fmt::format_context::iterator fmt::formatter<stormphrax::Move>::format(
    stormphrax::Move value,
    format_context& ctx
) const {
    using namespace stormphrax;

    if (value.isNull()) {
        return format_to(ctx.out(), "????");
    }

    format_to(ctx.out(), "{}", value.fromSq());

    const auto type = value.type();

    if (type != MoveType::kCastling || g_opts.chess960) {
        format_to(ctx.out(), "{}", value.toSq());
        if (type == MoveType::kPromotion) {
            format_to(ctx.out(), "{}", value.promo());
        }
    } else {
        const auto dst =
            value.fromSqFile() < value.toSqFile() ? value.fromSq().withFile(kFileG) : value.fromSq().withFile(kFileC);
        format_to(ctx.out(), "{}", dst);
    }

    return ctx.out();
}
