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

#include <atomic>

#include "move.h"

namespace stormphrax::search {
    struct SearchData {
        i32 rootDepth{};
        i32 seldepth{};

        std::atomic<usize> nodes{};
        std::atomic<usize> tbhits{};

        SearchData() = default;

        SearchData(const SearchData& other) {
            *this = other;
        }

        inline void updateSeldepth(i32 ply) {
            seldepth = std::max(seldepth, ply + 1);
        }

        [[nodiscard]] inline usize loadNodes() const {
            return nodes.load(std::memory_order::relaxed);
        }

        inline void incNodes() {
            nodes.fetch_add(1, std::memory_order::relaxed);
        }

        [[nodiscard]] inline usize loadTbHits() const {
            return tbhits.load(std::memory_order::relaxed);
        }

        inline void incTbHits() {
            tbhits.fetch_add(1, std::memory_order::relaxed);
        }

        SearchData& operator=(const SearchData& other) {
            rootDepth = other.rootDepth;
            seldepth = other.seldepth;

            nodes.store(other.nodes.load());
            tbhits.store(other.tbhits.load());

            return *this;
        }
    };

    struct PlayedMove {
        Piece moving;
        Square dst;
    };
} // namespace stormphrax::search
