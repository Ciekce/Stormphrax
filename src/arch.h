/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#if defined(PS_NATIVE) || defined(PS_BMI2)
	#if __BMI2__ && (defined(PS_BMI2) || defined(PS_FAST_PEXT))
		#define PS_HAS_BMI2 1
	#else
		#define PS_HAS_BMI2 0
	#endif
	#if __BMI__
		#define PS_HAS_BMI1 1
	#else
		#define PS_HAS_BMI1 0
	#endif
	#if __POPCNT__
		#define PS_HAS_POPCNT 1
	#else
		#define PS_HAS_POPCNT 0
	#endif
#elif defined(PS_HASWELL)
	#define PS_HAS_BMI2 0
	#define PS_HAS_BMI1 1
	#define PS_HAS_POPCNT 1
#elif defined(PS_POPCNT)
	#define PS_HAS_BMI2 0
	#define PS_HAS_BMI1 0
	#define PS_HAS_POPCNT 1
#elif defined(PS_COMPAT)
	#define PS_HAS_BMI2 0
	#define PS_HAS_BMI1 0
	#define PS_HAS_POPCNT 0
#endif
