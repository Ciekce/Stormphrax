/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "loader.h"

#include "../../util/align.h"
#include "../../util/simd.h"
#include "../nnue.h"

namespace stormphrax::eval::nnue {
    NetworkLoader::NetworkLoader(const std::byte* buffer, usize bufferSize) :
            m_buffer{buffer}, m_remaining{bufferSize} {}

    const std::byte* NetworkLoader::get(usize size) {
        if (size > m_remaining) {
            eprintln("NetworkLoader: Requested {} bytes, {} remaining", size, m_remaining);
            return nullptr;
        }

        const auto* ptr = m_buffer;

        m_buffer += size;
        m_remaining -= size;

        if (!util::isAligned<util::simd::kAlignment>(ptr)) {
            eprintln("NetworkLoader: Unaligned pointer");
            return nullptr;
        }

        return ptr;
    }
} // namespace stormphrax::eval::nnue
