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

#include "eval.h"

#ifdef _MSC_VER
#define PS_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif

#define INCBIN_PREFIX g_
#include "../3rdparty/incbin.h"

#ifdef PS_MSVC
#pragma pop_macro("_MSC_VER")
#undef PS_MSVC
#endif

INCBIN(net, SP_NETWORK_FILE);

namespace stormphrax::eval
{
	// technically this might not be aligned as expected
	// but as incbin aligns for the worst case possible it's fine in practice
	// in theory though we're violating the alignment requirements of Network /shrug
	const Network &g_net = *reinterpret_cast<const Network *>(g_netData);
}
