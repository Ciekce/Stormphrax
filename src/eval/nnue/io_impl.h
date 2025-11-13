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

#include <cassert>
#include <istream>
#include <memory>
#include <ostream>
#include <variant>
#include <vector>

#include "../../3rdparty/zstd/zstd.h"
#include "../../util/cemath.h"
#include "../../util/memstream.h"
#include "io.h"

namespace stormphrax::eval::nnue {
    template <usize kBlockSize>
    class PaddedParamStream final : public IParamStream {
    public:
        explicit PaddedParamStream(std::istream& in) :
                m_stream{&in} {}
        explicit PaddedParamStream(std::ostream& out) :
                m_stream{&out} {}

        ~PaddedParamStream() final = default;

    protected:
        inline bool readI8s(std::span<i8> dst) final {
            return read(reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes());
        }

        inline bool writeI8s(std::span<const i8> src) final {
            return write(reinterpret_cast<const std::byte*>(src.data()), src.size_bytes());
        }

        inline bool readI16s(std::span<i16> dst) final {
            return read(reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes());
        }

        inline bool writeI16s(std::span<const i16> src) final {
            return write(reinterpret_cast<const std::byte*>(src.data()), src.size_bytes());
        }

        inline bool readI32s(std::span<i32> dst) final {
            return read(reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes());
        }

        inline bool writeI32s(std::span<const i32> src) final {
            return write(reinterpret_cast<const std::byte*>(src.data()), src.size_bytes());
        }

    private:
        std::variant<std::istream*, std::ostream*> m_stream;

        inline bool read(std::byte* dst, usize n) {
            if (!std::holds_alternative<std::istream*>(m_stream)) {
                assert(false);
                return false;
            }

            auto& stream = *std::get<std::istream*>(m_stream);

            const auto padding = calcPadding(n);

            stream.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(n));
            stream.ignore(static_cast<std::streamsize>(padding));

            return !stream.fail();
        }

        inline bool write(const std::byte* src, usize n) {
            if (!std::holds_alternative<std::ostream*>(m_stream)) {
                assert(false);
                return false;
            }

            static constexpr std::array<std::byte, kBlockSize> Empty{};

            auto& stream = *std::get<std::ostream*>(m_stream);

            const auto padding = calcPadding(n);

            stream.write(reinterpret_cast<const char*>(src), static_cast<std::streamsize>(n));
            stream.write(reinterpret_cast<const char*>(Empty.data()), padding);

            return !stream.fail();
        }

        [[nodiscard]] static constexpr usize calcPadding(usize v) {
            return v - util::pad<kBlockSize>(v);
        }
    };

    class ZstdParamStream final : public IParamStream {
    public:
        explicit ZstdParamStream(std::istream& in);

        ~ZstdParamStream() final;

    protected:
        inline bool readI8s(std::span<i8> dst) final {
            return read(reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes());
        }

        inline bool writeI8s([[maybe_unused]] std::span<const i8> src) final {
            eprintln("ZstdParamStream::writeI8s");
            std::terminate();
        }

        inline bool readI16s(std::span<i16> dst) final {
            return read(reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes());
        }

        inline bool writeI16s([[maybe_unused]] std::span<const i16> src) final {
            eprintln("ZstdParamStream::writeI16s");
            std::terminate();
        }

        inline bool readI32s(std::span<i32> dst) final {
            return read(reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes());
        }

        inline bool writeI32s([[maybe_unused]] std::span<const i32> src) final {
            eprintln("ZstdParamStream::writeI32s");
            std::terminate();
        }

    private:
        std::istream& m_stream;

        std::vector<std::byte> m_inBuf{};
        std::vector<std::byte> m_outBuf{};

        usize m_pos{};
        usize m_end{};

        usize m_result{};

        ZSTD_DStream* m_dStream;

        ZSTD_inBuffer m_input{};
        ZSTD_outBuffer m_output{};

        bool m_fail{false};

        bool fillBuffer();
        bool read(std::byte* dst, usize n);
    };
} // namespace stormphrax::eval::nnue
