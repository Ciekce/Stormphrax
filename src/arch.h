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

#include "types.h"

#if defined(SP_NATIVE) || defined(SP_BMI2)
	#if __BMI2__ && (defined(SP_BMI2) || defined(SP_FAST_PEXT))
		#define SP_HAS_BMI2 1
	#else
		#define SP_HAS_BMI2 0
	#endif
	#if __BMI__
		#define SP_HAS_BMI1 1
	#else
		#define SP_HAS_BMI1 0
	#endif
	#if __POPCNT__
		#define SP_HAS_POPCNT 1
	#else
		#define SP_HAS_POPCNT 0
	#endif
#elif defined(SP_MODERN)
	#define SP_HAS_BMI2 0
	#define SP_HAS_BMI1 1
	#define SP_HAS_POPCNT 1
#elif defined(SP_POPCNT)
	#define SP_HAS_BMI2 0
	#define SP_HAS_BMI1 0
	#define SP_HAS_POPCNT 1
#elif defined(SP_COMPAT)
	#define SP_HAS_BMI2 0
	#define SP_HAS_BMI1 0
	#define SP_HAS_POPCNT 0
#else
#error no arch specified
#endif
