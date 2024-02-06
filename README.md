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
| Version | [CCRL 40/15][ccrl-4015] | [CCRL Blitz][ccrl-blitz] | [CCRL 40/2 FRC][ccrl-402-frc] | [CEGT 40/4][cegt] | [MCERL][mcerl] |
|:-------:|:-----------------------:|:------------------------:|:-----------------------------:|:-----------------:|:--------------:|
|  4.0.0  | ~3402 (testing ongoing) |           3543           |             3745              |         -         |       -        |
|  3.0.0  |          3325           |           3470           |             3659              |         -         |       -        |
|  2.0.0  |          3320           |           3459           |             3640              |       3339        |      3483      |
|  1.0.0  |          3252           |           3360           |             3512              |       3235        |      3342      |

## Features
- standard PVS with quiescence search and iterative deepening
  - aspiration windows
  - check extensions
  - countermoves
  - futility pruning
  - history
    - capture history
    - 1-ply continuation history (countermove history)
    - 2-ply continuation history (follow-up history)
    - 4-ply continuation history
  - internal iterative reduction
  - killers (1 per ply)
  - late move reductions
  - late move pruning
  - mate distance pruning
  - multicut
  - nullmove pruning
  - reverse futility pruning
  - SEE move ordering and pruning
  - singular extensions
    - double extension
    - negative extension
  - Syzygy tablebase support
- NNUE
  - (768x4->768)x2->1x8 architecture
  - trained from zero knowledge with reinforcement learning from a randomly-initialised network
- BMI2 attacks in the `bmi2` build, otherwise fancy black magic
  - `pext`/`pdep` for rooks
  - `pext` for bishops
- lazy SMP
- static contempt

## To-do
- tune search constants
- make it stronger uwu

## UCI options
| Name             |  Type   | Default value |       Valid values        | Description                                                                          |
|:-----------------|:-------:|:-------------:|:-------------------------:|:-------------------------------------------------------------------------------------|
| Hash             | integer |      64       |        [1, 131072]        | Memory allocated to the transposition table (in MB).                                 |
| Clear Hash       | button  |      N/A      |            N/A            | Clears the transposition table.                                                      |
| Threads          | integer |       1       |         [1, 2048]         | Number of threads used to search.                                                    |
| UCI_Chess960     |  check  |    `false`    |      `false`, `true`      | Whether Stormphrax plays Chess960 instead of standard chess.                         |
| UCI_ShowWDL      |  check  |    `true`     |      `false`, `true`      | Whether Stormphrax displays predicted win/draw/loss probabilities in UCI output.     |
| Move Overhead    | integer |      10       |        [0, 50000]         | Amount of time Stormphrax assumes to be lost to overhead when making a move (in ms). |
| LowerMoveAverage |  check  |    `false`    |      `false`, `true`      | Does nothing.                                                                        |
| SyzygyPath       | string  |   `<empty>`   |  any path, or `<empty>`   | Location of Syzygy tablebases to probe during search.                                |
| SyzygyProbeDepth |  spin   |       1       |         [1, 255]          | Minimum depth to probe Syzygy tablebases at.                                         |
| SyzygyProbeLimit |  spin   |       7       |          [0, 7]           | Maximum number of pieces on the board to probe Syzygy tablebases with.               |
| EvalFile         | string  | `<internal>`  | any path, or `<internal>` | NNUE file to use for evaluation.                                                     |

## Builds
`avx512`: requires AVX-512 (Zen 4, Skylake-X)  
`avx2-bmi2`: requires BMI2 and AVX2 and assumes fast `pext` and `pdep` (i.e. no Bulldozer, Piledriver, Steamroller, Excavator, Zen 1, Zen+ or Zen 2)  
`avx2`: requires BMI and AVX2 - primarily useful for pre-Zen 3 AMD CPUs back to Excavator  
`sse41-popcnt`: needs SSE 4.1 and `popcnt` - for older x64 CPUs

Alternatively, build the makefile target `native` for a binary tuned for your specific CPU (see below)  

### Note:  
- If you have an AMD Zen 1 (Ryzen x 1xxx), Zen+ (Ryzen x 2xxx) or Zen 2 (Ryzen x 3xxx) CPU, use the `avx2` build even though your CPU supports BMI2. These CPUs implement the BMI2 instructions `pext` and `pdep` in microcode, which makes them unusably slow for Stormphrax's purposes.

## Building
Requires Make and a competent C++20 compiler that supports LTO. Currently, GCC produces *much* slower binaries than Clang.
```bash
> make <BUILD> CXX=<COMPILER>
```
- replace `<COMPILER>` with your preferred compiler - for example, `clang++` or `g++`
  - if not specified, the compiler defaults to `clang++`
- replace `<BUILD>` with the binary you wish to build - `native`/`avx512`/`avx2-bmi2`/`avx2`/`sse41-popcnt`
  - if not specified, the default build is `native`
- if you wish, you can have Stormphrax include the current git commit keys in its UCI version string - pass `COMMIT_HASH=on`

By default, the makefile builds binaries with profile-guided optimisation (PGO). To disable this, pass `PGO=off`. When using Clang with PGO enabled, `llvm-profdata` must be in your PATH.

## Credit
Stormphrax uses [Fathom](https://github.com/jdart1/Fathom) for tablebase probing, licensed under the MIT license, and a slightly modified version of [incbin](https://github.com/graphitemaster/incbin) for embedding neural network files, under the Unlicense.

Stormphrax is tested on [this OpenBench instance][ob] - thanks to all the people there, SP would be much weaker without your support :3

In no particular order, these engines have been notable sources of ideas or inspiration:
- [Viridithas][viri]
- [Svart][svart]
- [Stockfish][sf]
- [Ethereal][ethy]
- [Berserk][berky]
- [Weiss][weiss]
- [Koivisto][koi]

Stormphrax's current networks are trained with [bullet][bullet]. Previous networks were trained with [Marlinflow][marlinflow].

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
[cegt]: http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html
[mcerl]: https://www.chessengeria.eu/mcerl

[ob]: https://chess.swehosting.se/index/

[viri]: https://github.com/cosmobobak/viridithas
[svart]: https://github.com/crippa1337/svart
[sf]: https://github.com/official-stockfish/Stockfish
[ethy]: https://github.com/AndyGrant/Ethereal
[berky]: https://github.com/jhonnold/Berserk
[weiss]: https://github.com/TerjeKir/weiss
[koi]: https://github.com/Luecx/Koivisto

[bullet]: https://github.com/jw1912/bullet
[marlinflow]: https://github.com/jnlt3/marlinflow

[edge-chronicles]: https://en.wikipedia.org/wiki/The_Edge_Chronicles
