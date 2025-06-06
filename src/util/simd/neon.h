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

#include "../../arch.h"
#include "../align.h"

#if SP_HAS_NEON

#include <arm_neon.h>

namespace stormphrax::util::simd
{
	using VectorU8 = uint8x16_t;
	using VectorU16 = uint16x8_t;

	using VectorI8 = int8x16_t;
	using VectorI16 = int16x8_t;
	using VectorI32 = int32x4_t;

	constexpr std::uintptr_t Alignment = sizeof(int16x8_t);

	constexpr bool PackNonSequential = false;

	constexpr usize PackGrouping = 1;
	constexpr auto PackOrdering = std::array {
		0,
	};

	namespace impl
	{
		// ================================ u8 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroU8() -> VectorU8
		{
			return vdupq_n_u8(0);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadU8(const void *ptr) -> VectorU8
		{
			assert(isAligned<Alignment>(ptr));
			return vld1q_u8(static_cast<const u8 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeU8(void *ptr, VectorU8 v)
		{
			assert(isAligned<Alignment>(ptr));
			vst1q_u8(static_cast<u8 *>(ptr), v);
		}

		// ================================ u16 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroU16() -> VectorU16
		{
			return vdupq_n_u16(0);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadU16(const void *ptr) -> VectorU16
		{
			assert(isAligned<Alignment>(ptr));
			return vld1q_u16(static_cast<const u16 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeU16(void *ptr, VectorU16 v)
		{
			assert(isAligned<Alignment>(ptr));
			vst1q_u16(static_cast<u16 *>(ptr), v);
		}

		// ================================ i8 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI8() -> VectorI8
		{
			return vdupq_n_s8(0);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I8(i8 v) -> VectorI8
		{
			return vdupq_n_s8(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI8(const void *ptr) -> VectorI8
		{
			assert(isAligned<Alignment>(ptr));
			return vld1q_s8(static_cast<const i8 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI8(void *ptr, VectorI8 v)
		{
			assert(isAligned<Alignment>(ptr));
			vst1q_s8(static_cast<i8 *>(ptr), v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return vminq_s8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return vmaxq_s8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI8(
			VectorI8 v, VectorI8 min, VectorI8 max) -> VectorI8
		{
			return minI8(maxI8(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return vaddq_s8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI8(VectorI8 a, VectorI8 b) -> VectorI8
		{
			return vsubq_s8(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftI8(VectorI8 v, i32 shift) -> VectorI8
		{
			return vshlq_s8(v, vdupq_n_s8(static_cast<u8>(shift)));
		}

		// ================================ i16 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI16() -> VectorI16
		{
			return vdupq_n_s16(0);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I16(i16 v) -> VectorI16
		{
			return vdupq_n_s16(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI16(const void *ptr) -> VectorI16
		{
			assert(isAligned<Alignment>(ptr));
			return vld1q_s16(static_cast<const i16 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI16(void *ptr, VectorI16 v)
		{
			assert(isAligned<Alignment>(ptr));
			vst1q_s16(static_cast<i16 *>(ptr), v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return vminq_s16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return vmaxq_s16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI16(
			VectorI16 v, VectorI16 min, VectorI16 max) -> VectorI16
		{
			return minI16(maxI16(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return vaddq_s16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return vsubq_s16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulLoI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
			return vmulq_s16(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftI16(VectorI16 v, i32 shift) -> VectorI16
		{
			return vshlq_s16(v, vdupq_n_s16(static_cast<i16>(shift)));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftRightI16(VectorI16 v, i32 shift) -> VectorI16
		{
			return shiftLeftI16(v, -shift);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftMulHiI16(VectorI16 a, VectorI16 b, i32 shift) -> VectorI16
		{
			// the instruction used for mulhi here, VQDMULH, doubles the results.
			// this is effectively a shift by another bit, so shift by one less
			const auto shifted = vshlq_s16(a, vdupq_n_s16(static_cast<i16>(shift - 1)));
			return vqdmulhq_s16(shifted, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjI16(VectorI16 a, VectorI16 b) -> VectorI32
		{
			const auto low  = vmull_s16(vget_low_s16(a), vget_low_s16(b));
			const auto high = vmull_high_s16(a, b);
			return vpaddq_s32(low, high);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto packUnsignedI16(VectorI16 a, VectorI16 b) -> VectorU8
		{
			const auto high = vqmovun_s16(a);
			const auto low = vqmovun_s16(b);
			return vcombine_u8(high, low);
		}

		// ================================ i32 ================================

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI32() -> VectorI32
		{
			return vdupq_n_s32(0);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I32(i32 v) -> VectorI32
		{
			return vdupq_n_s32(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI32(const void *ptr) -> VectorI32
		{
			assert(isAligned<Alignment>(ptr));
			return vld1q_s32(static_cast<const i32 *>(ptr));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI32(void *ptr, VectorI32 v)
		{
			assert(isAligned<Alignment>(ptr));
			vst1q_s32(static_cast<i32 *>(ptr), v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return vminq_s32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return vmaxq_s32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI32(
			VectorI32 v, VectorI32 min, VectorI32 max) -> VectorI32
		{
			return minI32(maxI32(v, min), max);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return vaddq_s32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return vsubq_s32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulLoI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
			return vmulq_s32(a, b);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftI32(VectorI32 v, i32 shift) -> VectorI32
		{
			return vshlq_s32(v, vdupq_n_s32(static_cast<i32>(shift)));
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto shiftRightI32(VectorI32 v, i32 shift) -> VectorI32
		{
			return shiftLeftI32(v, -shift);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto packUnsignedI32(VectorI32 a, VectorI32 b) -> VectorU16
		{
			const auto high = vqmovun_s32(a);
			const auto low = vqmovun_s32(b);
			return vcombine_u16(high, low);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32(VectorI32 v) -> i32
		{
			return vaddvq_s32(v);
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto dpbusdI32(VectorI32 sum, VectorU8 u, VectorI8 i) -> VectorI32
		{
			const auto i0 = vreinterpretq_u8_s8(u);

#if SP_HAS_NEON_DOTPROD
			return vdotq_s32(sum, i0, i);
#else
			const auto low  = vmull_s8(vget_low_s8(i0), vget_low_s8(i));
			const auto high = vmull_high_s8(i0, i);
			const auto p = vpaddq_s16(low, high);
			return vpadalq_s16(sum, p);
#endif
		}

		// Depends on addI32
		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAccI16(VectorI32 sum, VectorI16 a, VectorI16 b) -> VectorI32
		{
			const auto products = mulAddAdjI16(a, b);
			return addI32(sum, products);
		}
	}
}

#endif
