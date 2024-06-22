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

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <concepts>

#include "../../util/simd.h"

namespace stormphrax::eval::nnue::activation
{
	template <typename T>
	concept Activation = requires(T t)
	{
		{ T::Id } -> std::same_as<const u8 &>;
		{ T::activateDotAccumulate(util::simd::zero<typename T::OutputType>(),
		        util::simd::zero<typename T::InputType>(),
	            util::simd::zero<typename T::InputType>()) }
			-> std::same_as<util::simd::Vector<typename T::OutputType>>;
		{ T::  output(typename T::OutputType{}) }
			-> std::same_as<typename T::OutputType>;
	};

	template <typename T, typename Output>
	struct [[maybe_unused]] Identity
	{
		using InputType = T;
		using InputVector = util::simd::Vector<InputType>;

		using OutputType = Output;
		using OutputVector = util::simd::Vector<OutputType>;

		static constexpr u8 Id = 3;

		SP_ALWAYS_INLINE_NDEBUG static inline auto activateDotAccumulate(OutputVector sum,
			InputVector inputs, InputVector weights)
		{
			using namespace util::simd;

			return mulAddAdjAcc<InputType>(sum, inputs, weights);
		}

		SP_ALWAYS_INLINE_NDEBUG static inline auto output(OutputType value)
		{
			return value;
		}
	};

	template <typename T, typename Output>
	struct [[maybe_unused]] ReLU
	{
		using InputType = T;
		using InputVector = util::simd::Vector<InputType>;

		using OutputType = Output;
		using OutputVector = util::simd::Vector<OutputType>;

		static constexpr u8 Id = 2;

		SP_ALWAYS_INLINE_NDEBUG static inline auto activateDotAccumulate(OutputVector sum,
			InputVector inputs, InputVector weights)
		{
			using namespace util::simd;

			const auto activated = max<InputType>(inputs, zero<InputType>());
			return mulAddAdjAcc<InputType>(sum, activated, weights);
		}

		SP_ALWAYS_INLINE_NDEBUG static inline auto output(OutputType value)
		{
			return value;
		}
	};

	template <typename T, typename Output, T Max>
	struct [[maybe_unused]] ClippedReLU
	{
		using InputType = T;
		using InputVector = util::simd::Vector<InputType>;

		using OutputType = Output;
		using OutputVector = util::simd::Vector<OutputType>;

		static constexpr u8 Id = 0;

		SP_ALWAYS_INLINE_NDEBUG static inline auto activateDotAccumulate(OutputVector sum,
			InputVector inputs, InputVector weights)
		{
			using namespace util::simd;

			static const auto max = set1(Max);

			const auto activated = clamp<InputType>(inputs, zero<InputType>(), max);
			return mulAddAdjAcc<InputType>(sum, activated, weights);
		}

		SP_ALWAYS_INLINE_NDEBUG static inline auto output(OutputType value)
		{
			return value;
		}
	};

	template <typename T, typename Output, T Max>
	struct [[maybe_unused]] SquaredClippedReLU
	{
		using InputType = T;
		using InputVector = util::simd::Vector<InputType>;

		using OutputType = Output;
		using OutputVector = util::simd::Vector<OutputType>;

		static constexpr u8 Id = 1;

		SP_ALWAYS_INLINE_NDEBUG static inline auto activateDotAccumulate(OutputVector sum,
			InputVector inputs, InputVector weights)
		{
			using namespace util::simd;

			static const auto max = set1(Max);

			const auto clipped = util::simd::clamp<InputType>(inputs, zero<InputType>(), max);
			const auto crelu = mul<InputType>(clipped, weights);
			return mulAddAdjAcc<InputType>(sum, crelu, clipped);
		}

		SP_ALWAYS_INLINE_NDEBUG static inline auto output(OutputType value)
		{
			return value / static_cast<OutputType>(Max);
		}
	};
}
