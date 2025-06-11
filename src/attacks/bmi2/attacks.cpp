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

#include "../attacks.h"

#if SP_HAS_BMI2
namespace stormphrax::attacks {
    using namespace bmi2;

    namespace {
        std::array<u16, RookData.tableSize> generateRookAttacks() {
            std::array<u16, RookData.tableSize> dst{};

            for (u32 square = 0; square < 64; ++square) {
                const auto& data = RookData.data[square];
                const auto entries = 1 << data.srcMask.popcount();

                for (u32 i = 0; i < entries; ++i) {
                    const auto occupancy = util::pdep(i, data.srcMask);

                    Bitboard attacks{};

                    for (const auto dir : {offsets::Up, offsets::Down, offsets::Left, offsets::Right}) {
                        attacks |= internal::generateSlidingAttacks(static_cast<Square>(square), dir, occupancy);
                    }

                    dst[data.offset + i] = static_cast<u16>(util::pext(attacks, data.dstMask));
                }
            }

            return dst;
        }

        std::array<Bitboard, BishopData.tableSize> generateBishopAttacks() {
            std::array<Bitboard, BishopData.tableSize> dst{};

            for (u32 square = 0; square < 64; ++square) {
                const auto& data = BishopData.data[square];
                const auto entries = 1 << data.mask.popcount();

                for (u32 i = 0; i < entries; ++i) {
                    const auto occupancy = util::pdep(i, data.mask);

                    for (const auto dir : {offsets::UpLeft, offsets::UpRight, offsets::DownLeft, offsets::DownRight}) {
                        dst[data.offset + i] |=
                            internal::generateSlidingAttacks(static_cast<Square>(square), dir, occupancy);
                    }
                }
            }

            return dst;
        }
    } // namespace

    const std::array<u16, RookData.tableSize> RookAttacks = generateRookAttacks();
    const std::array<Bitboard, BishopData.tableSize> BishopAttacks = generateBishopAttacks();
} // namespace stormphrax::attacks
#endif // SP_HAS_BMI2
