/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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

#include "core.h"
#include "util/bits.h"

namespace stormphrax {
    namespace offsets {
        constexpr i32 Up = 8;
        constexpr i32 Down = -8;
        constexpr i32 Left = -1;
        constexpr i32 Right = 1;

        constexpr auto UpLeft = Up + Left;
        constexpr auto UpRight = Up + Right;
        constexpr auto DownLeft = Down + Left;
        constexpr auto DownRight = Down + Right;

        template<Color C>
        constexpr auto up() {
            if constexpr (C == Color::Black)
                return Down;
            else
                return Up;
        }

        template<Color C>
        constexpr auto upLeft() {
            if constexpr (C == Color::Black)
                return DownLeft;
            else
                return UpLeft;
        }

        template<Color C>
        constexpr auto upRight() {
            if constexpr (C == Color::Black)
                return DownRight;
            else
                return UpRight;
        }

        template<Color C>
        constexpr auto down() {
            if constexpr (C == Color::Black)
                return Up;
            else
                return Down;
        }

        template<Color C>
        constexpr auto downLeft() {
            if constexpr (C == Color::Black)
                return UpLeft;
            else
                return DownLeft;
        }

        template<Color C>
        constexpr auto downRight() {
            if constexpr (C == Color::Black)
                return UpRight;
            else
                return DownRight;
        }
    } // namespace offsets

    namespace shifts {
        constexpr i32 Vertical = 8;
        constexpr i32 Horizontal = 1;

        // '\'
        constexpr auto DiagonalLR = Vertical - Horizontal;
        // '/'
        constexpr auto DiagonalRL = Vertical + Horizontal;

        constexpr auto Diagonal12LR = Vertical + Vertical - Horizontal;
        constexpr auto Diagonal12RL = Vertical + Vertical + Horizontal;
        constexpr auto Diagonal21LR = Vertical - Horizontal - Horizontal;
        constexpr auto Diagonal21RL = Vertical + Horizontal + Horizontal;
    } // namespace shifts

    class BitboardSlot {
    public:
        [[nodiscard]] constexpr operator bool() const {
            return (m_board & m_mask) != 0;
        }

        constexpr auto operator=(bool rhs) -> auto & {
            if (rhs)
                m_board |= m_mask;
            else
                m_board &= ~m_mask;

            return *this;
        }

    private:
        constexpr BitboardSlot(u64 &board, i32 n) :
                m_board{board}, m_mask{u64{1} << n} {}

        u64 &m_board;
        u64 m_mask;

        friend class Bitboard;
    };

    class Bitboard {
    public:
        static constexpr auto Rank1 = U64(0x00000000000000FF);
        static constexpr auto Rank2 = U64(0x000000000000FF00);
        static constexpr auto Rank3 = U64(0x0000000000FF0000);
        static constexpr auto Rank4 = U64(0x00000000FF000000);
        static constexpr auto Rank5 = U64(0x000000FF00000000);
        static constexpr auto Rank6 = U64(0x0000FF0000000000);
        static constexpr auto Rank7 = U64(0x00FF000000000000);
        static constexpr auto Rank8 = U64(0xFF00000000000000);

        static constexpr auto FileA = U64(0x0101010101010101);
        static constexpr auto FileB = U64(0x0202020202020202);
        static constexpr auto FileC = U64(0x0404040404040404);
        static constexpr auto FileD = U64(0x0808080808080808);
        static constexpr auto FileE = U64(0x1010101010101010);
        static constexpr auto FileF = U64(0x2020202020202020);
        static constexpr auto FileG = U64(0x4040404040404040);
        static constexpr auto FileH = U64(0x8080808080808080);

        static constexpr auto DarkSquares = U64(0xAA55AA55AA55AA55);
        static constexpr auto LightSquares = U64(0x55AA55AA55AA55AA);

        static constexpr auto CenterSquares = U64(0x0000001818000000);

        static constexpr auto All = U64(0xFFFFFFFFFFFFFFFF);

        constexpr Bitboard(u64 board = 0) :
                m_board{board} {}

        [[nodiscard]] constexpr operator u64() const {
            return m_board;
        }

        [[nodiscard]] constexpr auto operator&(Bitboard rhs) const -> Bitboard {
            return m_board & rhs;
        }
        [[nodiscard]] constexpr auto operator|(Bitboard rhs) const -> Bitboard {
            return m_board | rhs;
        }
        [[nodiscard]] constexpr auto operator^(Bitboard rhs) const -> Bitboard {
            return m_board ^ rhs;
        }

        constexpr auto operator&=(Bitboard rhs) -> auto & {
            m_board &= rhs;
            return *this;
        }
        constexpr auto operator|=(Bitboard rhs) -> auto & {
            m_board |= rhs;
            return *this;
        }
        constexpr auto operator^=(Bitboard rhs) -> auto & {
            m_board ^= rhs;
            return *this;
        }

        [[nodiscard]] constexpr auto operator&(u64 rhs) const -> Bitboard {
            return m_board & rhs;
        }
        [[nodiscard]] constexpr auto operator|(u64 rhs) const -> Bitboard {
            return m_board | rhs;
        }
        [[nodiscard]] constexpr auto operator^(u64 rhs) const -> Bitboard {
            return m_board ^ rhs;
        }

        constexpr auto operator&=(u64 rhs) -> auto & {
            m_board &= rhs;
            return *this;
        }
        constexpr auto operator|=(u64 rhs) -> auto & {
            m_board |= rhs;
            return *this;
        }
        constexpr auto operator^=(u64 rhs) -> auto & {
            m_board ^= rhs;
            return *this;
        }

        [[nodiscard]] constexpr auto operator&(i32 rhs) const -> Bitboard {
            return m_board & static_cast<u64>(rhs);
        }
        [[nodiscard]] constexpr auto operator|(i32 rhs) const -> Bitboard {
            return m_board | static_cast<u64>(rhs);
        }
        [[nodiscard]] constexpr auto operator^(i32 rhs) const -> Bitboard {
            return m_board ^ static_cast<u64>(rhs);
        }

        constexpr auto operator&=(i32 rhs) -> auto & {
            m_board &= static_cast<u64>(rhs);
            return *this;
        }
        constexpr auto operator|=(i32 rhs) -> auto & {
            m_board |= static_cast<u64>(rhs);
            return *this;
        }
        constexpr auto operator^=(i32 rhs) -> auto & {
            m_board ^= static_cast<u64>(rhs);
            return *this;
        }

        [[nodiscard]] constexpr auto operator~() const -> Bitboard {
            return ~m_board;
        }

        [[nodiscard]] constexpr auto operator<<(i32 rhs) const -> Bitboard {
            return m_board << rhs;
        }
        [[nodiscard]] constexpr auto operator>>(i32 rhs) const -> Bitboard {
            return m_board >> rhs;
        }

        constexpr auto operator<<=(i32 rhs) -> auto & {
            m_board <<= rhs;
            return *this;
        }
        constexpr auto operator>>=(i32 rhs) -> auto & {
            m_board >>= rhs;
            return *this;
        }

        [[nodiscard]] constexpr auto operator[](Square s) const -> bool {
            return m_board & (U64(1) << static_cast<i32>(s));
        }
        [[nodiscard]] constexpr auto operator[](Square s) {
            return BitboardSlot{m_board, static_cast<i32>(s)};
        }

        [[nodiscard]] constexpr auto popcount() const {
            return util::popcnt(m_board);
        }

        [[nodiscard]] constexpr auto empty() const {
            return m_board == 0;
        }
        [[nodiscard]] constexpr auto multiple() const {
            return util::resetLsb(m_board) != 0;
        }
        [[nodiscard]] constexpr auto one() const {
            return !empty() && !multiple();
        }

        [[nodiscard]] constexpr auto lowestSquare() const {
            return static_cast<Square>(util::ctz(m_board));
        }

        [[nodiscard]] constexpr auto lowestBit() const -> Bitboard {
            return util::lsb(m_board);
        }

        [[nodiscard]] constexpr auto popLowestSquare() {
            const auto square = lowestSquare();
            m_board = util::resetLsb(m_board);
            return square;
        }

        [[nodiscard]] constexpr auto popLowestBit() -> Bitboard {
            const auto bit = lowestBit();
            m_board = util::resetLsb(m_board);
            return bit;
        }

        constexpr auto clear() {
            m_board = 0;
        }

        [[nodiscard]] constexpr auto shiftUp() const -> Bitboard {
            return m_board << shifts::Vertical;
        }

        [[nodiscard]] constexpr auto shiftDown() const -> Bitboard {
            return m_board >> shifts::Vertical;
        }

        [[nodiscard]] constexpr auto shiftLeft() const -> Bitboard {
            return (m_board >> shifts::Horizontal) & ~FileH;
        }

        [[nodiscard]] constexpr auto shiftLeftUnchecked() const -> Bitboard {
            return m_board >> shifts::Horizontal;
        }

        [[nodiscard]] constexpr auto shiftRight() const -> Bitboard {
            return (m_board << shifts::Horizontal) & ~FileA;
        }

        [[nodiscard]] constexpr auto shiftRightUnchecked() const -> Bitboard {
            return m_board << shifts::Horizontal;
        }

        [[nodiscard]] constexpr auto shiftUpLeft() const -> Bitboard {
            return (m_board << shifts::DiagonalLR) & ~FileH;
        }

        [[nodiscard]] constexpr auto shiftUpRight() const -> Bitboard {
            return (m_board << shifts::DiagonalRL) & ~FileA;
        }

        [[nodiscard]] constexpr auto shiftDownLeft() const -> Bitboard {
            return (m_board >> shifts::DiagonalRL) & ~FileH;
        }

        [[nodiscard]] constexpr auto shiftDownRight() const -> Bitboard {
            return (m_board >> shifts::DiagonalLR) & ~FileA;
        }

        [[nodiscard]] constexpr auto shiftUpUpLeft() const -> Bitboard {
            return (m_board << shifts::Diagonal12LR) & ~FileH;
        }

        [[nodiscard]] constexpr auto shiftUpUpRight() const -> Bitboard {
            return (m_board << shifts::Diagonal12RL) & ~FileA;
        }

        [[nodiscard]] constexpr auto shiftUpLeftLeft() const -> Bitboard {
            return (m_board << shifts::Diagonal21LR) & ~(FileG | FileH);
        }

        [[nodiscard]] constexpr auto shiftUpRightRight() const -> Bitboard {
            return (m_board << shifts::Diagonal21RL) & ~(FileA | FileB);
        }

        [[nodiscard]] constexpr auto shiftDownLeftLeft() const -> Bitboard {
            return (m_board >> shifts::Diagonal21RL) & ~(FileG | FileH);
        }

        [[nodiscard]] constexpr auto shiftDownRightRight() const -> Bitboard {
            return (m_board >> shifts::Diagonal21LR) & ~(FileA | FileB);
        }

        [[nodiscard]] constexpr auto shiftDownDownLeft() const -> Bitboard {
            return (m_board >> shifts::Diagonal12RL) & ~FileH;
        }

        [[nodiscard]] constexpr auto shiftDownDownRight() const -> Bitboard {
            return (m_board >> shifts::Diagonal12LR) & ~FileA;
        }

        template<Color C>
        [[nodiscard]] constexpr auto shiftUpRelative() const {
            if constexpr (C == Color::Black) {
                return shiftDown();
            } else {
                return shiftUp();
            }
        }

        template<Color C>
        [[nodiscard]] constexpr auto shiftUpLeftRelative() const {
            if constexpr (C == Color::Black) {
                return shiftDownLeft();
            } else {
                return shiftUpLeft();
            }
        }

        template<Color C>
        [[nodiscard]] constexpr auto shiftUpRightRelative() const {
            if constexpr (C == Color::Black) {
                return shiftDownRight();
            } else {
                return shiftUpRight();
            }
        }

        template<Color C>
        [[nodiscard]] constexpr auto shiftDownRelative() const {
            if constexpr (C == Color::Black) {
                return shiftUp();
            } else {
                return shiftDown();
            }
        }

        template<Color C>
        [[nodiscard]] constexpr auto shiftDownLeftRelative() const {
            if constexpr (C == Color::Black) {
                return shiftUpLeft();
            } else {
                return shiftDownLeft();
            }
        }

        template<Color C>
        [[nodiscard]] constexpr auto shiftDownRightRelative() const {
            if constexpr (C == Color::Black) {
                return shiftUpRight();
            } else {
                return shiftDownRight();
            }
        }

        [[nodiscard]] constexpr auto fillUp() const -> Bitboard {
            auto b = m_board;
            b |= b << 8;
            b |= b << 16;
            b |= b << 32;
            return b;
        }

        [[nodiscard]] constexpr auto fillDown() const -> Bitboard {
            auto b = m_board;
            b |= b >> 8;
            b |= b >> 16;
            b |= b >> 32;
            return b;
        }

        template<Color C>
        [[nodiscard]] constexpr auto fillUpRelative() const {
            if constexpr (C == Color::Black) {
                return fillDown();
            } else {
                return fillUp();
            }
        }

        template<Color C>
        [[nodiscard]] constexpr auto fillDownRelative() const {
            if constexpr (C == Color::Black) {
                return fillUp();
            } else {
                return fillDown();
            }
        }

        [[nodiscard]] constexpr auto fillFile() const {
            return fillUp() | fillDown();
        }

        [[nodiscard]] constexpr auto operator==(const Bitboard &other) const {
            return m_board == other.m_board;
        }

        [[nodiscard]] constexpr auto operator==(u64 other) const {
            return m_board == other;
        }

        [[nodiscard]] constexpr auto operator==(i32 other) const {
            return m_board == other;
        }

        [[nodiscard]] constexpr static auto fromSquare(Square square) -> Bitboard {
            return squareBit(square);
        }

        [[nodiscard]] constexpr static auto fromSquareChecked(Square square) -> Bitboard {
            return squareBitChecked(square);
        }

    private:
        u64 m_board;
    };

    namespace boards {
        constexpr Bitboard Rank1{Bitboard::Rank1};
        constexpr Bitboard Rank2{Bitboard::Rank2};
        constexpr Bitboard Rank3{Bitboard::Rank3};
        constexpr Bitboard Rank4{Bitboard::Rank4};
        constexpr Bitboard Rank5{Bitboard::Rank5};
        constexpr Bitboard Rank6{Bitboard::Rank6};
        constexpr Bitboard Rank7{Bitboard::Rank7};
        constexpr Bitboard Rank8{Bitboard::Rank8};

        constexpr Bitboard FileA{Bitboard::FileA};
        constexpr Bitboard FileB{Bitboard::FileB};
        constexpr Bitboard FileC{Bitboard::FileC};
        constexpr Bitboard FileD{Bitboard::FileD};
        constexpr Bitboard FileE{Bitboard::FileE};
        constexpr Bitboard FileF{Bitboard::FileF};
        constexpr Bitboard FileG{Bitboard::FileG};
        constexpr Bitboard FileH{Bitboard::FileH};

        constexpr auto Ranks = std::array{Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8};
        constexpr auto Files = std::array{FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH};

        constexpr Bitboard DarkSquares{Bitboard::DarkSquares};
        constexpr Bitboard LightSquares{Bitboard::LightSquares};

        constexpr Bitboard CenterSquares{Bitboard::CenterSquares};

        constexpr Bitboard All{Bitboard::All};

        template<Color C>
        [[nodiscard]] constexpr auto promotionRank() {
            if constexpr (C == Color::Black) {
                return Rank1;
            } else {
                return Rank8;
            }
        }

        [[nodiscard]] constexpr auto promotionRank(Color c) {
            return c == Color::Black ? Rank1 : Rank8;
        }

        template<Color C>
        [[nodiscard]] constexpr auto rank(i32 idx) {
            return Ranks[relativeRank<C>(idx)];
        }
    } // namespace boards
} // namespace stormphrax
