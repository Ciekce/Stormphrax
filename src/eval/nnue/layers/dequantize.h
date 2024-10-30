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

#include "../../../types.h"

#include <span>

#include "../../../position/boards.h"
#include "../io.h"

namespace stormphrax::eval::nnue::layers {
    template<typename Input, typename Output, u32 Count, Output Q>
    struct Dequantize {
        using InputType = Input;
        using OutputType = Output;

        static constexpr auto InputCount = Count;
        static constexpr auto OutputCount = Count;

        inline auto forward(
            [[maybe_unused]] const BitboardSet& bbs,
            std::span<const InputType, Count> inputs,
            std::span<OutputType, Count> outputs
        ) const {
            for (u32 i = 0; i < Count; ++i) {
                outputs[i] = static_cast<OutputType>(inputs[i]) / Q;
            }
        }

        inline auto readFrom([[maybe_unused]] IParamStream& stream) -> bool {
            return true;
        }

        inline auto writeTo([[maybe_unused]] IParamStream& stream) const -> bool {
            return true;
        }
    };
} // namespace stormphrax::eval::nnue::layers
