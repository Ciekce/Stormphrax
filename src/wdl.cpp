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

#include "wdl.h"

#include <array>
#include <numeric>

namespace stormphrax::wdl
{
	auto wdlParams(i32 material) -> std::pair<f64, f64>
	{
		static constexpr auto As = std::array {
			-21.62471742, 57.27533161, -196.12706447, 419.75122408
		};

		static constexpr auto Bs = std::array {
			-41.67494726, 81.27819509, 10.83073229, 48.03832274
		};

		static_assert(Material58NormalizationK == static_cast<i32>(std::reduce(As.begin(), As.end())));

		const auto m = static_cast<f64>(std::clamp(material, 17, 78)) / 58.0;

		return {
			(((As[0] * m + As[1]) * m + As[2]) * m) + As[3],
			(((Bs[0] * m + Bs[1]) * m + Bs[2]) * m) + Bs[3]
		};
	}

	auto wdlModel(Score povScore, i32 material) -> std::pair<i32, i32>
	{
		const auto [a, b] = wdlParams(material);

		const auto x = static_cast<f64>(povScore);

		return {
			static_cast<i32>(std::round(1000.0 / (1.0 + std::exp((a - x) / b)))),
			static_cast<i32>(std::round(1000.0 / (1.0 + std::exp((a + x) / b))))
		};
	}
}
