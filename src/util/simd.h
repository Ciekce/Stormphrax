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

#include "../types.h"

#include <cassert>

#include "../arch.h"

#if SP_HAS_AVX512 || SP_HAS_AVX2 || SP_HAS_SSE41
#include <immintrin.h>
#else
#include <cmath>
#include <algorithm>
#endif

// Run

namespace stormphrax::util::simd
{
#if SP_HAS_AVX512
	using VectorI16 = __m512i;
	using VectorI32 = __m512i;
#elif SP_HAS_AVX2
	using VectorI16 = __m256i;
	using VectorI32 = __m256i;
#elif SP_HAS_SSE41
	using VectorI16 = __m128i;
	using VectorI32 = __m128i;
#else // TODO neon
	using VectorI16 = i16;
	using VectorI32 = i32;
#endif

#if SP_HAS_AVX512 || SP_HAS_AVX2 || SP_HAS_SSE41
	constexpr std::uintptr_t Alignment = sizeof(VectorI16);
#else // neon
	constexpr std::uintptr_t Alignment = 16;
#endif

	constexpr usize ChunkSize = sizeof(VectorI16) / sizeof(i16);

#define SP_SIMD_ALIGNAS alignas(stormphrax::util::simd::Alignment)

	template <std::uintptr_t Alignment = Alignment, typename T = void>
	auto isAligned(const T *ptr)
	{
		return (reinterpret_cast<std::uintptr_t>(ptr) % Alignment) == 0;
	}

	namespace impl
	{
		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI16() -> VectorI16
		{
#if SP_HAS_AVX512
			return _mm512_setzero_si512();
#elif SP_HAS_AVX2
			return _mm256_setzero_si256();
#elif SP_HAS_SSE41
			return _mm_setzero_si128();
#else
			return 0;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I16(i16 v) -> VectorI16
		{
#if SP_HAS_AVX512
			return _mm512_set1_epi16(v);
#elif SP_HAS_AVX2
			return _mm256_set1_epi16(v);
#elif SP_HAS_SSE41
			return _mm_set1_epi16(v);
#else
			return v;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI16(const void *ptr) -> VectorI16
		{
			assert(isAligned(ptr));

#if SP_HAS_AVX512
			return _mm512_load_si512(ptr);
#elif SP_HAS_AVX2
			return _mm256_load_si256(static_cast<const VectorI16 *>(ptr));
#elif SP_HAS_SSE41
			return _mm_load_si128(static_cast<const VectorI16 *>(ptr));
#else
			return *static_cast<const VectorI16 *>(ptr);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI16(void *ptr, VectorI16 v)
		{
			assert(isAligned(ptr));

#if SP_HAS_AVX512
			_mm512_store_si512(ptr, v);
#elif SP_HAS_AVX2
			_mm256_store_si256(static_cast<VectorI16 *>(ptr), v);
#elif SP_HAS_SSE41
			_mm_store_si128(static_cast<VectorI16 *>(ptr), v);
#else
			*static_cast<VectorI16 *>(ptr) = v;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
#if SP_HAS_AVX512
			return _mm512_min_epi16(a, b);
#elif SP_HAS_AVX2
			return _mm256_min_epi16(a, b);
#elif SP_HAS_SSE41
			return _mm_min_epi16(a, b);
#else
			return std::min(a, b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
#if SP_HAS_AVX512
			return _mm512_max_epi16(a, b);
#elif SP_HAS_AVX2
			return _mm256_max_epi16(a, b);
#elif SP_HAS_SSE41
			return _mm_max_epi16(a, b);
#else
			return std::max(a, b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI16(
			VectorI16 v, VectorI16 min, VectorI16 max) -> VectorI16
		{
#if SP_HAS_AVX512 || SP_HAS_AVX2 || SP_HAS_SSE41
			return minI16(maxI16(v, min), max);
#else
			return std::clamp(v, min, max);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
#if SP_HAS_AVX512
			return _mm512_add_epi16(a, b);
#elif SP_HAS_AVX2
			return _mm256_add_epi16(a, b);
#elif SP_HAS_SSE41
			return _mm_add_epi16(a, b);
#else
			return static_cast<VectorI16>(a + b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
#if SP_HAS_AVX512
			return _mm512_sub_epi16(a, b);
#elif SP_HAS_AVX2
			return _mm256_sub_epi16(a, b);
#elif SP_HAS_SSE41
			return _mm_sub_epi16(a, b);
#else
			return static_cast<VectorI16>(a - b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulI16(VectorI16 a, VectorI16 b) -> VectorI16
		{
#if SP_HAS_AVX512
			return _mm512_mullo_epi16(a, b);
#elif SP_HAS_AVX2
			return _mm256_mullo_epi16(a, b);
#elif SP_HAS_SSE41
			return _mm_mullo_epi16(a, b);
#else
			//TODO is this correct for overflow?
			return static_cast<VectorI16>(a * b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjI16(VectorI16 a, VectorI16 b) -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_madd_epi16(a, b);
#elif SP_HAS_AVX2
			return _mm256_madd_epi16(a, b);
#elif SP_HAS_SSE41
			return _mm_madd_epi16(a, b);
#else
			return static_cast<VectorI32>(a) * static_cast<VectorI32>(b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto zeroI32() -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_setzero_si512();
#elif SP_HAS_AVX2
			return _mm256_setzero_si256();
#elif SP_HAS_SSE41
			return _mm_setzero_si128();
#else
			return 0;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto set1I32(i32 v) -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_set1_epi32(v);
#elif SP_HAS_AVX2
			return _mm256_set1_epi32(v);
#elif SP_HAS_SSE41
			return _mm_set1_epi32(v);
#else
			return v;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto loadI32(const void *ptr) -> VectorI32
		{
			assert(isAligned(ptr));

#if SP_HAS_AVX512
			return _mm512_load_si512(ptr);
#elif SP_HAS_AVX2
			return _mm256_load_si256(static_cast<const VectorI16 *>(ptr));
#elif SP_HAS_SSE41
			return _mm_load_si128(static_cast<const VectorI16 *>(ptr));
#else
			return *static_cast<const VectorI32 *>(ptr);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto storeI32(void *ptr, VectorI32 v)
		{
			assert(isAligned(ptr));

#if SP_HAS_AVX512
			_mm512_store_si512(ptr, v);
#elif SP_HAS_AVX2
			_mm256_store_si256(static_cast<VectorI32 *>(ptr), v);
#elif SP_HAS_SSE41
			_mm_store_si128(static_cast<VectorI32 *>(ptr), v);
#else
			*static_cast<VectorI32 *>(ptr) = v;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto minI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_min_epi32(a, b);
#elif SP_HAS_AVX2
			return _mm256_min_epi32(a, b);
#elif SP_HAS_SSE41
			return _mm_min_epi32(a, b);
#else
			return std::min(a, b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto maxI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_max_epi32(a, b);
#elif SP_HAS_AVX2
			return _mm256_max_epi32(a, b);
#elif SP_HAS_SSE41
			return _mm_max_epi32(a, b);
#else
			return std::max(a, b);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto clampI32(
			VectorI32 v, VectorI32 min, VectorI32 max) -> VectorI32
		{
#if SP_HAS_AVX512 || SP_HAS_AVX2 || SP_HAS_SSE41
			return minI32(maxI32(v, min), max);
#else
			return std::clamp(v, min, max);
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto addI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_add_epi32(a, b);
#elif SP_HAS_AVX2
			return _mm256_add_epi32(a, b);
#elif SP_HAS_SSE41
			return _mm_add_epi32(a, b);
#else
			return a + b;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto subI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_sub_epi32(a, b);
#elif SP_HAS_AVX2
			return _mm256_sub_epi32(a, b);
#elif SP_HAS_SSE41
			return _mm_sub_epi32(a, b);
#else
			return a - b;
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto mulI32(VectorI32 a, VectorI32 b) -> VectorI32
		{
#if SP_HAS_AVX512
			return _mm512_mullo_epi32(a, b);
#elif SP_HAS_AVX2
			return _mm256_mullo_epi32(a, b);
#elif SP_HAS_SSE41
			return _mm_mullo_epi32(a, b);
#else
			return a * b;
#endif
		}

		namespace internal
		{
#if SP_HAS_SSE41
			SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32Sse41(__m128i v) -> i32
			{
				const auto high64 = _mm_unpackhi_epi64(v, v);
				const auto sum64 = _mm_add_epi32(v, high64);

				const auto high32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
				const auto sum32 = _mm_add_epi32(sum64, high32);

				return _mm_cvtsi128_si32(sum32);
			}
#endif

#if SP_HAS_AVX2
			SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32Avx2(__m256i v) -> i32
			{
				const auto high128 = _mm256_extracti128_si256(v, 1);
				const auto low128 = _mm256_castsi256_si128(v);

				const auto sum128 = _mm_add_epi32(high128, low128);

				return hsumI32Sse41(sum128);
			}
#endif

#if SP_HAS_AVX512
			SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32Avx512(__m512i v) -> i32
			{
				const auto high256 = _mm512_extracti64x4_epi64(v, 1);
				const auto low256 = _mm512_castsi512_si256(v);

				const auto sum256 = _mm256_add_epi32(high256, low256);

				return hsumI32Avx2(sum256);
			}
#endif
		}

		SP_ALWAYS_INLINE_NDEBUG inline auto hsumI32(VectorI32 v) -> i32
		{
#if SP_HAS_AVX512
			return internal::hsumI32Avx512(v);
#elif SP_HAS_AVX2
			return internal::hsumI32Avx2(v);
#elif SP_HAS_SSE41
			return internal::hsumI32Sse41(v);
#else
			return v;
#endif
		}

		// Depends on addI32
		SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAccI16(VectorI32 sum, VectorI16 a, VectorI32 b) -> VectorI16
		{
#if SP_HAS_VNNI512
			return _mm512_dpwssd_epi32(sum, a, b);
#else
			const auto products = mulAddAdjI16(a, b);
			return addI32(sum, products);
#endif
		}
	}

	template <typename T>
	struct VectorImpl {};

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

#define SP_SIMD_OP_0(Name) \
	template <typename T> \
	SP_ALWAYS_INLINE_NDEBUG inline auto Name() = delete; \
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
SP_SIMD_OP_2_VECTORS(mul, a, b)
SP_SIMD_OP_2_VECTORS(min, a, b)
SP_SIMD_OP_2_VECTORS(max, a, b)
SP_SIMD_OP_3_VECTORS(clamp, v, min, max)

	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto load(const void *ptr) = delete;
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
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdj(Vector<T> a, Vector<T> b) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdj<i16>(Vector<i16> a, Vector<i16> b)
	{
		return impl::mulAddAdjI16(a, b);
	}


	template <typename T>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAcc(Vector<T> sum, Vector<T> a, Vector<T> b) = delete;
	template <>
	SP_ALWAYS_INLINE_NDEBUG inline auto mulAddAdjAcc<i16>(Vector<i16> sum, Vector<i16> a, Vector<i16> b)
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

#undef SP_SIMD_OP_0
#undef SP_SIMD_OP_1_VALUE
#undef SP_SIMD_OP_2_VECTORS
#undef SP_SIMD_OP_3_VECTORS
}
