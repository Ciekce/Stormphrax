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

#include "../types.h"

#include <compare>

namespace stormphrax::util {
    class Instant {
    public:
        [[nodiscard]] f64 elapsed() const;

        inline Instant operator+(f64 time) const {
            return Instant{m_time + time};
        }

        inline Instant operator-(f64 time) const {
            return Instant{m_time - time};
        }

        [[nodiscard]] inline std::partial_ordering operator<=>(const Instant& other) const {
            return m_time <=> other.m_time;
        }

        [[nodiscard]] static Instant now();

    private:
        explicit Instant(f64 time) :
                m_time{time} {}

        f64 m_time;
    };
} // namespace stormphrax::util
