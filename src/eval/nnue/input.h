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

#include <array>
#include <istream>
#include <ostream>
#include <span>
#include <algorithm>

#include "../../core.h"
#include "../../util/simd.h"
#include "io.h"

namespace stormphrax::eval::nnue
{
	template <typename Type, u32 Inputs, u32 Outputs, u32 Buckets = 1>
	struct FeatureTransformer
	{
		using WeightType = Type;
		using OutputType = Type;

		static constexpr auto InputBucketCount = Buckets;

		static constexpr auto  InputCount = InputBucketCount * Inputs;
		static constexpr auto OutputCount = Outputs;

		static constexpr auto WeightCount =  InputCount * OutputCount;
		static constexpr auto   BiasCount = OutputCount;

		static_assert( InputCount > 0);
		static_assert(OutputCount > 0);

		static_assert(OutputCount < 512 || (OutputCount % 256) == 0);

		SP_SIMD_ALIGNAS std::array<WeightType, WeightCount> weights;
		SP_SIMD_ALIGNAS std::array<OutputType,   BiasCount> biases;

		inline auto readFrom(std::istream &stream) -> std::istream &
		{
			readPadded(stream, weights);
			readPadded(stream,  biases);

			return stream;
		}

		inline auto writeTo(std::ostream &stream) const -> std::ostream &
		{
			writePadded(stream, weights);
			writePadded(stream,  biases);

			return stream;
		}
	};

	template <typename Ft>
	class Accumulator
	{
	public:
		[[nodiscard]] inline auto black() const -> const auto &
		{
			return m_outputs[0];
		}

		[[nodiscard]] inline auto white() const -> const auto &
		{
			return m_outputs[1];
		}

		[[nodiscard]] inline auto forColor(Color c) const -> const auto &
		{
			assert(c != Color::None);
			return m_outputs[static_cast<i32>(c)];
		}

		[[nodiscard]] inline auto black() -> auto &
		{
			return m_outputs[0];
		}

		[[nodiscard]] inline auto white() -> auto &
		{
			return m_outputs[1];
		}

		[[nodiscard]] inline auto forColor(Color c) -> auto &
		{
			assert(c != Color::None);
			return m_outputs[static_cast<i32>(c)];
		}

		inline void init(const Ft &featureTransformer, Color c)
		{
			std::ranges::copy(featureTransformer.biases, forColor(c).begin());
		}

		inline auto moveFeature(const Ft &featureTransformer, Color c, u32 srcFeature, u32 dstFeature)
		{
			assert(srcFeature < InputCount);
			assert(dstFeature < InputCount);

			assert(srcFeature != dstFeature);

			subAdd(forColor(c), featureTransformer.weights, srcFeature * OutputCount, dstFeature * OutputCount);
		}

		inline auto activateFeature(const Ft &featureTransformer, Color c, u32 feature)
		{
			assert(feature < InputCount);
			add(forColor(c), featureTransformer.weights, feature * OutputCount);
		}

		inline auto deactivateFeature(const Ft &featureTransformer, Color c, u32 feature)
		{
			assert(feature < InputCount);
			sub(forColor(c), featureTransformer.weights, feature * OutputCount);
		}

	private:
		using Type = Ft::OutputType;

		static constexpr auto  InputCount = Ft:: InputCount;
		static constexpr auto WeightCount = Ft::WeightCount;
		static constexpr auto OutputCount = Ft::OutputCount;

		static_assert(OutputCount < 512 || (OutputCount % 256) == 0);

		SP_SIMD_ALIGNAS std::array<std::array<Type, OutputCount>, 2> m_outputs;

		static inline auto subAdd(std::span<Type, OutputCount> accumulator,
			std::span<const Type, WeightCount> delta, u32 subOffset, u32 addOffset) -> void
		{
			assert(subOffset + OutputCount <= delta.size());
			assert(addOffset + OutputCount <= delta.size());

			if constexpr(OutputCount >= 512)
			{
				for (usize i = 0; i < OutputCount; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						accumulator[idx] += delta[addOffset + idx] - delta[subOffset + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < OutputCount; ++i)
				{
					accumulator[i] += delta[addOffset + i] - delta[subOffset + i];
				}
			}
		}

		static inline auto add(std::span<Type, OutputCount> accumulator,
			std::span<const Type, WeightCount> delta, u32 offset) -> void
		{
			assert(offset + OutputCount <= delta.size());

			if constexpr(OutputCount >= 512)
			{
				for (usize i = 0; i < OutputCount; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						accumulator[idx] += delta[offset + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < OutputCount; ++i)
				{
					accumulator[i] += delta[offset + i];
				}
			}
		}

		static inline auto sub(std::span<Type, OutputCount> accumulator,
			std::span<const Type, WeightCount> delta, u32 offset) -> void
		{
			assert(offset + OutputCount <= delta.size());

			if constexpr(OutputCount >= 512)
			{
				for (usize i = 0; i < OutputCount; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						accumulator[idx] -= delta[offset + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < OutputCount; ++i)
				{
					accumulator[i] -= delta[offset + i];
				}
			}
		}
	};
}
