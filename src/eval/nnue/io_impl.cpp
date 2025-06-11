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

#include "io_impl.h"

#include <cstring>

#include "../../3rdparty/zstd/zstd_errors.h"

namespace stormphrax::eval::nnue {
    ZstdParamStream::ZstdParamStream(std::istream& in) :
            m_stream{in} {
        m_inBuf.resize(ZSTD_DStreamInSize());
        m_outBuf.resize(ZSTD_DStreamOutSize());

        m_dStream = ZSTD_createDStream();

        m_input.src = m_inBuf.data();
        m_output.dst = m_outBuf.data();
    }

    ZstdParamStream::~ZstdParamStream() {
        ZSTD_freeDStream(m_dStream);
    }

    auto ZstdParamStream::fillBuffer() -> bool {
        while (m_pos >= m_end && (m_input.pos < m_input.size || !m_stream.fail())) {
            if (m_input.pos == m_input.size) {
                if (!m_stream) {
                    m_fail = true;
                    return false;
                }

                m_stream.read(reinterpret_cast<char*>(m_inBuf.data()), static_cast<std::streamsize>(m_inBuf.size()));

                m_input.size = m_stream.gcount();
                m_input.pos = 0;
            }

            if (m_result == 0 && m_input.pos < m_input.size)
                ZSTD_initDStream(m_dStream);

            m_output.size = m_outBuf.size();
            m_output.pos = 0;

            m_result = ZSTD_decompressStream(m_dStream, &m_output, &m_input);

            if (ZSTD_isError(m_result)) {
                const auto code = ZSTD_getErrorCode(m_result);
                std::cerr << "zstd error: " << ZSTD_getErrorString(code) << std::endl;

                m_fail = true;
                return false;
            }

            m_pos = 0;
            m_end = m_output.pos;
        }

        return m_pos < m_end;
    }

    auto ZstdParamStream::read(std::byte* dst, usize n) -> bool {
        if (m_fail)
            return false;

        while (n > 0) {
            if (m_pos >= m_end && !fillBuffer())
                return false;

            const auto remaining = m_end - m_pos;
            const auto count = std::min(n, remaining);

            std::memcpy(dst, &m_outBuf[m_pos], count);

            m_pos += count;
            dst += count;

            n -= count;
        }

        return true;
    }
} // namespace stormphrax::eval::nnue
