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

#include "../types.h"

#include <istream>
#include <span>
#include <cassert>
#include <limits>

namespace stormphrax::util
{
	class MemoryBuffer : public std::streambuf
	{
	public:
		explicit MemoryBuffer(std::span<const std::byte> data)
			: m_begin{reinterpret_cast<char *>(const_cast<std::byte *>(data.data()))},
			  m_end{m_begin + data.size_bytes()}
		{
			setg(m_begin, m_begin, m_end);
		}

		auto seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) -> pos_type override
		{
			assert(off <= std::numeric_limits<i32>::max());

			switch (dir)
			{
			case std::ios_base::cur:
				gbump(static_cast<i32>(off));
				break;
			case std::ios_base::end:
				setg(m_begin, m_end + off, m_end);
				break;
			case std::ios_base::beg:
				setg(m_begin, m_begin + off, m_end);
				break;
			default:
				break;
			}

			return gptr() - eback();
		}

		auto seekpos(std::streampos pos, std::ios_base::openmode mode) -> pos_type override
		{
			return seekoff(pos, std::ios_base::beg, mode);
		}

	private:
		char *m_begin;
		char *m_end;
	};

	class MemoryIstream : public std::istream
	{
	public:
		explicit MemoryIstream(std::span<const std::byte> data)
			: m_buf{data},
			  std::istream{&m_buf} {}

	private:
		MemoryBuffer m_buf;
	};
}
