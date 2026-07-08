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

#include <span>
#include <string_view>
#include <vector>

#include "bench.h"
#include "cuckoo.h"
#include "datagen/datagen.h"
#include "tunable.h"
#include "uci/uci.h"
#include "util/numa/numa.h"
#include "util/parse.h"

#if SP_EXTERNAL_TUNE
    #include "util/split.h"
#endif

using namespace stormphrax;

namespace {
    i32 run(std::span<const std::string_view> args) {
        if (args.size() > 1) {
            const auto mode = args[1];

            if (mode == "datagen") {
                const auto printUsage = [&] {
                    eprintln(
                        "usage: {} datagen <marlinformat/viriformat/fen> <standard/dfrc> <path> [threads] [syzygy path]",
                        args[0]
                    );
                };

                if (args.size() < 5) {
                    printUsage();
                    return 1;
                }

                bool dfrc = false;

                if (args[3] == "dfrc") {
                    dfrc = true;
                } else if (args[3] != "standard") {
                    eprintln("invalid variant '{}'", args[3]);
                    printUsage();
                    return 1;
                }

                u32 threads = 1;
                if (args.size() > 5 && !util::tryParse<u32>(threads, args[5])) {
                    eprintln("invalid number of threads '{}'", args[5]);
                    printUsage();
                    return 1;
                }

                std::optional<std::string_view> tbPath{};
                if (args.size() > 6) {
                    tbPath = args[6];
                }

                return datagen::run(printUsage, args[2], dfrc, args[4], static_cast<i32>(threads), tbPath);
            }
#if SP_EXTERNAL_TUNE
            else if (mode == "printwf" || mode == "printctt" || mode == "printob")
            {
                if (args.size() == 2) {
                    return 0;
                }

                std::vector<std::string_view> params{};
                split::split(params, args[2], ',');

                if (mode == "printwf") {
                    uci::printWfTuningParams(params);
                } else if (mode == "printctt") {
                    uci::printCttTuningParams(params);
                } else if (mode == "printob") {
                    uci::printObTuningParams(params);
                }

                return 0;
            }
#endif
        }

        return uci::run(args.subspan<1>());
    }
} // namespace

i32 main(i32 argc, const char* argv[]) {
    if (!numa::init()) {
        eprintln("Failed to initialize NUMA support");
        return 1;
    }

    tunable::init();
    cuckoo::init();

    std::vector<std::string_view> args{};
    args.reserve(argc);

    for (i32 i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const auto exitCode = run(args);

    return exitCode;
}
