/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
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

#include "ctrlc.h"

#include <iostream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
#else
    #include <signal.h>
#endif

namespace stormphrax::util::signal {
    namespace {
        std::vector<CtrlCHandler> s_handlers{};
    }

    auto addCtrlCHandler(CtrlCHandler handler) -> void {
        s_handlers.push_back(std::move(handler));
    }

    auto init() -> void {
#ifdef _WIN32
        const auto result = SetConsoleCtrlHandler(
            [](DWORD dwCtrlType) -> BOOL {
                if (dwCtrlType == CTRL_BREAK_EVENT) {
                    return FALSE;
                }

                for (auto& handler : s_handlers) {
                    handler();
                }

                return TRUE;
            },
            TRUE
        );

        if (!result) {
            std::cerr << "failed to set ctrl+c handler" << std::endl;
        }
#else
        struct sigaction action{.sa_flags = SA_RESTART};

        // on some platforms this is a union, and C++ doesn't support nested designated initialisers
        action.sa_handler = [](int signal) {
            for (auto& handler : s_handlers) {
                handler();
            }
        };

        if (sigaction(SIGINT, &action, nullptr)) {
            std::cerr << "failed to set SIGINT handler" << std::endl;
        }

        if (sigaction(SIGTERM, &action, nullptr)) {
            std::cerr << "failed to set SIGTERM handler" << std::endl;
        }
#endif
    }
} // namespace stormphrax::util::signal
