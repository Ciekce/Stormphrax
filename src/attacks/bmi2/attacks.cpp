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
namespace stormphrax::attacks::lookup {
    using namespace bmi2;

    namespace {
        std::array<u16, kRookData.tableSize> generateRookAttacks() {
            std::array<u16, kRookData.tableSize> dst{};

            for (u32 sq = 0; sq < Squares::kCount; ++sq) {
                const auto& data = kRookData.data[sq];
                const auto entries = 1 << data.srcMask.popcount();

                for (u32 i = 0; i < entries; ++i) {
                    const auto occupancy = util::pdep(i, data.srcMask);

                    Bitboard attacks{};

                    for (const auto dir : {offsets::kUp, offsets::kDown, offsets::kLeft, offsets::kRight}) {
                        attacks |= internal::generateSlidingAttacks(Square::fromRaw(sq), dir, occupancy);
                    }

                    dst[data.offset + i] = static_cast<u16>(util::pext(attacks, data.dstMask));
                }
            }

            return dst;
        }

        std::array<Bitboard, kBishopData.tableSize> generateBishopAttacks() {
            std::array<Bitboard, kBishopData.tableSize> dst{};

            for (u32 sq = 0; sq < Squares::kCount; ++sq) {
                const auto& data = kBishopData.data[sq];
                const auto entries = 1 << data.mask.popcount();

                for (u32 i = 0; i < entries; ++i) {
                    const auto occupancy = util::pdep(i, data.mask);

                    for (const auto dir :
                         {offsets::kUpLeft, offsets::kUpRight, offsets::kDownLeft, offsets::kDownRight})
                    {
                        dst[data.offset + i] |= internal::generateSlidingAttacks(Square::fromRaw(sq), dir, occupancy);
                    }
                }
            }

            return dst;
        }
    } // namespace

    const std::array<u16, kRookData.tableSize> g_rookAttacks = generateRookAttacks();
    const std::array<Bitboard, kBishopData.tableSize> g_bishopAttacks = generateBishopAttacks();
} // namespace stormphrax::attacks::lookup
#endif // SP_HAS_BMI2
