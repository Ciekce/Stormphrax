/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
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

#include "split.h"

#include <algorithm>

namespace stormphrax::split {
    void split(std::vector<std::string_view>& dst, std::string_view str, char delim) {
        while (true) {
            if (str.empty()) {
                break;
            }

            const auto end = std::ranges::find(str, delim);

            if (end == str.begin()) {
                str = str.substr(1);
                continue;
            } else if (end == str.end()) {
                dst.push_back(str);
                break;
            }

            dst.emplace_back(str.begin(), end);
            str = std::string_view{end, str.end()};
        }
    }
} // namespace stormphrax::split
