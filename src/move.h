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

#include "types.h"

#include <array>
#include <cstdint>

#include "core.h"
#include "opts.h"
#include "util/static_vector.h"

namespace stormphrax {
    enum class MoveType { kStandard = 0, kPromotion, kCastling, kEnPassant };

    class Move {
    public:
        constexpr Move() = default;
        constexpr ~Move() = default;

        [[nodiscard]] constexpr usize srcIdx() const {
            return m_move >> 10;
        }

        [[nodiscard]] constexpr Square src() const {
            return static_cast<Square>(srcIdx());
        }

        [[nodiscard]] constexpr i32 srcRank() const {
            return m_move >> 13;
        }

        [[nodiscard]] constexpr i32 srcFile() const {
            return (m_move >> 10) & 0x7;
        }

        [[nodiscard]] constexpr usize dstIdx() const {
            return (m_move >> 4) & 0x3F;
        }

        [[nodiscard]] constexpr Square dst() const {
            return static_cast<Square>(dstIdx());
        }

        [[nodiscard]] constexpr i32 dstRank() const {
            return (m_move >> 7) & 0x7;
        }

        [[nodiscard]] constexpr i32 dstFile() const {
            return (m_move >> 4) & 0x7;
        }

        [[nodiscard]] constexpr usize promoIdx() const {
            return (m_move >> 2) & 0x3;
        }

        [[nodiscard]] constexpr PieceType promo() const {
            return static_cast<PieceType>(promoIdx() + 1);
        }

        [[nodiscard]] constexpr MoveType type() const {
            return static_cast<MoveType>(m_move & 0x3);
        }

        [[nodiscard]] constexpr bool isNull() const {
            return /*src() == dst()*/ m_move == 0;
        }

        [[nodiscard]] constexpr u16 data() const {
            return m_move;
        }

        // returns the king's actual destination square for non-FRC
        // castling moves, otherwise just the move's destination
        // used to avoid inflating the history of the generally
        // bad moves of putting the king in a corner when castling
        [[nodiscard]] constexpr Square historyDst() const {
            if (type() == MoveType::kCastling && !g_opts.chess960) {
                return toSquare(srcRank(), srcFile() < dstFile() ? 6 : 2);
            } else {
                return dst();
            }
        }

        [[nodiscard]] explicit constexpr operator bool() const {
            return !isNull();
        }

        constexpr bool operator==(Move other) const {
            return m_move == other.m_move;
        }

        [[nodiscard]] static constexpr Move standard(Square src, Square dst) {
            return Move{static_cast<u16>(
                (static_cast<u16>(src) << 10) | (static_cast<u16>(dst) << 4) | static_cast<u16>(MoveType::kStandard)
            )};
        }

        [[nodiscard]] static constexpr Move promotion(Square src, Square dst, PieceType promo) {
            return Move{static_cast<u16>(
                (static_cast<u16>(src) << 10) | (static_cast<u16>(dst) << 4) | ((static_cast<u16>(promo) - 1) << 2)
                | static_cast<u16>(MoveType::kPromotion)
            )};
        }

        [[nodiscard]] static constexpr Move castling(Square src, Square dst) {
            return Move{static_cast<u16>(
                (static_cast<u16>(src) << 10) | (static_cast<u16>(dst) << 4) | static_cast<u16>(MoveType::kCastling)
            )};
        }

        [[nodiscard]] static constexpr Move enPassant(Square src, Square dst) {
            return Move{static_cast<u16>(
                (static_cast<u16>(src) << 10) | (static_cast<u16>(dst) << 4) | static_cast<u16>(MoveType::kEnPassant)
            )};
        }

    private:
        explicit constexpr Move(u16 move) :
                m_move{move} {}

        u16 m_move{};
    };

    constexpr Move kNullMove{};

    // assumed upper bound for number of possible moves is 218
    constexpr usize kDefaultMoveListCapacity = 256;

    using MoveList = StaticVector<Move, kDefaultMoveListCapacity>;
} // namespace stormphrax
