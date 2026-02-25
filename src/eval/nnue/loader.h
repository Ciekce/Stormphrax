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

        virtual bool read(std::span<i8> dst) = 0;
        virtual bool read(std::span<i16> dst) = 0;
        virtual bool read(std::span<i32> dst) = 0;

        template <typename T, usize kSize>
        inline bool read(std::array<T, kSize>& dst) {
            return read(std::span<T>{dst});
        }
    };

    class NetworkLoader {
    public:
        explicit NetworkLoader(const std::byte* buffer, usize bufferSize);

        template <usize kN>
        bool load(std::span<const i8, kN>& dst) {
            const auto* bytes = get(dst.size_bytes());
            if (!bytes) {
                return false;
            }
            dst = std::span<const i8, kN>{reinterpret_cast<const i8*>(bytes), kN};
            return true;
        }

        template <usize kN>
        bool load(std::span<const i16, kN>& dst) {
            const auto* bytes = get(dst.size_bytes());
            if (!bytes) {
                return false;
            }
            dst = std::span<const i16, kN>{reinterpret_cast<const i16*>(bytes), kN};
            return true;
        }

        template <usize kN>
        bool load(std::span<const i32, kN>& dst) {
            const auto* bytes = get(dst.size_bytes());
            if (!bytes) {
                return false;
            }
            dst = std::span<const i32, kN>{reinterpret_cast<const i32*>(bytes), kN};
            return true;
        }

    private:
        const std::byte* m_buffer;
        usize m_remaining;

        [[nodiscard]] const std::byte* get(usize size);
    };

    template <typename T, usize kSize>
    [[nodiscard]] static std::span<const T, kSize> nullSpan() {
        return std::span<const T, kSize>{static_cast<const T*>(nullptr), kSize};
    }
} // namespace stormphrax::eval::nnue

#define SP_NETWORK_PARAMS(Type, Size, Name) \
    std::span<const Type, Size> Name = stormphrax::eval::nnue::nullSpan<Type, Size>()
