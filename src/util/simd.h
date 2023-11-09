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

#include "../types.h"

#include "../arch.h"

#if SP_HAS_AVX512
	#define SP_SIMD_ALIGNMENT 64
#elif SP_HAS_AVX2
	#define SP_SIMD_ALIGNMENT 32
#else // sse 4.1, neon
	#define SP_SIMD_ALIGNMENT 16
#endif

#define SP_SIMD_ALIGNAS alignas(SP_SIMD_ALIGNMENT)
