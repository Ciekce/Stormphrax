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
#include <variant>
#include <cassert>
#include <vector>
#include <iostream>
#include <memory>

#include "io.h"
#include "../../util/cemath.h"
#include "../../util/memstream.h"
#include "../../3rdparty/zstd/zstd.h"

namespace stormphrax::eval::nnue
{
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
		inline auto readI8s(std::span<i8> dst) -> bool final
		{
			return read(reinterpret_cast<std::byte *>(dst.data()), dst.size_bytes());
		}

		inline auto writeI8s(std::span<const i8> src) -> bool final
		{
			return write(reinterpret_cast<const std::byte *>(src.data()), src.size_bytes());
		}

		inline auto readI16s(std::span<i16> dst) -> bool final
		{
			return read(reinterpret_cast<std::byte *>(dst.data()), dst.size_bytes());
		}

		inline auto writeI16s(std::span<const i16> src) -> bool final
		{
			return write(reinterpret_cast<const std::byte *>(src.data()), src.size_bytes());
		}

		inline auto readF32s(std::span<f32> dst) -> bool final
		{
			return read(reinterpret_cast<std::byte *>(dst.data()), dst.size_bytes());
		}

		inline auto writeF32s(std::span<const f32> src) -> bool final
		{
			return write(reinterpret_cast<const std::byte *>(src.data()), src.size_bytes());
		}

	private:
		std::variant<std::istream *, std::ostream *> m_stream;

		inline auto read(std::byte *dst, usize n) -> bool
		{
			if (!std::holds_alternative<std::istream *>(m_stream))
			{
				assert(false);
				return false;
			}

			auto &stream = *std::get<std::istream *>(m_stream);

			const auto padding = calcPadding(n);

			stream.read(reinterpret_cast<char *>(dst), static_cast<std::streamsize>(n));
			stream.ignore(static_cast<std::streamsize>(padding));

			return !stream.fail();
		}

		inline auto write(const std::byte *src, usize n) -> bool
		{
			if (!std::holds_alternative<std::ostream *>(m_stream))
			{
				assert(false);
				return false;
			}

			static constexpr std::array<std::byte, BlockSize> Empty{};

			auto &stream = *std::get<std::ostream *>(m_stream);

			const auto padding = calcPadding(n);

			stream.write(reinterpret_cast<const char *>(src), static_cast<std::streamsize>(n));
			stream.write(reinterpret_cast<const char *>(Empty.data()), padding);

			return !stream.fail();
		}

		[[nodiscard]] static constexpr auto calcPadding(usize v) -> usize
		{
			return v - util::ceilDiv(v, BlockSize) * BlockSize;
		}
	};

	class ZstdParamStream final : public IParamStream
	{
	public:
		explicit ZstdParamStream(std::istream &in);

		~ZstdParamStream() final;

	protected:
		inline auto readI8s(std::span<i8> dst) -> bool final
		{
			return read(reinterpret_cast<std::byte *>(dst.data()), dst.size_bytes());
		}

		inline auto writeI8s(std::span<const i8> src) -> bool final
		{
			std::cerr << "ZstdParamStream::writeI8s" << std::endl;
			std::terminate();
		}

		inline auto readI16s(std::span<i16> dst) -> bool final
		{
			return read(reinterpret_cast<std::byte *>(dst.data()), dst.size_bytes());
		}

		inline auto writeI16s(std::span<const i16> src) -> bool final
		{
			std::cerr << "ZstdParamStream::writeI16s" << std::endl;
			std::terminate();
		}

		inline auto readF32s(std::span<f32> dst) -> bool final
		{
			return read(reinterpret_cast<std::byte *>(dst.data()), dst.size_bytes());
		}

		inline auto writeF32s(std::span<const f32> src) -> bool final
		{
			std::cerr << "ZstdParamStream::writeF32s" << std::endl;
			std::terminate();
		}

	private:
		std::istream &m_stream;

		std::vector<std::byte> m_inBuf{};
		std::vector<std::byte> m_outBuf{};

		usize m_pos{};
		usize m_end{};

		usize m_result{};

		ZSTD_DStream *m_dStream;

		ZSTD_inBuffer m_input{};
		ZSTD_outBuffer m_output{};

		bool m_fail{false};

		auto fillBuffer() -> bool;
		auto read(std::byte *dst, usize n) -> bool;
	};
}
