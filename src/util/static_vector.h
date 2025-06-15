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

#include <algorithm>
#include <array>
#include <cassert>

namespace stormphrax {
    template <typename T, usize kCapacity>
    class StaticVector {
    public:
        StaticVector() = default;

        StaticVector(const StaticVector<T, kCapacity>& other) {
            *this = other;
        }

        inline void push(const T& elem) {
            assert(m_size < kCapacity);
            m_data[m_size++] = elem;
        }

        inline void push(T&& elem) {
            assert(m_size < kCapacity);
            m_data[m_size++] = std::move(elem);
        }

        inline T pop() {
            assert(m_size > 0);
            return std::move(m_data[--m_size]);
        }

        inline void clear() {
            m_size = 0;
        }

        inline void fill(const T& v) {
            m_data.fill(v);
        }

        [[nodiscard]] inline usize size() const {
            return m_size;
        }

        [[nodiscard]] inline bool empty() const {
            return m_size == 0;
        }

        [[nodiscard]] inline const T& operator[](usize i) const {
            assert(i < m_size);
            return m_data[i];
        }

        [[nodiscard]] inline auto begin() {
            return m_data.begin();
        }

        [[nodiscard]] inline auto end() {
            return m_data.begin() + static_cast<std::ptrdiff_t>(m_size);
        }

        [[nodiscard]] inline T& operator[](usize i) {
            assert(i < m_size);
            return m_data[i];
        }

        [[nodiscard]] inline auto begin() const {
            return m_data.begin();
        }

        [[nodiscard]] inline auto end() const {
            return m_data.begin() + static_cast<std::ptrdiff_t>(m_size);
        }

        inline void resize(usize size) {
            assert(size <= kCapacity);
            m_size = size;
        }

        inline StaticVector<T, kCapacity>& operator=(const StaticVector<T, kCapacity>& other) {
            std::copy(other.begin(), other.end(), begin());
            m_size = other.m_size;
            return *this;
        }

    private:
        std::array<T, kCapacity> m_data{};
        usize m_size{0};
    };
} // namespace stormphrax
