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

#include <array>
#include <span>
#include <istream>
#include <ostream>
#include <cassert>
#include <type_traits>

#include "activation.h"
#include "output.h"
#include "../../util/simd.h"

namespace stormphrax::eval::nnue
{
	template <typename Input, typename Weight, activation::Activation Activation,
		u32 Inputs, u32 Outputs, output::OutputBucketing OutputBucketing>
	struct BaseAffineLayer
	{
		using  InputType = Input;
		using WeightType = Weight;
		using OutputType = Activation::Type;

		static constexpr auto  InputCount =  Inputs;
		static constexpr auto OutputCount = Outputs;

		static constexpr auto OutputBucketCount = OutputBucketing::BucketCount;

		static constexpr auto WeightCount =  InputCount * OutputCount;
		static constexpr auto   BiasCount = OutputCount;

		static_assert( InputCount > 0);
		static_assert(OutputCount > 0);

		static_assert(sizeof(InputType) * InputCount >= util::SimdAlignment
			&& (sizeof(InputType) * InputCount) % util::SimdAlignment == 0);

		static_assert(sizeof(WeightType) * WeightCount >= util::SimdAlignment
			&& (sizeof(WeightType) * WeightCount) % util::SimdAlignment == 0);

		SP_SIMD_ALIGNAS std::array<WeightType, OutputBucketCount * WeightCount> weights;
		SP_SIMD_ALIGNAS std::array<OutputType, OutputBucketCount *   BiasCount> biases;

		inline auto readFrom(std::istream &stream) -> std::istream &
		{
			stream.read(reinterpret_cast<char *>(weights.data()), weights.size() * sizeof(WeightType));
			stream.read(reinterpret_cast<char *>( biases.data()),  biases.size() * sizeof(OutputType));

			return stream;
		}

		inline auto writeTo(std::ostream &stream) const -> std::ostream &
		{
			stream.write(reinterpret_cast<char *>(weights.data()), weights.size() * sizeof(WeightType));
			stream.write(reinterpret_cast<char *>( biases.data()),  biases.size() * sizeof(OutputType));

			return stream;
		}
	};

	template <typename Input, activation::Activation Activation, typename Weight,
		u32 Inputs, u32 Outputs, output::OutputBucketing OutputBucketing>
	inline auto operator>>(std::istream &stream,
		BaseAffineLayer<Input, Activation, Weight, Inputs, Outputs, OutputBucketing> &layer)
	-> std::istream &
	{
		return layer.readFrom(stream);
	}

	template <typename Input, activation::Activation Activation, typename Weight,
		u32 Inputs, u32 Outputs, output::OutputBucketing OutputBucketing>
	inline auto operator<<(std::ostream &stream,
		const BaseAffineLayer<Input, Activation, Weight, Inputs, Outputs, OutputBucketing> &layer)
	-> std::ostream &
	{
		return layer.writeTo(stream);
	}

	template <typename Input, typename Weight, activation::Activation Activation,
	    u32 Inputs, u32 Outputs, output::OutputBucketing OutputBucketing = output::Single>
	struct DenseAffineLayer : BaseAffineLayer<Input, Weight, Activation, Inputs, Outputs, OutputBucketing>
	{
		using Base = BaseAffineLayer<Input, Weight, Activation, Inputs, Outputs, OutputBucketing>;

		// for larger layers, operations are done in blocks of 256
		// values - this allows GCC to unroll loops without dying
		// cheers @jhonnold for the tip
		static_assert(Base::InputCount < 512 || (Base::InputCount % 256) == 0);

		inline auto forward(const PositionBoards &boards,
			std::span<const typename Base::InputType, Base::InputCount> inputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			assert(util::isAligned( inputs.data()));
			assert(util::isAligned(outputs.data()));

			const auto outputBucket = OutputBucketing::getBucket(boards);

			const auto bucketWeightOffset = outputBucket * Base::WeightCount;
			const auto   bucketBiasOffset = outputBucket * Base::  BiasCount;

			for (u32 outputIdx = 0; outputIdx < Base::OutputCount; ++outputIdx)
			{
				const auto weightOffset = bucketWeightOffset + outputIdx * Base::InputCount;

				typename Base::OutputType sum{0};

				if constexpr(Base::InputCount >= 512)
				{
					for (u32 i = 0; i < Base::InputCount; i += 256)
					{
						for (u32 j = 0; j < 256; ++j)
						{
							const auto inputIdx = i + j;

							const auto input = static_cast<Base::OutputType>(inputs[inputIdx]);
							const auto activated = Activation::activate(input);

							const auto weight = static_cast<Base::OutputType>(Base::weights[weightOffset + inputIdx]);

							sum += weight * activated;
						}
					}
				}
				else
				{
					for (u32 inputIdx = 0; inputIdx < Base::InputCount; ++inputIdx)
					{
						const auto input = static_cast<Base::OutputType>(inputs[inputIdx]);
						const auto activated = Activation::activate(input);

						const auto weight = static_cast<Base::OutputType>(Base::weights[weightOffset + inputIdx]);

						sum += weight * activated;
					}
				}

				outputs[outputIdx] = Base::biases[bucketBiasOffset + outputIdx] + Activation::output(sum);
			}
		}
	};

	template <typename Input, typename Weight, activation::Activation Activation,
		u32 Inputs, u32 Outputs, output::OutputBucketing OutputBucketing = output::Single>
	struct DensePerspectiveAffineLayer
		: BaseAffineLayer<Input, Weight, Activation, Inputs * 2, Outputs, OutputBucketing>
	{
		using Base = BaseAffineLayer<Input, Weight, Activation, Inputs * 2, Outputs, OutputBucketing>;

		static constexpr auto PerspectiveInputCount = Inputs;
		static_assert(PerspectiveInputCount < 512 || (PerspectiveInputCount % 256) == 0);

		inline auto forward(const PositionBoards &boards,
			std::span<const typename Base::InputType, PerspectiveInputCount>  stmInputs,
			std::span<const typename Base::InputType, PerspectiveInputCount> nstmInputs,
			std::span<typename Base::OutputType, Base::OutputCount> outputs) const
		{
			assert(util::isAligned( stmInputs.data()));
			assert(util::isAligned(nstmInputs.data()));
			assert(util::isAligned(   outputs.data()));

			const auto outputBucket = OutputBucketing::getBucket(boards);

			const auto bucketWeightOffset = outputBucket * Base::WeightCount;
			const auto   bucketBiasOffset = outputBucket * Base::  BiasCount;

			for (u32 outputIdx = 0; outputIdx < Base::OutputCount; ++outputIdx)
			{
				const auto weightOffset = bucketWeightOffset + outputIdx * PerspectiveInputCount;

				typename Base::OutputType sum{0};

				if constexpr(PerspectiveInputCount >= 512)
				{
					// stm perspective
					for (u32 i = 0; i < PerspectiveInputCount; i += 256)
					{
						for (u32 j = 0; j < 256; ++j)
						{
							const auto inputIdx = i + j;

							const auto input = static_cast<Base::OutputType>(stmInputs[inputIdx]);
							const auto activated = Activation::activate(input);

							const auto weight = static_cast<Base::OutputType>(Base::weights[weightOffset + inputIdx]);

							sum += weight * activated;
						}
					}

					// nstm perspective
					for (u32 i = 0; i < PerspectiveInputCount; i += 256)
					{
						for (u32 j = 0; j < 256; ++j)
						{
							const auto inputIdx = i + j;

							const auto input = static_cast<Base::OutputType>(nstmInputs[inputIdx]);
							const auto activated = Activation::activate(input);

							const auto weight = static_cast<Base::OutputType>(
								Base::weights[PerspectiveInputCount + weightOffset + inputIdx]
							);

							sum += weight * activated;
						}
					}
				}
				else
				{
					// stm perspective
					for (u32 inputIdx = 0; inputIdx < PerspectiveInputCount; ++inputIdx)
					{
						const auto input = static_cast<Base::OutputType>(stmInputs[inputIdx]);
						const auto activated = Activation::activate(input);

						const auto weight = static_cast<Base::OutputType>(Base::weights[weightOffset + inputIdx]);

						sum += weight * activated;
					}

					// nstm perspective
					for (u32 inputIdx = 0; inputIdx < PerspectiveInputCount; ++inputIdx)
					{
						const auto input = static_cast<Base::OutputType>(nstmInputs[inputIdx]);
						const auto activated = Activation::activate(input);

						const auto weight = static_cast<Base::OutputType>(
							Base::weights[PerspectiveInputCount + weightOffset + inputIdx]
						);

						sum += weight * activated;
					}
				}

				outputs[outputIdx] = Base::biases[bucketBiasOffset + outputIdx] + Activation::output(sum);
			}
		}
	};
}
