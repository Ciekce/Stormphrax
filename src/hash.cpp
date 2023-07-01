/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

#include "hash.h"

#include "util/rng.h"

namespace stormphrax::hash
{
	namespace
	{
		constexpr u64 Seed = U64(0xD06C659954EC904A);

		auto generateHashes()
		{
			std::array<u64, sizes::Total> hashes{};

			Jsf64Rng rng{Seed};

			for (auto &hash : hashes)
			{
				hash = rng.nextU64();
			}

			return hashes;
		}
	}

	const std::array<u64, sizes::Total> Hashes = generateHashes();
}
