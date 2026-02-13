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

#include "types.h"

#include <new>

#if defined(SP_NATIVE)
    // cannot expand a macro to defined()
    #if __BMI2__ && defined(SP_FAST_PEXT)
        #define SP_HAS_BMI2 1
    #else
        #define SP_HAS_BMI2 0
    #endif
    #if !defined(SP_DISABLE_AVX512)
        #define SP_HAS_VNNI512 __AVX512VNNI__
        #define SP_HAS_VBMI2 __AVX512VBMI2__
        #define SP_HAS_AVX512VL __AVX512VL__
        #define SP_HAS_AVX512 (__AVX512F__ && (__AVX512BW__ || __AVX512VNNI__))
    #else
        #define SP_HAS_VNNI512 0
        #define SP_HAS_VBMI2 0
        #define SP_HAS_AVX512VL 0
        #define SP_HAS_AVX512 0
    #endif
    #define SP_HAS_VNNI256 0 // slowdown on any cpu that would use it
    #define SP_HAS_AVX2 __AVX2__
    #define SP_HAS_POPCNT __POPCNT__
    #define SP_HAS_NEON __ARM_NEON
    #if !defined(SP_DISABLE_NEON_DOTPROD)
        #define SP_HAS_NEON_DOTPROD (__ARM_ARCH >= 8)
    #else
        #define SP_HAS_NEON_DOTPROD 0
    #endif
#elif defined(SP_VNNI512)
    #define SP_HAS_BMI2 1
    #define SP_HAS_VNNI512 1
    #define SP_HAS_VBMI2 0
    #define SP_HAS_AVX512VL 0
    #define SP_HAS_AVX512 1
    #define SP_HAS_VNNI256 1
    #define SP_HAS_AVX2 1
    #define SP_HAS_POPCNT 1
    #define SP_HAS_NEON 0
    #define SP_HAS_NEON_DOTPROD 0
#elif defined(SP_AVX512)
    #define SP_HAS_BMI2 1
    #define SP_HAS_VNNI512 0
    #define SP_HAS_VBMI2 0
    #define SP_HAS_AVX512VL 0
    #define SP_HAS_AVX512 1
    #define SP_HAS_VNNI256 0
    #define SP_HAS_AVX2 1
    #define SP_HAS_POPCNT 1
    #define SP_HAS_NEON 0
    #define SP_HAS_NEON_DOTPROD 0
#elif defined(SP_AVX2_BMI2)
    #define SP_HAS_BMI2 1
    #define SP_HAS_VNNI512 0
    #define SP_HAS_VBMI2 0
    #define SP_HAS_AVX512VL 0
    #define SP_HAS_AVX512 0
    #define SP_HAS_VNNI256 0
    #define SP_HAS_AVX2 1
    #define SP_HAS_POPCNT 1
    #define SP_HAS_NEON 0
    #define SP_HAS_NEON_DOTPROD 0
#elif defined(SP_AVX2)
    #define SP_HAS_BMI2 0
    #define SP_HAS_VNNI512 0
    #define SP_HAS_VBMI2 0
    #define SP_HAS_AVX512VL 0
    #define SP_HAS_AVX512 0
    #define SP_HAS_VNNI256 0
    #define SP_HAS_AVX2 1
    #define SP_HAS_POPCNT 1
    #define SP_HAS_NEON 0
    #define SP_HAS_NEON_DOTPROD 0
#else
    #error no arch specified
#endif

namespace stormphrax {
#ifdef __cpp_lib_hardware_interference_size
    constexpr auto kCacheLineSize = std::hardware_destructive_interference_size;
#else
    constexpr usize kCacheLineSize = 64;
#endif
} // namespace stormphrax
