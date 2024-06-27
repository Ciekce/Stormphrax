<div align="center">

# Stormphrax

[![License][license-badge]][license-link]  
[![GitHub release (latest by date)][release-badge]][release-link]
[![Commits since latest release][commits-badge]][commits-link]

![LGBTQ+ friendly][lgbtqp-badge]
![trans rights][trans-rights-badge]

</div>

a work-in-progress UCI chess and [chess960] engine, with NNUE evaluation trained from zero knowledge starting with random weights

this project is a continuation of my HCE engine [Polaris]

## Strength
| Version | [CCRL 40/15][ccrl-4015] | [CCRL Blitz][ccrl-blitz] | [CCRL 40/2 FRC][ccrl-402-frc] | [CEGT 40/4][cegt] | [MCERL] |
|:-------:|:-----------------------:|:------------------------:|:-----------------------------:|:-----------------:|:-------:|
|  5.0.0  |            -            |            -             |               -               |         -         |    -    |
|  4.1.0  |          3484           |            -             |             3802              |         -         |    -    |
|  4.0.0  |          3476           |           3566           |             3775              |       3440        |  3542   |
|  3.0.0  |          3408           |           3491           |             3691              |         -         |  3495   |
|  2.0.0  |          3399           |           3481           |             3671              |       3339        |  3482   |
|  1.0.0  |          3318           |           3374           |             3540              |       3235        |  3346   |

## Features
- standard PVS with quiescence search and iterative deepening
  - aspiration windows
  - futility pruning
  - history
    - capture history
    - 1-ply continuation history (countermove history)
    - 2-ply continuation history (follow-up history)
    - 4-ply continuation history
  - history pruning
  - internal iterative reduction
  - killers (2 per ply)
  - late move reductions
  - late move pruning
  - mate distance pruning
  - multicut
  - nullmove pruning
  - reverse futility pruning
  - probcut
  - SEE move ordering and pruning
  - singular extensions
    - double extensions
    - triple extensions
    - various negative extensions
  - Syzygy tablebase support
- NNUE
  - (768x4->1536)x2->1x8 architecture, horizontally mirrored
  - trained from zero knowledge with reinforcement learning from a randomly-initialised network
- BMI2 attacks in the `bmi2` build, otherwise fancy black magic
  - `pext`/`pdep` for rooks
  - `pext` for bishops
- lazy SMP
- static contempt

## To-do
- make it stronger uwu

## UCI options
| Name               |  Type   | Default value |       Valid values        | Description                                                                                                                                                                                                                              |
|:-------------------|:-------:|:-------------:|:-------------------------:|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Hash`             | integer |      64       |        [1, 131072]        | Memory allocated to the transposition table (in MB).                                                                                                                                                                                     |
| `Clear Hash`       | button  |      N/A      |            N/A            | Clears the transposition table.                                                                                                                                                                                                          |
| `Threads`          | integer |       1       |         [1, 2048]         | Number of threads used to search.                                                                                                                                                                                                        |
| `UCI_Chess960`     |  check  |    `false`    |      `false`, `true`      | Whether Stormphrax plays Chess960 instead of standard chess.                                                                                                                                                                             |
| `UCI_ShowWDL`      |  check  |    `true`     |      `false`, `true`      | Whether Stormphrax displays predicted win/draw/loss probabilities in UCI output.                                                                                                                                                         |
| `ShowCurrMove`     |  check  |    `false`    |      `false`, `true`      | Whether Stormphrax starts printing the move currently being searched after a short delay.                                                                                                                                                |
| `Move Overhead`    | integer |      10       |        [0, 50000]         | Amount of time Stormphrax assumes to be lost to overhead when making a move (in ms).                                                                                                                                                     |
| `EnableWeirdTCs`   |  check  |    `false`    |      `false`, `true`      | Whether unusual time controls (movestogo != 0, or increment = 0) are enabled. Enabling this option means you recognise that Stormphrax is neither designed for nor tested with these TCs, and is likely to perform worse than under X+Y. |
| `SyzygyPath`       | string  |   `<empty>`   |  any path, or `<empty>`   | Location of Syzygy tablebases to probe during search.                                                                                                                                                                                    |
| `SyzygyProbeDepth` |  spin   |       1       |         [1, 255]          | Minimum depth to probe Syzygy tablebases at.                                                                                                                                                                                             |
| `SyzygyProbeLimit` |  spin   |       7       |          [0, 7]           | Maximum number of pieces on the board to probe Syzygy tablebases with.                                                                                                                                                                   |
| `EvalFile`         | string  | `<internal>`  | any path, or `<internal>` | NNUE file to use for evaluation.                                                                                                                                                                                                         |

## Builds
`vnni512`: requires BMI2, AVX-512 and VNNI (Zen 4/Cascade Lake-SP/Rocket Lake and up)  
`avx512`: requires BMI2 and AVX-512 (Skylake-X, Cannon Lake)  
`avx2-bmi2`: requires BMI2 and AVX2 and assumes fast `pext` and `pdep` (i.e. no Bulldozer, Piledriver, Steamroller, Excavator, Zen 1, Zen+ or Zen 2)  
`avx2`: requires BMI and AVX2 - primarily useful for pre-Zen 3 AMD CPUs back to Excavator  
`sse41-popcnt`: needs SSE 4.1 and `popcnt` - for older x64 CPUs

If in doubt, compare the `avx2-bmi2` and `avx2` binaries and pick the one that's faster. BMI2 will always be faster on Intel CPUs.

Alternatively, build the makefile target `native` for a binary tuned for your specific CPU (see below)  

### Note:  
- If you have an AMD Zen 1 (Ryzen x 1xxx), Zen+ (Ryzen x 2xxx) or Zen 2 (Ryzen x 3xxx) CPU, use the `avx2` build even though your CPU supports BMI2. These CPUs implement the BMI2 instructions `pext` and `pdep` in microcode, which makes them unusably slow for Stormphrax's purposes.

## Building
Requires Make and a competent C++20 compiler that supports LTO. GCC is not currently supported, so the usual compiler is Clang. MSVC explicitly does not work.
```bash
> make <BUILD> CXX=<COMPILER>
```
- replace `<COMPILER>` with your preferred compiler - for example, `clang++` or `icpx`
  - if not specified, the compiler defaults to `clang++`
- replace `<BUILD>` with the binary you wish to build - `native`/`vnni512`/`avx512`/`avx2-bmi2`/`avx2`/`sse41-popcnt`
  - if not specified, the default build is `native`
- if you wish, you can have Stormphrax include the current git commit hash in its UCI version string - pass `COMMIT_HASH=on`

By default, the makefile builds binaries with profile-guided optimisation (PGO). To disable this, pass `PGO=off`. When using Clang with PGO enabled, `llvm-profdata` must be in your PATH.

## Credit
Stormphrax uses [Fathom] for tablebase probing, licensed under the MIT license, and a slightly modified version of [incbin] for embedding neural network files, under the Unlicense.

Stormphrax is tested on [this OpenBench instance][ob] - thanks to all the people there, SP would be much weaker without your support :3

In no particular order, these engines have been notable sources of ideas or inspiration:
- [Viridithas]
- [Svart]
- [Stockfish]
- [Ethereal]
- [Berserk]
- [Weiss]
- [Koivisto]

Stormphrax's current networks are trained with [bullet]. Previous networks were trained with [Marlinflow].

The name "Stormphrax" is a reference to the excellent [Edge Chronicles][edge-chronicles] :)

[license-badge]: https://img.shields.io/github/license/Ciekce/Stormphrax?style=for-the-badge
[release-badge]: https://img.shields.io/github/v/release/Ciekce/Stormphrax?style=for-the-badge
[commits-badge]: https://img.shields.io/github/commits-since/Ciekce/Stormphrax/latest?style=for-the-badge

[license-link]: https://github.com/Ciekce/Stormphrax/blob/main/LICENSE
[release-link]: https://github.com/Ciekce/Stormphrax/releases/latest
[commits-link]: https://github.com/Ciekce/Stormphrax/commits/main

[lgbtqp-badge]: https://pride-badges.pony.workers.dev/static/v1?label=lgbtq%2B%20friendly&stripeWidth=6&stripeColors=E40303,FF8C00,FFED00,008026,24408E,732982
[trans-rights-badge]: https://pride-badges.pony.workers.dev/static/v1?label=trans%20rights&stripeWidth=6&stripeColors=5BCEFA,F5A9B8,FFFFFF,F5A9B8,5BCEFA

[chess960]: https://en.wikipedia.org/wiki/Fischer_random_chess

[polaris]: https://github.com/Ciekce/Polaris

[ccrl-4015]: https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list
[ccrl-blitz]: https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list
[ccrl-402-frc]: https://www.computerchess.org.uk/ccrl/404FRC/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list
[cegt]: http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html
[mcerl]: https://www.chessengeria.eu/mcerl

[fathom]: https://github.com/jdart1/Fathom
[incbin]: https://github.com/graphitemaster/incbin

[ob]: https://chess.swehosting.se/index/

[viridithas]: https://github.com/cosmobobak/viridithas
[svart]: https://github.com/crippa1337/svart
[stockfish]: https://github.com/official-stockfish/Stockfish
[ethereal]: https://github.com/AndyGrant/Ethereal
[berserk]: https://github.com/jhonnold/Berserk
[weiss]: https://github.com/TerjeKir/weiss
[koivisto]: https://github.com/Luecx/Koivisto

[bullet]: https://github.com/jw1912/bullet
[marlinflow]: https://github.com/jnlt3/marlinflow

[edge-chronicles]: https://en.wikipedia.org/wiki/The_Edge_Chronicles
