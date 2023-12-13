/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2023 Ciekce
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

namespace stormphrax::eval::nnue::activation
{
	template <typename T>
	concept Activation = requires(T t)
	{
		{ T::Id } -> std::same_as<const u8 &>;
		{ T::activate(typename T::Type{}) } -> std::same_as<typename T::Type>;
		{ T::  output(typename T::Type{}) } -> std::same_as<typename T::Type>;
	};

	template <typename T>
	struct [[maybe_unused]] Identity
	{
		using Type = T;

		static constexpr u8 Id = 3;

		static inline auto activate(Type value)
		{
			return value;
		}

		static inline auto output(Type value)
		{
			return value;
		}
	};

	template <typename T>
	struct [[maybe_unused]] ReLU
	{
		using Type = T;

		static constexpr u8 Id = 2;

		static inline auto activate(Type value)
		{
			return std::max(value, Type{0});
		}

		static inline auto output(Type value)
		{
			return value;
		}
	};

	template <typename T, T Max>
	struct [[maybe_unused]] ClippedReLU
	{
		using Type = T;

		static constexpr u8 Id = 0;

		static inline auto activate(Type value)
		{
			return std::clamp(value, Type{0}, Max);
		}

		static inline auto output(Type value)
		{
			return value;
		}
	};

	template <typename T, T Max>
	struct [[maybe_unused]] SquaredClippedReLU
	{
		using Type = T;

		static constexpr u8 Id = 1;

		static inline auto activate(Type value)
		{
			const auto clipped = std::clamp(value, Type{0}, Max);
			return clipped * clipped;
		}

		static inline auto output(Type value)
		{
			return value / Max;
		}
	};

	template <std::floating_point T>
	struct [[maybe_unused]] Sigmoid
	{
		using Type = T;

		static constexpr u8 Id = 4;

		static inline auto activate(Type value)
		{
			return Type{1} / (Type{1} + std::exp(-value));
		}

		static inline auto output(Type value)
		{
			return value;
		}
	};

	template <std::floating_point T>
	struct [[maybe_unused]] Tanh
	{
		using Type = T;

		static constexpr u8 Id = 5;

		static inline auto activate(Type value)
		{
			return std::tanh(value);
		}

		static inline auto output(Type value)
		{
			return value;
		}
	};
}
