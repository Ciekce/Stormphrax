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

#include <bit>
#include <limits>
#include <random>

namespace stormphrax::util::rng {
    class Jsf64Rng {
    public:
        using result_type = u64;

        explicit constexpr Jsf64Rng(u64 seed) :
                m_b{seed}, m_c{seed}, m_d{seed} {
            for (usize i = 0; i < 20; ++i) {
                nextU64();
            }
        }

        constexpr u64 nextU64() {
            const auto e = m_a - std::rotl(m_b, 7);
            m_a = m_b ^ std::rotl(m_c, 13);
            m_b = m_c + std::rotl(m_d, 37);
            m_c = m_d + e;
            m_d = e + m_a;
            return m_d;
        }

        constexpr u32 nextU32() {
            return static_cast<u32>(nextU64() >> 32);
        }

        constexpr u32 nextU32(u32 bound) {
            if (bound == 0) {
                return 0;
            }

            auto x = nextU32();
            auto m = static_cast<u64>(x) * static_cast<u64>(bound);
            auto l = static_cast<u32>(m);

            if (l < bound) {
                auto t = -bound;

                if (t >= bound) {
                    t -= bound;

                    if (t >= bound) {
                        t %= bound;
                    }
                }

                while (l < t) {
                    x = nextU32();
                    m = static_cast<u64>(x) * static_cast<u64>(bound);
                    l = static_cast<u32>(m);
                }
            }

            return static_cast<u32>(m >> 32);
        }

        constexpr u64 operator()() {
            return nextU64();
        }

        static constexpr u64 min() {
            return std::numeric_limits<u64>::min();
        }

        static constexpr u64 max() {
            return std::numeric_limits<u64>::max();
        }

    private:
        u64 m_a{0xF1EA5EED};
        u64 m_b;
        u64 m_c;
        u64 m_d;
    };

    inline u64 generateSingleSeed() {
        std::random_device generator{};
        return static_cast<u64>(generator()) << 32 | generator();
    }

    // splitmix64, suitable for seeding jsf64
    class SeedGenerator {
    public:
        explicit SeedGenerator(u64 seed = generateSingleSeed()) :
                m_state{seed} {}

        constexpr u64 nextSeed() {
            m_state += U64(0x9E3779B97F4A7C15);

            auto z = m_state;

            z = (z ^ (z >> 30)) * U64(0xBF58476D1CE4E5B9);
            z = (z ^ (z >> 27)) * U64(0x94D049BB133111EB);

            return z ^ (z >> 31);
        }

    private:
        u64 m_state{};
    };
} // namespace stormphrax::util::rng
