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

#include "core.h"
#include "util/bits.h"

namespace stormphrax {
    namespace offsets {
        constexpr i32 kUp = 8;
        constexpr i32 kDown = -8;
        constexpr i32 kLeft = -1;
        constexpr i32 kRight = 1;

        constexpr auto kUpLeft = kUp + kLeft;
        constexpr auto kUpRight = kUp + kRight;
        constexpr auto kDownLeft = kDown + kLeft;
        constexpr auto kDownRight = kDown + kRight;

        template <Color kC>
        constexpr i32 up() {
            if constexpr (kC == Color::kBlack) {
                return kDown;
            } else {
                return kUp;
            }
        }

        template <Color kC>
        constexpr i32 upLeft() {
            if constexpr (kC == Color::kBlack) {
                return kDownLeft;
            } else {
                return kUpLeft;
            }
        }

        template <Color kC>
        constexpr i32 upRight() {
            if constexpr (kC == Color::kBlack) {
                return kDownRight;
            } else {
                return kUpRight;
            }
        }

        template <Color C>
        constexpr i32 down() {
            if constexpr (C == Color::kBlack) {
                return kUp;
            } else {
                return kDown;
            }
        }

        template <Color kC>
        constexpr i32 downLeft() {
            if constexpr (kC == Color::kBlack) {
                return kUpLeft;
            } else {
                return kDownLeft;
            }
        }

        template <Color kC>
        constexpr i32 downRight() {
            if constexpr (kC == Color::kBlack) {
                return kUpRight;
            } else {
                return kDownRight;
            }
        }
    } // namespace offsets

    namespace shifts {
        constexpr i32 kVertical = 8;
        constexpr i32 kHorizontal = 1;

        // '\'
        constexpr auto kDiagonalLR = kVertical - kHorizontal;
        // '/'
        constexpr auto kDiagonalRL = kVertical + kHorizontal;

        constexpr auto kDiagonal12LR = kVertical + kVertical - kHorizontal;
        constexpr auto kDiagonal12RL = kVertical + kVertical + kHorizontal;
        constexpr auto kDiagonal21LR = kVertical - kHorizontal - kHorizontal;
        constexpr auto kDiagonal21RL = kVertical + kHorizontal + kHorizontal;
    } // namespace shifts

    class BitboardSlot {
    public:
        [[nodiscard]] constexpr operator bool() const {
            return (m_board & m_mask) != 0;
        }

        constexpr BitboardSlot& operator=(bool rhs) {
            if (rhs) {
                m_board |= m_mask;
            } else {
                m_board &= ~m_mask;
            }

            return *this;
        }

    private:
        constexpr BitboardSlot(u64& board, i32 n) :
                m_board{board}, m_mask{u64{1} << n} {}

        u64& m_board;
        u64 m_mask;

        friend class Bitboard;
    };

    class Bitboard {
    public:
        static constexpr auto kRank1 = U64(0x00000000000000FF);
        static constexpr auto kRank2 = U64(0x000000000000FF00);
        static constexpr auto kRank3 = U64(0x0000000000FF0000);
        static constexpr auto kRank4 = U64(0x00000000FF000000);
        static constexpr auto kRank5 = U64(0x000000FF00000000);
        static constexpr auto kRank6 = U64(0x0000FF0000000000);
        static constexpr auto kRank7 = U64(0x00FF000000000000);
        static constexpr auto kRank8 = U64(0xFF00000000000000);

        static constexpr auto kFileA = U64(0x0101010101010101);
        static constexpr auto kFileB = U64(0x0202020202020202);
        static constexpr auto kFileC = U64(0x0404040404040404);
        static constexpr auto kFileD = U64(0x0808080808080808);
        static constexpr auto kFileE = U64(0x1010101010101010);
        static constexpr auto kFileF = U64(0x2020202020202020);
        static constexpr auto kFileG = U64(0x4040404040404040);
        static constexpr auto kFileH = U64(0x8080808080808080);

        static constexpr auto kDarkSquares = U64(0xAA55AA55AA55AA55);
        static constexpr auto kLightSquares = U64(0x55AA55AA55AA55AA);

        static constexpr auto kCenterSquares = U64(0x0000001818000000);

        static constexpr auto kAll = U64(0xFFFFFFFFFFFFFFFF);

        constexpr Bitboard(u64 board = 0) :
                m_board{board} {}

        [[nodiscard]] constexpr operator u64() const {
            return m_board;
        }

        [[nodiscard]] constexpr Bitboard operator&(Bitboard rhs) const {
            return m_board & rhs;
        }

        [[nodiscard]] constexpr Bitboard operator|(Bitboard rhs) const {
            return m_board | rhs;
        }

        [[nodiscard]] constexpr Bitboard operator^(Bitboard rhs) const {
            return m_board ^ rhs;
        }

        constexpr Bitboard& operator&=(Bitboard rhs) {
            m_board &= rhs;
            return *this;
        }

        constexpr Bitboard& operator|=(Bitboard rhs) {
            m_board |= rhs;
            return *this;
        }

        constexpr Bitboard& operator^=(Bitboard rhs) {
            m_board ^= rhs;
            return *this;
        }

        [[nodiscard]] constexpr Bitboard operator&(u64 rhs) const {
            return m_board & rhs;
        }

        [[nodiscard]] constexpr Bitboard operator|(u64 rhs) const {
            return m_board | rhs;
        }

        [[nodiscard]] constexpr Bitboard operator^(u64 rhs) const {
            return m_board ^ rhs;
        }

        constexpr Bitboard& operator&=(u64 rhs) {
            m_board &= rhs;
            return *this;
        }

        constexpr Bitboard& operator|=(u64 rhs) {
            m_board |= rhs;
            return *this;
        }

        constexpr Bitboard& operator^=(u64 rhs) {
            m_board ^= rhs;
            return *this;
        }

        [[nodiscard]] constexpr Bitboard operator&(i32 rhs) const {
            return m_board & static_cast<u64>(rhs);
        }

        [[nodiscard]] constexpr Bitboard operator|(i32 rhs) const {
            return m_board | static_cast<u64>(rhs);
        }

        [[nodiscard]] constexpr Bitboard operator^(i32 rhs) const {
            return m_board ^ static_cast<u64>(rhs);
        }

        constexpr Bitboard& operator&=(i32 rhs) {
            m_board &= static_cast<u64>(rhs);
            return *this;
        }

        constexpr Bitboard& operator|=(i32 rhs) {
            m_board |= static_cast<u64>(rhs);
            return *this;
        }

        constexpr Bitboard& operator^=(i32 rhs) {
            m_board ^= static_cast<u64>(rhs);
            return *this;
        }

        [[nodiscard]] constexpr Bitboard operator~() const {
            return ~m_board;
        }

        [[nodiscard]] constexpr Bitboard operator<<(i32 rhs) const {
            return m_board << rhs;
        }

        [[nodiscard]] constexpr Bitboard operator>>(i32 rhs) const {
            return m_board >> rhs;
        }

        constexpr Bitboard& operator<<=(i32 rhs) {
            m_board <<= rhs;
            return *this;
        }

        constexpr Bitboard& operator>>=(i32 rhs) {
            m_board >>= rhs;
            return *this;
        }

        [[nodiscard]] constexpr bool operator[](Square s) const {
            return m_board & squareBit(s);
        }

        [[nodiscard]] constexpr BitboardSlot operator[](Square s) {
            return BitboardSlot{m_board, static_cast<i32>(s)};
        }

        [[nodiscard]] constexpr i32 popcount() const {
            return std::popcount(m_board);
        }

        [[nodiscard]] constexpr bool empty() const {
            return m_board == 0;
        }

        [[nodiscard]] constexpr bool multiple() const {
            return util::resetLsb(m_board) != 0;
        }

        [[nodiscard]] constexpr bool one() const {
            return !empty() && !multiple();
        }

        [[nodiscard]] constexpr Square lowestSquare() const {
            return static_cast<Square>(util::ctz(m_board));
        }

        [[nodiscard]] constexpr Bitboard lowestBit() const {
            return util::isolateLsb(m_board);
        }

        [[nodiscard]] constexpr Square popLowestSquare() {
            const auto square = lowestSquare();
            m_board = util::resetLsb(m_board);
            return square;
        }

        [[nodiscard]] constexpr Bitboard popLowestBit() {
            const auto bit = lowestBit();
            m_board = util::resetLsb(m_board);
            return bit;
        }

        constexpr void clear() {
            m_board = 0;
        }

        [[nodiscard]] constexpr Bitboard shiftUp() const {
            return m_board << shifts::kVertical;
        }

        [[nodiscard]] constexpr Bitboard shiftDown() const {
            return m_board >> shifts::kVertical;
        }

        [[nodiscard]] constexpr Bitboard shiftLeft() const {
            return (m_board >> shifts::kHorizontal) & ~kFileH;
        }

        [[nodiscard]] constexpr Bitboard shiftLeftUnchecked() const {
            return m_board >> shifts::kHorizontal;
        }

        [[nodiscard]] constexpr Bitboard shiftRight() const {
            return (m_board << shifts::kHorizontal) & ~kFileA;
        }

        [[nodiscard]] constexpr Bitboard shiftRightUnchecked() const {
            return m_board << shifts::kHorizontal;
        }

        [[nodiscard]] constexpr Bitboard shiftUpLeft() const {
            return (m_board << shifts::kDiagonalLR) & ~kFileH;
        }

        [[nodiscard]] constexpr Bitboard shiftUpRight() const {
            return (m_board << shifts::kDiagonalRL) & ~kFileA;
        }

        [[nodiscard]] constexpr Bitboard shiftDownLeft() const {
            return (m_board >> shifts::kDiagonalRL) & ~kFileH;
        }

        [[nodiscard]] constexpr Bitboard shiftDownRight() const {
            return (m_board >> shifts::kDiagonalLR) & ~kFileA;
        }

        [[nodiscard]] constexpr Bitboard shiftUpUpLeft() const {
            return (m_board << shifts::kDiagonal12LR) & ~kFileH;
        }

        [[nodiscard]] constexpr Bitboard shiftUpUpRight() const {
            return (m_board << shifts::kDiagonal12RL) & ~kFileA;
        }

        [[nodiscard]] constexpr Bitboard shiftUpLeftLeft() const {
            return (m_board << shifts::kDiagonal21LR) & ~(kFileG | kFileH);
        }

        [[nodiscard]] constexpr Bitboard shiftUpRightRight() const {
            return (m_board << shifts::kDiagonal21RL) & ~(kFileA | kFileB);
        }

        [[nodiscard]] constexpr Bitboard shiftDownLeftLeft() const {
            return (m_board >> shifts::kDiagonal21RL) & ~(kFileG | kFileH);
        }

        [[nodiscard]] constexpr Bitboard shiftDownRightRight() const {
            return (m_board >> shifts::kDiagonal21LR) & ~(kFileA | kFileB);
        }

        [[nodiscard]] constexpr Bitboard shiftDownDownLeft() const {
            return (m_board >> shifts::kDiagonal12RL) & ~kFileH;
        }

        [[nodiscard]] constexpr Bitboard shiftDownDownRight() const {
            return (m_board >> shifts::kDiagonal12LR) & ~kFileA;
        }

        template <Color kC>
        [[nodiscard]] constexpr Bitboard shiftUpRelative() const {
            if constexpr (kC == Color::kBlack) {
                return shiftDown();
            } else {
                return shiftUp();
            }
        }

        template <Color kC>
        [[nodiscard]] constexpr Bitboard shiftUpLeftRelative() const {
            if constexpr (kC == Color::kBlack) {
                return shiftDownLeft();
            } else {
                return shiftUpLeft();
            }
        }

        template <Color kC>
        [[nodiscard]] constexpr Bitboard shiftUpRightRelative() const {
            if constexpr (kC == Color::kBlack) {
                return shiftDownRight();
            } else {
                return shiftUpRight();
            }
        }

        template <Color kC>
        [[nodiscard]] constexpr Bitboard shiftDownRelative() const {
            if constexpr (kC == Color::kBlack) {
                return shiftUp();
            } else {
                return shiftDown();
            }
        }

        template <Color kC>
        [[nodiscard]] constexpr Bitboard shiftDownLeftRelative() const {
            if constexpr (kC == Color::kBlack) {
                return shiftUpLeft();
            } else {
                return shiftDownLeft();
            }
        }

        template <Color kC>
        [[nodiscard]] constexpr Bitboard shiftDownRightRelative() const {
            if constexpr (kC == Color::kBlack) {
                return shiftUpRight();
            } else {
                return shiftDownRight();
            }
        }

        [[nodiscard]] constexpr Bitboard fillUp() const {
            auto b = m_board;
            b |= b << 8;
            b |= b << 16;
            b |= b << 32;
            return b;
        }

        [[nodiscard]] constexpr Bitboard fillDown() const {
            auto b = m_board;
            b |= b >> 8;
            b |= b >> 16;
            b |= b >> 32;
            return b;
        }

        template <Color C>
        [[nodiscard]] constexpr Bitboard fillUpRelative() const {
            if constexpr (C == Color::kBlack) {
                return fillDown();
            } else {
                return fillUp();
            }
        }

        template <Color C>
        [[nodiscard]] constexpr Bitboard fillDownRelative() const {
            if constexpr (C == Color::kBlack) {
                return fillUp();
            } else {
                return fillDown();
            }
        }

        [[nodiscard]] constexpr Bitboard fillFile() const {
            return fillUp() | fillDown();
        }

        [[nodiscard]] constexpr bool operator==(const Bitboard& other) const {
            return m_board == other.m_board;
        }

        [[nodiscard]] constexpr bool operator==(u64 other) const {
            return m_board == other;
        }

        [[nodiscard]] constexpr bool operator==(i32 other) const {
            return m_board == other;
        }

        [[nodiscard]] constexpr static Bitboard fromSquare(Square square) {
            return squareBit(square);
        }

        [[nodiscard]] constexpr static Bitboard fromSquareChecked(Square square) {
            return squareBitChecked(square);
        }

    private:
        u64 m_board;
    };

    namespace boards {
        constexpr Bitboard kRank1{Bitboard::kRank1};
        constexpr Bitboard kRank2{Bitboard::kRank2};
        constexpr Bitboard kRank3{Bitboard::kRank3};
        constexpr Bitboard kRank4{Bitboard::kRank4};
        constexpr Bitboard kRank5{Bitboard::kRank5};
        constexpr Bitboard kRank6{Bitboard::kRank6};
        constexpr Bitboard kRank7{Bitboard::kRank7};
        constexpr Bitboard kRank8{Bitboard::kRank8};

        constexpr Bitboard kFileA{Bitboard::kFileA};
        constexpr Bitboard kFileB{Bitboard::kFileB};
        constexpr Bitboard kFileC{Bitboard::kFileC};
        constexpr Bitboard kFileD{Bitboard::kFileD};
        constexpr Bitboard kFileE{Bitboard::kFileE};
        constexpr Bitboard kFileF{Bitboard::kFileF};
        constexpr Bitboard kFileG{Bitboard::kFileG};
        constexpr Bitboard kFileH{Bitboard::kFileH};

        constexpr auto kRanks = std::array{kRank1, kRank2, kRank3, kRank4, kRank5, kRank6, kRank7, kRank8};
        constexpr auto kFiles = std::array{kFileA, kFileB, kFileC, kFileD, kFileE, kFileF, kFileG, kFileH};

        constexpr Bitboard kDarkSquares{Bitboard::kDarkSquares};
        constexpr Bitboard kLightSquares{Bitboard::kLightSquares};

        constexpr Bitboard kCenterSquares{Bitboard::kCenterSquares};

        constexpr Bitboard kAll{Bitboard::kAll};

        template <Color kC>
        [[nodiscard]] constexpr Bitboard promotionRank() {
            if constexpr (kC == Color::kBlack) {
                return kRank1;
            } else {
                return kRank8;
            }
        }

        [[nodiscard]] constexpr Bitboard promotionRank(Color c) {
            return c == Color::kBlack ? kRank1 : kRank8;
        }

        template <Color kC>
        [[nodiscard]] constexpr Bitboard rank(i32 idx) {
            return kRanks[relativeRank<kC>(idx)];
        }
    } // namespace boards
} // namespace stormphrax

template <>
struct fmt::formatter<stormphrax::Bitboard> : fmt::formatter<std::string_view> {
    constexpr auto format(const stormphrax::Bitboard& value, format_context& ctx) const {
        for (stormphrax::i32 rank = 7; rank >= 0; --rank) {
            for (stormphrax::i32 file = 0; file < 8; ++file) {
                if (file > 0) {
                    format_to(ctx.out(), " ");
                }

                format_to(ctx.out(), "{}", value[stormphrax::toSquare(rank, file)] ? '1' : '.');
            }

            format_to(ctx.out(), "\n");
        }

        return ctx.out();
    }
};
