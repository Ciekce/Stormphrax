<div align="center">

# Stormphrax

[![License][license-badge]][license-link]  
[![GitHub release (latest by date)][release-badge]][release-link]
[![Commits since latest release][commits-badge]][commits-link]

![LGBTQ+ friendly][lgbtqp-badge]
![trans rights][trans-rights-badge]

</div>

a work-in-progress UCI chess and [chess960](https://en.wikipedia.org/wiki/Fischer_random_chess) engine, with NNUE evaluation trained from zero knowledge starting with random weights

this project is a continuation of my HCE engine [Polaris](https://github.com/Ciekce/Polaris)

## Strength
| Version | [CCRL 40/15][ccrl-4015] | [CCRL Blitz][ccrl-blitz] | [CCRL 40/2 FRC][ccrl-402-frc] | [SPCC][spcc] | [MCERL][mcerl] |
|:-------:|:-----------------------:|:------------------------:|:-----------------------------:|:------------:|:--------------:|
|  1.0.0  |          3268           |           3361           |             3513              |     3343     |      3342      |

## Features
- standard PVS with quiescence search and iterative deepening
  - aspiration windows
  - check extensions
  - countermoves
  - futility pruning
  - history
    - capture history
    - countermove history (1-ply continuation history)
    - follow-up history (2-ply continuation history)
  - internal iterative reduction
  - killers (1 per ply)
  - late move reductions
  - mate distance pruning
  - multicut
  - nullmove pruning
  - reverse futility pruning
  - SEE move ordering and pruning
  - singular extensions
  - Syzygy tablebase support
- NNUE
  - (768->384)x2->1 architecture
  - trained from zero knowledge with reinforcement learning from a randomly-initialised network
- BMI2 attacks in the `bmi2` build, otherwise fancy black magic
  - `pext`/`pdep` for rooks
  - `pext` for bishops
- lazy SMP

## To-do
- tune search constants
- contempt
- make it stronger uwu

## UCI options
| Name             |  Type   | Default value |       Valid values        | Description                                                                                                 |
|:-----------------|:-------:|:-------------:|:-------------------------:|:------------------------------------------------------------------------------------------------------------|
| Hash             | integer |      64       |        [1, 131072]        | Memory allocated to the transposition table (in MB). Rounded down internally to the next-lowest power of 2. |
| Clear Hash       | button  |      N/A      |            N/A            | Clears the transposition table.                                                                             |
| Threads          | integer |       1       |         [1, 2048]         | Number of threads used to search.                                                                           |
| UCI_Chess960     |  check  |    `false`    |      `false`, `true`      | Whether Stormphrax plays Chess960 instead of standard chess.                                                |
| UCI_ShowWDL      |  check  |    `true`     |      `false`, `true`      | Whether Stormphrax displays predicted win/draw/loss probabilities in UCI output.                            |
| Move Overhead    | integer |      10       |        [0, 50000]         | Amount of time Stormphrax assumes to be lost to overhead when making a move (in ms).                        |
| SyzygyPath       | string  |   `<empty>`   |  any path, or `<empty>`   | Location of Syzygy tablebases to probe during search.                                                       |
| SyzygyProbeDepth |  spin   |       1       |         [1, 255]          | Minimum depth to probe Syzygy tablebases at.                                                                |
| SyzygyProbeLimit |  spin   |       7       |          [0, 7]           | Maximum number of pieces on the board to probe Syzygy tablebases with.                                      |
| EvalFile         | string  | `<internal>`  | any path, or `<internal>` | NNUE file to use for evaluation.                                                                            |

## Builds
`avx512`: requires AVX-512 (Zen 4, Skylake-X)  
`avx2-bmi2`: requires BMI2 and AVX2 and assumes fast `pext` and `pdep` (i.e. no Zen 1 and 2)  
`avx2`: requires BMI and AVX2 - primarily useful for pre-Zen 3 AMD CPUs back to Excavator  
`popcnt`: just needs `popcnt` - for older x64 CPUs

Alternatively, build the CMake target `stormphrax-native` for a binary tuned for your specific CPU (see below)  
(note that this does *not* automatically disable `pext` and `pdep` for pre-Zen 3 AMD CPUs that implement them in microcode)

### Note:  
- If you have an AMD Zen 1 (Ryzen x 1xxx) or 2 (Ryzen x 2xxx) CPU, use the `avx2` build even though your CPU supports BMI2. These CPUs implement the BMI2 instructions `pext` and `pdep` in microcode, which makes them unusably slow for Stormphrax's purposes.
- Builds other than `bmi2` are untested and might crash on CPUs lacking newer instructions; I don't have older hardware to test them on.

## Building
**The makefile is not intended for building by users. It exists purely for OpenBench compliance.**  
Requires CMake and a competent C++20 compiler (tested with Clang 15 and 16 on Windows, GCC 11 and Clang 15 and 16 on Linux, and Apple Clang 14 on macOS on Apple Silicon)
```bash
> cmake -DCMAKE_BUILD_TYPE=Release -S . -B build/
> cmake --build build/ --target stormphrax-<TARGET>
```
(replace `<TARGET>` with your preferred target - `native`/`bmi2`/`modern`/`popcnt`/`compat`)

If you have a pre-Zen 3 AMD Ryzen CPU (see the notes in Builds above) and want to build the `native` target, use these commands instead (the second is unchanged):
```bash
> cmake -DCMAKE_BUILD_TYPE=Release -DSP_FAST_PEXT=OFF -S . -B build/
> cmake --build build/ --target stormphrax-native
```
Disabling the CMake option `SP_FAST_PEXT` builds the non-BMI2 attack getters.

## Credit
Stormphrax uses [Fathom](https://github.com/jdart1/Fathom) for tablebase probing, licensed under the MIT license, and a slightly modified version of [incbin](https://github.com/graphitemaster/incbin) for embedding neural network files, under the Unlicense.

The name "Stormphrax" is a reference to the excellent [Edge Chronicles][edge-chronicles] :)

[license-badge]: https://img.shields.io/github/license/Ciekce/Stormphrax?style=for-the-badge
[release-badge]: https://img.shields.io/github/v/release/Ciekce/Stormphrax?style=for-the-badge
[commits-badge]: https://img.shields.io/github/commits-since/Ciekce/Stormphrax/latest?style=for-the-badge

[license-link]: https://github.com/Ciekce/Stormphrax/blob/main/LICENSE
[release-link]: https://github.com/Ciekce/Stormphrax/releases/latest
[commits-link]: https://github.com/Ciekce/Stormphrax/commits/main

[lgbtqp-badge]: https://pride-badges.pony.workers.dev/static/v1?label=lgbtq%2B%20friendly&stripeWidth=6&stripeColors=E40303,FF8C00,FFED00,008026,24408E,732982
[trans-rights-badge]: https://pride-badges.pony.workers.dev/static/v1?label=trans%20rights&stripeWidth=6&stripeColors=5BCEFA,F5A9B8,FFFFFF,F5A9B8,5BCEFA

[ccrl-4015]: https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list
[ccrl-blitz]: https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list
[ccrl-402-frc]: https://www.computerchess.org.uk/ccrl/404FRC/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list
[spcc]: https://www.sp-cc.de/
[mcerl]: https://www.chessengeria.eu/mcerl

[edge-chronicles]: https://en.wikipedia.org/wiki/The_Edge_Chronicles
