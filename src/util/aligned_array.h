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

#pragma once

#include "../types.h"

#include <array>
#include <span>

namespace stormphrax::util {
    template <usize kAlignment, typename T, usize kCount>
    class AlignedArray {
    public:
        [[nodiscard]] constexpr T& at(usize idx) {
            return m_array.at(idx);
        }

        [[nodiscard]] constexpr const T& at(usize idx) const {
            return m_array.at(idx);
        }

        [[nodiscard]] constexpr T& operator[](usize idx) {
            return m_array[idx];
        }

        [[nodiscard]] constexpr const T& operator[](usize idx) const {
            return m_array[idx];
        }

        [[nodiscard]] constexpr T& front() {
            return m_array.front();
        }

        [[nodiscard]] constexpr const T& front() const {
            return m_array.front();
        }

        [[nodiscard]] constexpr T& back() {
            return m_array.back();
        }

        [[nodiscard]] constexpr const T& back() const {
            return m_array.back();
        }

        [[nodiscard]] constexpr T* data() {
            return m_array.data();
        }

        [[nodiscard]] constexpr const T* data() const {
            return m_array.data();
        }

        [[nodiscard]] constexpr auto begin() {
            return m_array.begin();
        }

        [[nodiscard]] constexpr auto begin() const {
            return m_array.begin();
        }

        [[nodiscard]] constexpr auto cbegin() const {
            return m_array.cbegin();
        }

        [[nodiscard]] constexpr auto end() {
            return m_array.end();
        }

        [[nodiscard]] constexpr auto end() const {
            return m_array.end();
        }

        [[nodiscard]] constexpr auto cend() const {
            return m_array.cend();
        }

        [[nodiscard]] constexpr auto rbegin() {
            return m_array.rbegin();
        }

        [[nodiscard]] constexpr auto rbegin() const {
            return m_array.rbegin();
        }

        [[nodiscard]] constexpr auto crbegin() const {
            return m_array.crbegin();
        }

        [[nodiscard]] constexpr auto rend() {
            return m_array.rend();
        }

        [[nodiscard]] constexpr auto rend() const {
            return m_array.rend();
        }

        [[nodiscard]] constexpr auto crend() const {
            return m_array.crend();
        }

        [[nodiscard]] constexpr auto empty() const {
            return m_array.empty();
        }

        [[nodiscard]] constexpr usize size() const {
            return m_array.size();
        }

        [[nodiscard]] constexpr usize max_size() const {
            return m_array.max_size();
        }

        constexpr void fill(const T& value) {
            m_array.fill(value);
        }

        constexpr void swap(AlignedArray& other) {
            m_array.swap(other.m_array);
        }

        [[nodiscard]] constexpr std::array<T, kCount>& array() {
            return m_array;
        }

        [[nodiscard]] constexpr const std::array<T, kCount>& array() const {
            return m_array;
        }

        constexpr operator std::array<T, kCount>&() {
            return m_array;
        }

        constexpr operator const std::array<T, kCount>&() const {
            return m_array;
        }

        constexpr operator std::span<T, kCount>() {
            return m_array;
        }

        constexpr operator std::span<const T, kCount>() const {
            return m_array;
        }

    private:
        alignas(kAlignment) std::array<T, kCount> m_array;
    };

    template <usize kAlignment, typename T, usize kCount>
    void swap(AlignedArray<kAlignment, T, kCount>& a, AlignedArray<kAlignment, T, kCount>& b) noexcept {
        a.swap(b);
    }
} // namespace stormphrax::util
