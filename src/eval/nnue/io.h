/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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

#include <span>
#include <array>

namespace stormphrax::eval::nnue
{
	class IParamStream
	{
	public:
		virtual ~IParamStream() = default;

		template <typename T>
		inline auto read(std::span<T> dst) -> bool = delete;

		template <>
		inline auto read<i8>(std::span<i8> dst) -> bool
		{
			return readI8s(dst);
		}

		template <>
		inline auto read<i16>(std::span<i16> dst) -> bool
		{
			return readI16s(dst);
		}

		template <>
		inline auto read<i32>(std::span<i32> dst) -> bool
		{
			return readI32s(dst);
		}

		template <typename T, usize Size>
		inline auto read(std::array<T, Size> &dst)
		{
			return read(std::span<T, std::dynamic_extent>{dst});
		}

		template <typename T>
		inline auto write(std::span<const T> src) -> bool = delete;

		template <>
		inline auto write<i8>(std::span<const i8> src) -> bool
		{
			return writeI8s(src);
		}

		template <>
		inline auto write<i16>(std::span<const i16> src) -> bool
		{
			return writeI16s(src);
		}

		template <>
		inline auto write<i32>(std::span<const i32> src) -> bool
		{
			return writeI32s(src);
		}

		template <typename T, usize Size>
		inline auto write(const std::array<T, Size> &src)
		{
			return write(std::span<const T, std::dynamic_extent>{src});
		}

	protected:
		virtual auto readI8s(std::span<i8> dst) -> bool = 0;
		virtual auto writeI8s(std::span<const i8> src) -> bool = 0;

		virtual auto readI16s(std::span<i16> dst) -> bool = 0;
		virtual auto writeI16s(std::span<const i16> src) -> bool = 0;

		virtual auto readI32s(std::span<i32> dst) -> bool = 0;
		virtual auto writeI32s(std::span<const i32> src) -> bool = 0;
	};
}
