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

#include <tuple>
#include <utility>
#include <span>

#include "../../position/boards.h"
#include "../../util/aligned_array.h"
#include "io.h"
#include "features.h"

namespace stormphrax::eval::nnue
{
	template <typename Type, u32 Inputs, u32 Outputs, typename FeatureSet = features::SingleBucket>
	struct FeatureTransformer
	{
		using WeightType = Type;
		using OutputType = Type;

		using InputFeatureSet = FeatureSet;

		static constexpr auto  InputCount = InputFeatureSet::BucketCount * Inputs;
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

	template <typename Ft, typename... Layers>
	class PerspectiveNetwork
	{
	private:
		template <typename Layer>
		using OutputStorageType = util::AlignedArray<
		    util::SimdAlignment, typename Layer::OutputType, Layer::OutputCount
		>;

		using LayerStack = std::tuple<Layers...>;
		using OutputStorage = std::tuple<OutputStorageType<Layers>...>;

		static_assert(sizeof...(Layers) > 0);
		static_assert(std::tuple_element_t<sizeof...(Layers) - 1, LayerStack>::OutputCount == 1);

	public:
		using FeatureTransformer = Ft;

		static_assert(FeatureTransformer::OutputCount == std::tuple_element_t<0, LayerStack>::PerspectiveInputCount);

		[[nodiscard]] inline auto featureTransformer() const -> const auto &
		{
			return m_featureTransformer;
		}

		template <usize I>
		[[nodiscard]] inline auto layer() const -> const auto &
		{
			return std::get<I>(m_layers);
		}

		inline auto propagate(const PositionBoards &boards,
			std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::OutputCount>  stmInputs,
			std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::OutputCount> nstmInputs) const
		{
			OutputStorage storage{};

			std::get<0>(m_layers).forward(boards, stmInputs, nstmInputs, std::get<0>(storage));
			propagate(storage, boards, std::make_index_sequence<sizeof...(Layers)>());

			return std::get<sizeof...(Layers) - 1>(storage)[0];
		}

		inline auto readFrom(std::istream &stream) -> std::istream &
		{
			m_featureTransformer.readFrom(stream);
			readLayersFrom(std::make_index_sequence<sizeof...(Layers)>(), stream);

			return stream;
		}

		inline auto writeTo(std::ostream &stream) const -> std::ostream &
		{
			m_featureTransformer.writeTo(stream);
			writeLayersTo(std::make_index_sequence<sizeof...(Layers)>(), stream);

			return stream;
		}

	private:
		template <usize I>
		inline auto forward(OutputStorage &storage, const PositionBoards &boards) const
		{
			if constexpr (I > 0)
			{
				static_assert(std::tuple_element_t<I - 1, LayerStack>::OutputCount
					== std::tuple_element_t<I, LayerStack>::InputCount);

				auto &layer = std::get<I>(m_layers);
				layer.forward(boards, std::get<I - 1>(storage), std::get<I>(storage));
			}
		}

		template <usize... Indices>
		inline auto propagate(OutputStorage &storage,
			const PositionBoards &boards, std::index_sequence<Indices...>) const
		{
			((forward<Indices>(storage, boards)), ...);
		}

		template <usize... Indices>
		inline auto readLayersFrom(std::index_sequence<Indices...>, std::istream &stream)
		{
			((std::get<Indices>(m_layers).readFrom(stream)), ...);
		}

		template <usize... Indices>
		inline auto writeLayersTo(std::index_sequence<Indices...>, std::ostream &stream) const
		{
			((std::get<Indices>(m_layers).writeTo(stream)), ...);
		}

		FeatureTransformer m_featureTransformer{};
		LayerStack m_layers{};
	};

	template <typename Ft, typename... Layers>
	inline auto operator>>(std::istream &stream, PerspectiveNetwork<Ft, Layers...> &network) -> std::istream &
	{
		return network.readFrom(stream);
	}

	template <typename Ft, typename... Layers>
	inline auto operator<<(std::ostream &stream, PerspectiveNetwork<Ft, Layers...> &network) -> std::ostream &
	{
		return network.writeTo(stream);
	}
}
