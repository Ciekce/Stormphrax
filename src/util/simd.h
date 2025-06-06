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

#include "../types.h"

#include <cassert>

#include "../arch.h"
#include "align.h"
#include "aligned_array.h"

#if SP_HAS_AVX512
#include "simd/avx512.h"
#elif SP_HAS_AVX2
#include "simd/avx2.h"
#elif SP_HAS_NEON
#include "simd/neon.h"
#else
#error No supported SIMD extension found
#endif

#define SP_SIMD_ALIGNAS alignas(stormphrax::util::simd::Alignment)

namespace stormphrax::util::simd
{
	template <typename T, usize N>
	using Array = AlignedArray<Alignment, T, N>;

	template <typename T = void>
	auto isAligned(const T *ptr)
	{
		return util::isAligned<Alignment>(ptr);
	}

	template <typename T>
	struct VectorImpl {};

	template <>
	struct VectorImpl<u8>
	{
		using Type = VectorU8;
	};

	template <>
	struct VectorImpl<i8>
	{
		using Type = VectorI8;
	};

	template <>
	struct VectorImpl<i16>
	{
		using Type = VectorI16;
	};

	template <>
	struct VectorImpl<i32>
	{
		using Type = VectorI32;
	};

	template <typename T>
	using Vector = typename VectorImpl<T>::Type;
	template <typename T>
	struct PromotedVectorImpl {};

	template <>
	struct PromotedVectorImpl<i8>
	{
		using Type = VectorI16;
	};

	template <>
	struct PromotedVectorImpl<i16>
	{
		using Type = VectorI32;
	};

	template <typename T>
	using PromotedVector = typename PromotedVectorImpl<T>::Type;

	template <typename T>
	using Vector = typename VectorImpl<T>::Type;

	template <typename T>
	struct PackedVectorImpl {};

	template <>
	struct PackedVectorImpl<i16>
	{
		using Type = VectorU8;
	};

	template <>
	struct PackedVectorImpl<i32>
	{
		using Type = VectorI16;
	};

	template <typename T>
	using PackedVector = typename PromotedVectorImpl<T>::Type;

	template <typename T>
	constexpr auto ChunkSize = sizeof(Vector<T>) / sizeof(T);

#define SP_SIMD_OP_0(Name) \
	template <typename T> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name() = delete; \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i8>() \
	{ \
		return impl::Name##I8(); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i16>() \
	{ \
		return impl::Name##I16(); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i32>() \
	{ \
		return impl::Name##I32(); \
	}

#define SP_SIMD_OP_1_VALUE(Name, Arg0) \
	template <typename T> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name(T Arg0) = delete; \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i8>(i8 Arg0) \
	{ \
		return impl::Name##I8(Arg0); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i16>(i16 Arg0) \
	{ \
		return impl::Name##I16(Arg0); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i32>(i32 Arg0) \
	{ \
		return impl::Name##I32(Arg0); \
	}

#define SP_SIMD_OP_2_VECTORS(Name, Arg0, Arg1) \
	template <typename T> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name(Vector<T> Arg0, Vector<T> Arg1) = delete; \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i8>(Vector<i8> Arg0, Vector<i8> Arg1) \
	{ \
		return impl::Name##I8(Arg0, Arg1); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i16>(Vector<i16> Arg0, Vector<i16> Arg1) \
	{ \
		return impl::Name##I16(Arg0, Arg1); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i32>(Vector<i32> Arg0, Vector<i32> Arg1) \
	{ \
		return impl::Name##I32(Arg0, Arg1); \
	}

#define SP_SIMD_OP_3_VECTORS(Name, Arg0, Arg1, Arg2) \
	template <typename T> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name(Vector<T> Arg0, Vector<T> Arg1, Vector<T> Arg2) = delete; \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i8>(Vector<i8> Arg0, Vector<i8> Arg1, Vector<i8> Arg2) \
	{ \
		return impl::Name##I8(Arg0, Arg1, Arg2); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i16>(Vector<i16> Arg0, Vector<i16> Arg1, Vector<i16> Arg2) \
	{ \
		return impl::Name##I16(Arg0, Arg1, Arg2); \
	} \
	template <> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name<i32>(Vector<i32> Arg0, Vector<i32> Arg1, Vector<i32> Arg2) \
	{ \
		return impl::Name##I32(Arg0, Arg1, Arg2); \
	}

SP_SIMD_OP_0(zero)
SP_SIMD_OP_1_VALUE(set1, v)
SP_SIMD_OP_2_VECTORS(add, a, b)
SP_SIMD_OP_2_VECTORS(sub, a, b)
SP_SIMD_OP_2_VECTORS(min, a, b)
SP_SIMD_OP_2_VECTORS(max, a, b)
SP_SIMD_OP_3_VECTORS(clamp, v, min, max)

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto load(const void *ptr) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto load<i8>(const void *ptr)
	{
		return impl::loadI8(ptr);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto load<i16>(const void *ptr)
	{
		return impl::loadI16(ptr);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto load<i32>(const void *ptr)
	{
		return impl::loadI32(ptr);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto store(void *ptr, Vector<T> v) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto store<u8>(void *ptr, Vector<u8> v)
	{
		impl::storeU8(ptr, v);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto store<i8>(void *ptr, Vector<i8> v)
	{
		impl::storeI8(ptr, v);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto store<i16>(void *ptr, Vector<i16> v)
	{
		impl::storeI16(ptr, v);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto store<i32>(void *ptr, Vector<i32> v)
	{
		impl::storeI32(ptr, v);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeft(Vector<T> v, i32 shift) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeft<i8>(Vector<i8> v, i32 shift)
	{
		return impl::shiftLeftI8(v, shift);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeft<i16>(Vector<i16> v, i32 shift)
	{
		return impl::shiftLeftI16(v, shift);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeft<i32>(Vector<i32> v, i32 shift)
	{
		return impl::shiftLeftI32(v, shift);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftRight(Vector<T> v, i32 shift) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftRight<i16>(Vector<i16> v, i32 shift)
	{
		return impl::shiftRightI16(v, shift);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftRight<i32>(Vector<i32> v, i32 shift)
	{
		return impl::shiftRightI32(v, shift);
	}

	template <typename T, i32 Shift>
	SP_ALWAYS_INLINE_NDEBUG inline auto shift(Vector<T> v)
	{
		if constexpr (Shift > 0)
			return shiftLeft<T>(v, Shift);
		else if constexpr (Shift < 0)
			return shiftRight<T>(v, -Shift);
		else return v;
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdj(Vector<T> a, Vector<T> b) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdj<i16>(Vector<i16> a, Vector<i16> b)
	{
		return impl::mulAddAdjI16(a, b);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulLo(Vector<T> a, Vector<T> b) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulLo<i16>(Vector<i16> a, Vector<i16> b)
	{
		return impl::mulLoI16(a, b);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulLo<i32>(Vector<i32> a, Vector<i32> b)
	{
		return impl::mulLoI32(a, b);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftMulHi(Vector<T> a, Vector<T> b, i32 shift) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto shiftLeftMulHi<i16>(Vector<i16> a, Vector<i16> b, i32 shift)
	{
		return impl::shiftLeftMulHiI16(a, b, shift);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto packUnsigned(Vector<T> a, Vector<T> b) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto packUnsigned<i16>(Vector<i16> a, Vector<i16> b)
	{
		return impl::packUnsignedI16(a, b);
	}
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto packUnsigned<i32>(Vector<i32> a, Vector<i32> b)
	{
		return impl::packUnsignedI32(a, b);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAcc(PromotedVector<T> sum, Vector<T> a, Vector<T> b) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAcc<i16>(PromotedVector<i16> sum, Vector<i16> a, Vector<i16> b)
	{
		return impl::mulAddAdjAccI16(sum, a, b);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto hsum(Vector<T> v) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto hsum<i32>(Vector<i32> v)
	{
		return impl::hsumI32(v);
	}

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto dpbusd(Vector<T> sum, Vector<u8> u, Vector<i8> i) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto dpbusd<i32>(Vector<i32> sum, Vector<u8> u, Vector<i8> i)
	{
		return impl::dpbusdI32(sum, u, i);
	}

#undef SP_SIMD_OP_0
#undef SP_SIMD_OP_1_VALUE
#undef SP_SIMD_OP_2_VECTORS
#undef SP_SIMD_OP_3_VECTORS
}
