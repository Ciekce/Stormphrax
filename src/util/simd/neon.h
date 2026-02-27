/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "../../arch.h"
#include "../align.h"

#if SP_HAS_NEON

    #include <arm_neon.h>

namespace stormphrax::util::simd {
    using VectorU8 = uint8x16_t;
    using VectorU16 = uint16x8_t;

    using VectorI8 = int8x16_t;
    using VectorI16 = int16x8_t;
    using VectorI32 = int32x4_t;

    constexpr std::uintptr_t kAlignment = sizeof(int16x8_t);

    constexpr bool kPackNonSequential = false;

    constexpr usize kPackGrouping = 1;
    constexpr std::array kPackOrdering = {0};

    namespace impl {
        // ================================ u8 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorU8 zeroU8() {
            return vdupq_n_u8(0);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorU8 loadU8(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return vld1q_u8(static_cast<const u8*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeU8(void* ptr, VectorU8 v) {
            assert(isAligned<kAlignment>(ptr));
            vst1q_u8(static_cast<u8*>(ptr), v);
        }

        // ================================ u16 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorU16 zeroU16() {
            return vdupq_n_u16(0);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorU16 loadU16(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return vld1q_u16(static_cast<const u16*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeU16(void* ptr, VectorU16 v) {
            assert(isAligned<kAlignment>(ptr));
            vst1q_u16(static_cast<u16*>(ptr), v);
        }

        // ================================ i8 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 zeroI8() {
            return vdupq_n_s8(0);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 set1I8(i8 v) {
            return vdupq_n_s8(v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 loadI8(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return vld1q_s8(static_cast<const i8*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeI8(void* ptr, VectorI8 v) {
            assert(isAligned<kAlignment>(ptr));
            vst1q_s8(static_cast<i8*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 minI8(VectorI8 a, VectorI8 b) {
            return vminq_s8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 maxI8(VectorI8 a, VectorI8 b) {
            return vmaxq_s8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 clampI8(VectorI8 v, VectorI8 min, VectorI8 max) {
            return minI8(maxI8(v, min), max);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 addI8(VectorI8 a, VectorI8 b) {
            return vaddq_s8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 subI8(VectorI8 a, VectorI8 b) {
            return vsubq_s8(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI8 shiftLeftI8(VectorI8 v, i32 shift) {
            return vshlq_s8(v, vdupq_n_s8(static_cast<u8>(shift)));
        }

        // ================================ i16 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 zeroI16() {
            return vdupq_n_s16(0);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 set1I16(i16 v) {
            return vdupq_n_s16(v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 loadI16(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return vld1q_s16(static_cast<const i16*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeI16(void* ptr, VectorI16 v) {
            assert(isAligned<kAlignment>(ptr));
            vst1q_s16(static_cast<i16*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 minI16(VectorI16 a, VectorI16 b) {
            return vminq_s16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 maxI16(VectorI16 a, VectorI16 b) {
            return vmaxq_s16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 clampI16(VectorI16 v, VectorI16 min, VectorI16 max) {
            return minI16(maxI16(v, min), max);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 addI16(VectorI16 a, VectorI16 b) {
            return vaddq_s16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 subI16(VectorI16 a, VectorI16 b) {
            return vsubq_s16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 mulLoI16(VectorI16 a, VectorI16 b) {
            return vmulq_s16(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 shiftLeftI16(VectorI16 v, i32 shift) {
            return vshlq_s16(v, vdupq_n_s16(static_cast<i16>(shift)));
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 shiftRightI16(VectorI16 v, i32 shift) {
            return shiftLeftI16(v, -shift);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI16 shiftLeftMulHiI16(VectorI16 a, VectorI16 b, i32 shift) {
            // the instruction used for mulhi here, VQDMULH, doubles the results.
            // this is effectively a shift by another bit, so shift by one less
            const auto shifted = vshlq_s16(a, vdupq_n_s16(static_cast<i16>(shift - 1)));
            return vqdmulhq_s16(shifted, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 mulAddAdjI16(VectorI16 a, VectorI16 b) {
            const auto low = vmull_s16(vget_low_s16(a), vget_low_s16(b));
            const auto high = vmull_high_s16(a, b);
            return vpaddq_s32(low, high);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorU8 packUnsignedI16(VectorI16 a, VectorI16 b) {
            const auto high = vqmovun_s16(a);
            const auto low = vqmovun_s16(b);
            return vcombine_u8(high, low);
        }

        // ================================ i32 ================================

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 zeroI32() {
            return vdupq_n_s32(0);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 set1I32(i32 v) {
            return vdupq_n_s32(v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 loadI32(const void* ptr) {
            assert(isAligned<kAlignment>(ptr));
            return vld1q_s32(static_cast<const i32*>(ptr));
        }

        SP_ALWAYS_INLINE_NDEBUG inline void storeI32(void* ptr, VectorI32 v) {
            assert(isAligned<kAlignment>(ptr));
            vst1q_s32(static_cast<i32*>(ptr), v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 minI32(VectorI32 a, VectorI32 b) {
            return vminq_s32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 maxI32(VectorI32 a, VectorI32 b) {
            return vmaxq_s32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 clampI32(VectorI32 v, VectorI32 min, VectorI32 max) {
            return minI32(maxI32(v, min), max);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 addI32(VectorI32 a, VectorI32 b) {
            return vaddq_s32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 subI32(VectorI32 a, VectorI32 b) {
            return vsubq_s32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 mulLoI32(VectorI32 a, VectorI32 b) {
            return vmulq_s32(a, b);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 shiftLeftI32(VectorI32 v, i32 shift) {
            return vshlq_s32(v, vdupq_n_s32(static_cast<i32>(shift)));
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 shiftRightI32(VectorI32 v, i32 shift) {
            return shiftLeftI32(v, -shift);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorU16 packUnsignedI32(VectorI32 a, VectorI32 b) {
            const auto high = vqmovun_s32(a);
            const auto low = vqmovun_s32(b);
            return vcombine_u16(high, low);
        }

        SP_ALWAYS_INLINE_NDEBUG inline i32 hsumI32(VectorI32 v) {
            return vaddvq_s32(v);
        }

        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 dpbusdI32(VectorI32 sum, VectorU8 u, VectorI8 i) {
            const auto i0 = vreinterpretq_u8_s8(u);

    #if SP_HAS_NEON_DOTPROD
            return vdotq_s32(sum, i0, i);
    #else
            const auto low = vmull_s8(vget_low_s8(i0), vget_low_s8(i));
            const auto high = vmull_high_s8(i0, i);
            const auto p = vpaddq_s16(low, high);
            return vpadalq_s16(sum, p);
    #endif
        }

        SP_ALWAYS_INLINE_NDEBUG inline u32 nonzeroMaskU8(VectorU8 v) {
            alignas(kAlignment) static constexpr std::array<u32, 4> kMask = {1, 2, 4, 8};
            return vaddvq_u32(vandq_u32(vtstq_u32(v, v), vld1q_u32(kMask.data())));
        }

        // Depends on addI32
        SP_ALWAYS_INLINE_NDEBUG inline VectorI32 mulAddAdjAccI16(VectorI32 sum, VectorI16 a, VectorI16 b) {
            const auto products = mulAddAdjI16(a, b);
            return addI32(sum, products);
        }
    } // namespace impl
} // namespace stormphrax::util::simd

#endif
