/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../types.h"

#include <string>
#include <optional>

namespace polaris::util
{
	[[nodiscard]] inline auto tryParseDigit(char c) -> std::optional<u32>
	{
		if (c >= '0' && c <= '9')
			return static_cast<u32>(c - '0');
		else return {};
	}

	[[nodiscard]] inline auto tryParseU32(const std::string &value, u32 radix = 10) -> std::optional<u32>
	{
		try
		{
			return std::stoul(value, nullptr, static_cast<i32>(radix));
		}
		catch (...)
		{
			return {};
		}
	}

	inline auto tryParseU32(u32 &dst, const std::string &value, u32 radix = 10)
	{
		if (const auto parsed = tryParseU32(value, radix))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}

	[[nodiscard]] inline auto tryParseI32(const std::string &value, u32 radix = 10) -> std::optional<i32>
	{
		try
		{
			return std::stol(value, nullptr, static_cast<i32>(radix));
		}
		catch (...)
		{
			return {};
		}
	}

	inline auto tryParseI32(i32 &dst, const std::string &value, u32 radix = 10)
	{
		if (const auto parsed = tryParseI32(value, radix))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}

	[[nodiscard]] inline auto tryParseU64(const std::string &value, u32 radix = 10) -> std::optional<u64>
	{
		try
		{
			return std::stoull(value, nullptr, static_cast<i32>(radix));
		}
		catch (...)
		{
			return {};
		}
	}

	inline auto tryParseU64(u64 &dst, const std::string &value, u32 radix = 10)
	{
		if (const auto parsed = tryParseU64(value, radix))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}

	[[nodiscard]] inline auto tryParseI64(const std::string &value, u32 radix = 10) -> std::optional<i64>
	{
		try
		{
			return std::stoll(value, nullptr, static_cast<i32>(radix));
		}
		catch (...)
		{
			return {};
		}
	}

	inline auto tryParseI64(i64 &dst, const std::string &value, u32 radix = 10)
	{
		if (const auto parsed = tryParseI64(value, radix))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}

	[[nodiscard]] inline auto tryParseSize(const std::string &value, u32 radix = 10) -> std::optional<usize>
	{
		if constexpr (sizeof(usize) == 8)
			return tryParseU64(value, radix);
		else return tryParseU32(value, radix);
	}

	inline auto tryParseSize(usize &dst, const std::string &value, u32 radix = 10)
	{
		if (const auto parsed = tryParseSize(value, radix))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}

	[[nodiscard]] inline auto tryParseBool(const std::string &value) -> std::optional<bool>
	{
		if (value == "true")
			return true;
		else if (value == "false")
			return false;
		else return {};
	}

	inline auto tryParseBool(bool &dst, const std::string &value)
	{
		if (const auto parsed = tryParseBool(value))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}

	[[nodiscard]] inline auto tryParseF32(const std::string &value) -> std::optional<f32>
	{
		try
		{
			return std::stof(value, nullptr);
		}
		catch (...)
		{
			return {};
		}
	}

	inline auto tryParseF32(f32 &dst, const std::string &value)
	{
		if (const auto parsed = tryParseF32(value))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}

	[[nodiscard]] inline auto tryParseF64(const std::string &value) -> std::optional<f64>
	{
		try
		{
			return std::stod(value, nullptr);
		}
		catch (...)
		{
			return {};
		}
	}

	inline auto tryParseF64(f64 &dst, const std::string &value)
	{
		if (const auto parsed = tryParseF64(value))
		{
			dst = *parsed;
			return true;
		}
		else return false;
	}
}
