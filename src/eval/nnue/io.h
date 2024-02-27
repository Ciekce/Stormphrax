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

#include <istream>
#include <ostream>
#include <span>
#include <array>

namespace stormphrax::eval::nnue
{
	template <typename T, usize BlockSize = 64>
	inline auto readPadded(std::istream &stream, std::span<T> dst) -> std::istream &
	{
		stream.read(reinterpret_cast<char *>(dst.data()), dst.size_bytes());
		stream.ignore(dst.size_bytes() % BlockSize);

		return stream;
	}

	template <typename T, usize Count, usize BlockSize = 64>
	inline auto readPadded(std::istream &stream, std::array<T, Count> &dst) -> std::istream &
	{
		return readPadded<T, BlockSize>(stream, std::span{dst});
	}

	template <typename T, usize BlockSize = 64>
	inline auto writePadded(std::ostream &stream, std::span<const T> src) -> std::ostream &
	{
		stream.write(reinterpret_cast<const char *>(src.data()), src.size_bytes());

		std::array<std::byte, BlockSize> empty{};
		stream.write(reinterpret_cast<const char *>(empty.data()), src.size_bytes() % BlockSize);

		return stream;
	}

	template <typename T, usize Count, usize BlockSize = 64>
	inline auto writePadded(std::ostream &stream, const std::array<T, Count> &src) -> std::ostream &
	{
		return writePadded<T, BlockSize>(stream, std::span{src});
	}
}
