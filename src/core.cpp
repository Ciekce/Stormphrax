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

#include "core.h"

fmt::format_context::iterator fmt::formatter<stormphrax::Piece>::format(
    stormphrax::Piece value,
    format_context& ctx
) const {
    using namespace stormphrax;

    static constexpr std::array kPieceChars = {
        'p', // black pawn
        'P', // white pawn
        'n', // black knight
        'N', // white knight
        'b', // black bishop
        'B', // white bishop
        'r', // black rook
        'R', // white rook
        'q', // black queen
        'Q', // white queen
        'k', // black king
        'K', // white king
        ' ', // none
    };

    if (value.idx() >= kPieceChars.size()) {
        return format_to(ctx.out(), "?");
    }

    return format_to(ctx.out(), "{}", kPieceChars[value.idx()]);
}

fmt::format_context::iterator fmt::formatter<stormphrax::PieceType>::format(
    stormphrax::PieceType value,
    format_context& ctx
) const {
    using namespace stormphrax;

    static constexpr std::array kPieceTypeChars = {
        'p', // pawn
        'n', // knight
        'b', // bishop
        'r', // rook
        'q', // queen
        'k', // king
        ' ', // none
    };

    if (value.idx() >= kPieceTypeChars.size()) {
        return format_to(ctx.out(), "?");
    }

    return format_to(ctx.out(), "{}", kPieceTypeChars[value.idx()]);
}

fmt::format_context::iterator fmt::formatter<stormphrax::Square>::format(
    stormphrax::Square value,
    format_context& ctx
) const {
    using namespace stormphrax;

    return write_padded(ctx, [=, this](auto out) {
        if (value == Squares::kNone) {
            return format_to(out, "??");
        }

        return format_to(
            out,
            "{}{}",
            nested(static_cast<char>('a' + value.file())),
            nested(static_cast<char>('1' + value.rank()))
        );
    });
}
