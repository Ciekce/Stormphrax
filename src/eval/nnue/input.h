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

#include <array>
#include <istream>
#include <ostream>
#include <span>
#include <algorithm>

#include "../../core.h"
#include "../../util/simd.h"
#include "../../position/boards.h"
#include "io.h"
#include "features.h"

namespace stormphrax::eval::nnue
{
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

		inline void initBoth(const Ft &featureTransformer)
		{
			std::ranges::copy(featureTransformer.biases, m_outputs[0].begin());
			std::ranges::copy(featureTransformer.biases, m_outputs[1].begin());
		}

		inline auto subAdd(const Ft &featureTransformer, Color c, u32 sub, u32 add)
		{
			assert(sub < InputCount);
			assert(add < InputCount);

			subAdd(forColor(c), featureTransformer.weights, sub * OutputCount, add * OutputCount);
		}

		inline auto subSubAdd(const Ft &featureTransformer, Color c, u32 sub0, u32 sub1, u32 add)
		{
			assert(sub0 < InputCount);
			assert(sub1 < InputCount);
			assert(add  < InputCount);

			subSubAdd(forColor(c), featureTransformer.weights,
				sub0 * OutputCount, sub1 * OutputCount, add * OutputCount);
		}

		inline auto subSubAddAdd(const Ft &featureTransformer, Color c, u32 sub0, u32 sub1, u32 add0, u32 add1)
		{
			assert(sub0 < InputCount);
			assert(sub1 < InputCount);
			assert(add0 < InputCount);
			assert(add1 < InputCount);

			subSubAddAdd(forColor(c), featureTransformer.weights,
				sub0 * OutputCount, sub1 * OutputCount, add0 * OutputCount, add1 * OutputCount);
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

		inline auto copyFrom(Color c, const Accumulator<Ft> &other)
		{
			const auto idx = static_cast<i32>(c);
			std::ranges::copy(other.m_outputs[idx], m_outputs[idx].begin());
		}

	private:
		using Type = typename Ft::OutputType;

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
						accumulator[idx] += delta[addOffset + idx]
							- delta[subOffset + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < OutputCount; ++i)
				{
					accumulator[i] += delta[addOffset + i]
						- delta[subOffset + i];
				}
			}
		}

		static inline auto subSubAdd(std::span<Type, OutputCount> accumulator,
			std::span<const Type, WeightCount> delta, u32 subOffset0, u32 subOffset1, u32 addOffset) -> void
		{
			assert(subOffset0 + OutputCount <= delta.size());
			assert(subOffset1 + OutputCount <= delta.size());
			assert(addOffset  + OutputCount <= delta.size());

			if constexpr(OutputCount >= 512)
			{
				for (usize i = 0; i < OutputCount; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						accumulator[idx] += delta[addOffset + idx]
							- delta[subOffset0 + idx]
							- delta[subOffset1 + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < OutputCount; ++i)
				{
					accumulator[i] += delta[addOffset + i]
						- delta[subOffset0 + i]
						- delta[subOffset1 + i];
				}
			}
		}

		static inline auto subSubAddAdd(std::span<Type, OutputCount> accumulator,
			std::span<const Type, WeightCount> delta,
			u32 subOffset0, u32 subOffset1, u32 addOffset0, u32 addOffset1) -> void
		{
			assert(subOffset0 + OutputCount <= delta.size());
			assert(subOffset1 + OutputCount <= delta.size());
			assert(addOffset0 + OutputCount <= delta.size());
			assert(addOffset1 + OutputCount <= delta.size());

			if constexpr(OutputCount >= 512)
			{
				for (usize i = 0; i < OutputCount; i += 256)
				{
					for (u32 j = 0; j < 256; ++j)
					{
						const auto idx = i + j;
						accumulator[idx] += delta[addOffset0 + idx]
							- delta[subOffset0 + idx]
							+ delta[addOffset1 + idx]
							- delta[subOffset1 + idx];
					}
				}
			}
			else
			{
				for (u32 i = 0; i < OutputCount; ++i)
				{
					accumulator[i] += delta[addOffset0 + i]
						- delta[subOffset0 + i]
						+ delta[addOffset1 + i]
						- delta[subOffset1 + i];
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

	template <typename Acc>
	struct RefreshTableEntry
	{
		Acc accumulator{};
		std::array<PositionBoards, 2> boards{};

		[[nodiscard]] auto colorBoards(Color c) -> auto &
		{
			return boards[static_cast<i32>(c)];
		}
	};

	template <typename Ft, u32 BucketCount>
	struct RefreshTable
	{
		std::array<RefreshTableEntry<Accumulator<Ft>>, BucketCount> table{};

		inline void init(const Ft &featureTransformer)
		{
			for (auto &entry : table)
			{
				entry.accumulator.initBoth(featureTransformer);
				entry.boards.fill(PositionBoards{});
			}
		}
	};

	template <typename Type, u32 Inputs, u32 Outputs, typename FeatureSet = features::SingleBucket>
	struct FeatureTransformer
	{
		using WeightType = Type;
		using OutputType = Type;

		using InputFeatureSet = FeatureSet;

		using Accumulator = Accumulator<FeatureTransformer<Type, Inputs, Outputs, FeatureSet>>;
		using RefreshTable = RefreshTable<FeatureTransformer<Type, Inputs, Outputs, FeatureSet>,
		    FeatureSet::BucketCount>;

		static constexpr auto  InputCount = InputFeatureSet::BucketCount * Inputs;
		static constexpr auto OutputCount = Outputs;

		static constexpr auto WeightCount =  InputCount * OutputCount;
		static constexpr auto   BiasCount = OutputCount;

		static_assert( InputCount > 0);
		static_assert(OutputCount > 0);

		static_assert(OutputCount < 512 || (OutputCount % 256) == 0);

		SP_SIMD_ALIGNAS std::array<WeightType, WeightCount> weights;
		SP_SIMD_ALIGNAS std::array<OutputType,   BiasCount> biases;

		inline auto readFrom(IParamStream &stream) -> bool
		{
			return stream.read(weights)
				&& stream.read(biases);
		}

		inline auto writeTo(IParamStream &stream) const -> bool
		{
			return stream.write(weights)
				&& stream.write(biases);
		}
	};
}
