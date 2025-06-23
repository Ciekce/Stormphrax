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

#include "wdl.h"

#include <array>
#include <numeric>

namespace stormphrax::wdl {
    std::pair<f64, f64> wdlParams(i32 material) {
        static constexpr auto kAs = std::array{-96.02243718, 269.74715145, -333.86830676, 436.37312689};
        static constexpr auto kBs = std::array{-25.83309316, 94.79252729, -54.62661884, 80.45166722};

        static_assert(kMaterial58NormalizationK == static_cast<i32>(std::reduce(kAs.begin(), kAs.end())));

        const auto m = static_cast<f64>(std::clamp(material, 17, 78)) / 58.0;

        return {((kAs[0] * m + kAs[1]) * m + kAs[2]) * m + kAs[3], ((kBs[0] * m + kBs[1]) * m + kBs[2]) * m + kBs[3]};
    }

    std::pair<i32, i32> wdlModel(Score povScore, i32 material) {
        const auto [a, b] = wdlParams(material);

        const auto x = static_cast<f64>(povScore);

        return {
            static_cast<i32>(std::round(1000.0 / (1.0 + std::exp((a - x) / b)))),
            static_cast<i32>(std::round(1000.0 / (1.0 + std::exp((a + x) / b))))
        };
    }
} // namespace stormphrax::wdl
