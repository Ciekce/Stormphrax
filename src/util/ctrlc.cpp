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

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX // mingw
        #define NOMINMAX
    #endif
    #include <Windows.h>
#else
    #include <signal.h>
#endif

namespace stormphrax::util::signal {
    namespace {
        std::vector<CtrlCHandler> s_handlers{};
    }

    void addCtrlCHandler(CtrlCHandler handler) {
        s_handlers.push_back(std::move(handler));
    }

    void init() {
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
            eprintln("failed to set ctrl+c handler");
        }
#else
        struct sigaction action{};

        action.sa_flags = SA_RESTART;
        action.sa_handler = []([[maybe_unused]] int signal) {
            for (auto& handler : s_handlers) {
                handler();
            }
        };

        if (sigaction(SIGINT, &action, nullptr)) {
            eprintln("failed to set SIGINT handler");
        }

        if (sigaction(SIGTERM, &action, nullptr)) {
            eprintln("failed to set SIGTERM handler");
        }
#endif
    }
} // namespace stormphrax::util::signal
