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

#if !SP_HAS_BMI2
namespace stormphrax::attacks {
    using namespace black_magic;

    namespace {
        std::array<Bitboard, kRookData.tableSize> generateRookAttacks() {
            std::array<Bitboard, kRookData.tableSize> dst{};

            for (u32 square = 0; square < 64; ++square) {
                const auto& data = kRookData.data[square];

                const auto invMask = ~data.mask;
                const auto maxEntries = 1 << invMask.popcount();

                for (u32 i = 0; i < maxEntries; ++i) {
                    const auto occupancy = util::pdep(i, invMask);
                    const auto idx = getRookIdx(occupancy, static_cast<Square>(square));

                    if (!dst[data.offset + idx].empty()) {
                        continue;
                    }

                    for (const auto dir : {offsets::kUp, offsets::kDown, offsets::kLeft, offsets::kRight}) {
                        dst[data.offset + idx] |=
                            internal::generateSlidingAttacks(static_cast<Square>(square), dir, occupancy);
                    }
                }
            }

            return dst;
        }

        std::array<Bitboard, kBishopData.tableSize> generateBishopAttacks() {
            std::array<Bitboard, kBishopData.tableSize> dst{};

            for (u32 square = 0; square < 64; ++square) {
                const auto& data = kBishopData.data[square];

                const auto invMask = ~data.mask;
                const auto maxEntries = 1 << invMask.popcount();

                for (u32 i = 0; i < maxEntries; ++i) {
                    const auto occupancy = util::pdep(i, invMask);
                    const auto idx = getBishopIdx(occupancy, static_cast<Square>(square));

                    if (!dst[data.offset + idx].empty()) {
                        continue;
                    }

                    for (const auto dir :
                         {offsets::kUpLeft, offsets::kUpRight, offsets::kDownLeft, offsets::kDownRight})
                    {
                        dst[data.offset + idx] |=
                            internal::generateSlidingAttacks(static_cast<Square>(square), dir, occupancy);
                    }
                }
            }

            return dst;
        }
    } // namespace

    const std::array<Bitboard, kRookData.tableSize> g_rookAttacks = generateRookAttacks();
    const std::array<Bitboard, kBishopData.tableSize> g_bishopAttacks = generateBishopAttacks();
} // namespace stormphrax::attacks
#endif // !SP_HAS_BMI2
