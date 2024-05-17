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

#include <array>

namespace stormphrax::util
{
	namespace internal
	{
		template <typename T, usize...>
		struct MultiArrayImpl {};

		template <typename T, usize N1>
		struct MultiArrayImpl<T, N1>
		{
			using Type = std::array<T, N1>;
		};

		template <typename T, usize N1, usize N2>
		struct MultiArrayImpl<T, N1, N2>
		{
			using Type = std::array<
				std::array<
					T, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3>
		struct MultiArrayImpl<T, N1, N2, N3>
		{
			using Type = std::array<
				std::array<
					std::array<
						T, N3
					>, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3, usize N4>
		struct MultiArrayImpl<T, N1, N2, N3, N4>
		{
			using Type = std::array<
				std::array<
					std::array<
						std::array<
							T, N4
						>, N3
					>, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3, usize N4, usize N5>
		struct MultiArrayImpl<T, N1, N2, N3, N4, N5>
		{
			using Type = std::array<
				std::array<
					std::array<
						std::array<
							std::array<
								T, N5
							>, N4
						>, N3
					>, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3, usize N4, usize N5, usize N6>
		struct MultiArrayImpl<T, N1, N2, N3, N4, N5, N6>
		{
			using Type = std::array<
				std::array<
					std::array<
						std::array<
							std::array<
								std::array<
									T, N6
								>, N5
							>, N4
						>, N3
					>, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3, usize N4, usize N5, usize N6, usize N7>
		struct MultiArrayImpl<T, N1, N2, N3, N4, N5, N6, N7>
		{
			using Type = std::array<
				std::array<
					std::array<
						std::array<
							std::array<
								std::array<
									std::array<
										T, N7
									>, N6
								>, N5
							>, N4
						>, N3
					>, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3, usize N4, usize N5, usize N6, usize N7, usize N8>
		struct MultiArrayImpl<T, N1, N2, N3, N4, N5, N6, N7, N8>
		{
			using Type = std::array<
				std::array<
					std::array<
						std::array<
							std::array<
								std::array<
									std::array<
										std::array<
											T, N8
										>, N7
									>, N6
								>, N5
							>, N4
						>, N3
					>, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3, usize N4, usize N5, usize N6, usize N7, usize N8, usize N9>
		struct MultiArrayImpl<T, N1, N2, N3, N4, N5, N6, N7, N8, N9>
		{
			using Type = std::array<
				std::array<
					std::array<
						std::array<
							std::array<
								std::array<
									std::array<
										std::array<
											std::array<
												T, N9
											>, N8
										>, N7
									>, N6
								>, N5
							>, N4
						>, N3
					>, N2
				>, N1
			>;
		};

		template <typename T, usize N1, usize N2, usize N3, usize N4,
			usize N5, usize N6, usize N7, usize N8, usize N9, usize N10>
		struct MultiArrayImpl<T, N1, N2, N3, N4, N5, N6, N7, N8, N9, N10>
		{
			using Type = std::array<
				std::array<
					std::array<
						std::array<
							std::array<
								std::array<
									std::array<
										std::array<
											std::array<
												std::array<
													T, N10
												>, N9
											>, N8
										>, N7
									>, N6
								>, N5
							>, N4
						>, N3
					>, N2
				>, N1
			>;
		};
	}

	template <typename T, usize... Ns>
	using MultiArray = typename internal::MultiArrayImpl<T, Ns...>::Type;
}
