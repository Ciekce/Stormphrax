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

#include "../../types.h"

#include <array>
#include <span>

namespace stormphrax::eval::nnue {
    class IParamStream {
    public:
        virtual ~IParamStream() = default;

        template <typename T>
        inline bool read(std::span<T> dst) = delete;

        template <>
        inline bool read<i8>(std::span<i8> dst) {
            return readI8s(dst);
        }

        template <>
        inline bool read<i16>(std::span<i16> dst) {
            return readI16s(dst);
        }

        template <>
        inline bool read<i32>(std::span<i32> dst) {
            return readI32s(dst);
        }

        template <typename T, usize Size>
        inline bool read(std::array<T, Size>& dst) {
            return read(std::span<T, std::dynamic_extent>{dst});
        }

        template <typename T>
        inline bool write(std::span<const T> src) = delete;

        template <>
        inline bool write<i8>(std::span<const i8> src) {
            return writeI8s(src);
        }

        template <>
        inline bool write<i16>(std::span<const i16> src) {
            return writeI16s(src);
        }

        template <>
        inline bool write<i32>(std::span<const i32> src) {
            return writeI32s(src);
        }

        template <typename T, usize Size>
        inline bool write(const std::array<T, Size>& src) {
            return write(std::span<const T, std::dynamic_extent>{src});
        }

    protected:
        virtual bool readI8s(std::span<i8> dst) = 0;
        virtual bool writeI8s(std::span<const i8> src) = 0;

        virtual bool readI16s(std::span<i16> dst) = 0;
        virtual bool writeI16s(std::span<const i16> src) = 0;

        virtual bool readI32s(std::span<i32> dst) = 0;
        virtual bool writeI32s(std::span<const i32> src) = 0;
    };
} // namespace stormphrax::eval::nnue
