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

#include "bench.h"
#include "cuckoo.h"
#include "datagen/datagen.h"
#include "eval/nnue.h"
#include "tunable.h"
#include "uci.h"
#include "util/numa/numa.h"
#include "util/parse.h"

#if SP_EXTERNAL_TUNE
    #include "util/split.h"
#endif

using namespace stormphrax;

namespace {
    i32 run(i32 argc, const char* argv[]) {
        if (argc > 1) {
            const std::string_view mode{argv[1]};

            if (mode == "bench") {
                bench::run();
                return 0;
            } else if (mode == "datagen") {
                const auto printUsage = [&]() {
                    eprintln(
                        "usage: {} datagen <marlinformat/viriformat/fen> <standard/dfrc> <path> [threads] [syzygy path]",
                        argv[0]
                    );
                };

                if (argc < 5) {
                    printUsage();
                    return 1;
                }

                bool dfrc = false;

                if (std::string_view{argv[3]} == "dfrc") {
                    dfrc = true;
                } else if (std::string_view{argv[3]} != "standard") {
                    eprintln("invalid variant {}", argv[3]);
                    printUsage();
                    return 1;
                }

                u32 threads = 1;
                if (argc > 5 && !util::tryParse<u32>(threads, argv[5])) {
                    eprintln("invalid number of threads {}", argv[5]);
                    printUsage();
                    return 1;
                }

                std::optional<std::string_view> tbPath{};
                if (argc > 6) {
                    tbPath = std::string_view{argv[6]};
                }

                return datagen::run(printUsage, argv[2], dfrc, argv[4], static_cast<i32>(threads), tbPath);
            }
#if SP_EXTERNAL_TUNE
            else if (mode == "printwf" || mode == "printctt" || mode == "printob")
            {
                if (argc == 2) {
                    return 0;
                }

                std::vector<std::string_view> params{};
                split::split(params, argv[2], ',');

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

        return uci::run();
    }
} // namespace

i32 main(i32 argc, const char* argv[]) {
    if (!numa::init()) {
        eprintln("Failed to initialize NUMA support");
        return 1;
    }

    tunable::init();
    cuckoo::init();

    eval::init();

    const auto exitCode = run(argc, argv);

    eval::shutdown();

    return exitCode;
}
