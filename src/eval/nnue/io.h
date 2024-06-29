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
#include <variant>
#include <cassert>

#include "../../util/cemath.h"

namespace stormphrax::eval::nnue
{
	class IParamStream
	{
	public:
		virtual ~IParamStream() = default;

		template <typename T>
		inline auto read(std::span<T> dst) -> bool = delete;

		template <>
		inline auto read<i16>(std::span<i16> dst) -> bool
		{
			return readI16s(dst);
		}

		template <typename T, usize Size>
		inline auto read(std::array<T, Size> &dst)
		{
			return read(std::span<T, std::dynamic_extent>{dst});
		}

		template <typename T>
		inline auto write(std::span<T> src) -> bool = delete;

		template <>
		inline auto write<i16>(std::span<i16> src) -> bool
		{
			return writeI16s(src);
		}

		template <typename T, usize Size>
		inline auto write(const std::array<T, Size> &src)
		{
			return write(std::span<T, std::dynamic_extent>{src});
		}

	protected:
		virtual auto readI16s(std::span<i16> dst) -> bool = 0;
		virtual auto writeI16s(std::span<const i16> src) -> bool = 0;
	};

	template <usize BlockSize>
	class PaddedParamStream final : public IParamStream
	{
	public:
		explicit PaddedParamStream(std::istream &in)
			: m_stream{&in} {}
		explicit PaddedParamStream(std::ostream &out)
			: m_stream{&out} {}

		~PaddedParamStream() final = default;

	protected:
		inline auto readI16s(std::span<i16> dst) -> bool final
		{
			return read(dst.data(), dst.size_bytes());
		}

		inline auto writeI16s(std::span<const i16> src) -> bool final
		{
			return write(src.data(), src.size_bytes());
		}

	private:
		std::variant<std::istream *, std::ostream *> m_stream;

		inline auto read(void *dst, usize n) -> bool
		{
			if (!std::holds_alternative<std::istream *>(m_stream))
			{
				assert(false);
				return false;
			}

			auto &stream = *std::get<std::istream *>(m_stream);

			const auto padding = calcPadding(n);

			stream.read(static_cast<char *>(dst), static_cast<std::streamsize>(n));
			stream.ignore(static_cast<std::streamsize>(padding));

			return !stream.fail();
		}

		inline auto write(const void *src, usize n) -> bool
		{
			if (!std::holds_alternative<std::ostream *>(m_stream))
			{
				assert(false);
				return false;
			}

			static constexpr std::array<std::byte, BlockSize> Empty{};

			auto &stream = *std::get<std::ostream *>(m_stream);

			const auto padding = calcPadding(n);

			stream.write(static_cast<const char *>(src), static_cast<std::streamsize>(n));
			stream.write(reinterpret_cast<const char *>(Empty.data()), padding);

			return !stream.fail();
		}

		[[nodiscard]] static constexpr auto calcPadding(usize v) -> usize
		{
			return v - util::ceilDiv(v, BlockSize) * BlockSize;
		}
	};
}
