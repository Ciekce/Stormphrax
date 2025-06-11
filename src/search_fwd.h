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

        std::atomic<i32> seldepth{};
        std::atomic<usize> nodes{};
        std::atomic<usize> tbhits{};

        SearchData() = default;

        SearchData(const SearchData& other) {
            *this = other;
        }

        [[nodiscard]] inline auto loadSeldepth() const {
            return seldepth.load(std::memory_order::relaxed);
        }

        inline auto updateSeldepth(i32 v) {
            if (v > loadSeldepth())
                seldepth.store(v, std::memory_order::relaxed);
        }

        [[nodiscard]] inline auto loadNodes() const {
            return nodes.load(std::memory_order::relaxed);
        }

        inline auto incNodes() {
            nodes.fetch_add(1, std::memory_order::relaxed);
        }

        [[nodiscard]] inline auto loadTbHits() const {
            return tbhits.load(std::memory_order::relaxed);
        }

        inline auto incTbHits() {
            tbhits.fetch_add(1, std::memory_order::relaxed);
        }

        auto operator=(const SearchData& other) -> SearchData& {
            rootDepth = other.rootDepth;

            seldepth.store(other.seldepth.load());
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
