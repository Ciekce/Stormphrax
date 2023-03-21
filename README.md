<div align="center">

# Polaris

[![License](https://img.shields.io/github/license/Ciekce/Polaris?style=for-the-badge)](https://github.com/Ciekce/Polaris/blob/main/LICENSE)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/Ciekce/Polaris?style=for-the-badge)](https://github.com/Ciekce/Polaris/releases/latest)
[![Commits since latest release](https://img.shields.io/github/commits-since/Ciekce/Polaris/latest?style=for-the-badge)](https://github.com/Ciekce/Polaris/commits/main)

</div>

a UCI chess engine  
heavily WIP and currently pretty basic  

## Strength
| Version | [CCRL Blitz](https://www.computerchess.org.uk/ccrl/404/) | [CCRL 40/15](https://www.computerchess.org.uk/ccrl/4040/) |
|:-------:|:--------------------------------------------------------:|:---------------------------------------------------------:|
| 1.2.0   |                         2332                             |                           ~2300                           |


## Features
- standard PVS with aspiration windows, nullmove pruning etc
- Texel-tuned HCE (private tuner because that code hurts me to reread)
  - tuner based on Andrew Grant's [paper](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf)
  - tuned on the lichess-big3-resolved dataset
- BMI2 attacks in the `bmi2` build, otherwise fancy black magic
  - `pext`/`pdep` for rooks
  - `pext` for bishops

## To-do
- finish eval (king safety, hanging and pinned pieces)
- history
- lazy SMP
- better time management
- tune search constants
- contempt
- make it stronger uwu
- [Chess960](https://en.wikipedia.org/wiki/Fischer_random_chess) support

## Builds
`bmi2`: requires BMI2 and assumes fast `pext` and `pdep` (i.e. no Zen 1 and 2)  
`modern`: requires BMI (`blsi`, `blsr`, `tzcnt`) - primarily useful for pre-Zen 3 AMD CPUs back to Piledriver  
`popcnt`: just needs `popcnt`  
`compat`: should run on anything back to an original Core 2

Alternatively, build the CMake target `polaris-native` for a binary tuned for your specific CPU (see below)  
(note that this does *not* automatically disable `pext` and `pdep` for pre-Zen 3 AMD CPUs that implement them in microcode)

### Note:  
- If you have an AMD Zen 1 (Ryzen x 1xxx) or 2 (Ryzen x 2xxx) CPU, use the `modern` build even though your CPU supports BMI2. These CPUs implement the BMI2 instructions `pext` and `pdep` in microcode, which makes them unusably slow for Polaris' purposes. 
- Builds other than `bmi2` are untested and might crash on CPUs lacking newer instructions; I don't have older hardware to test them on.

## Building
Requires CMake and a competent C++20 compiler (tested with Clang 15 on Windows and GCC 11 on Ubuntu)
```bash
> cmake -DCMAKE_BUILD_TYPE=Release -S . -B build/
> cmake --build build/ --target polaris-<BUILD>
```
(replace `<TARGET>` with your preferred target - `native`/`bmi2`/`modern`/`popcnt`/`compat`)

If you have a pre-Zen 3 AMD Ryzen CPU (see the notes in Builds above) and want to build the `native` target, use these commands instead (the second is unchanged):
```bash
> cmake -DCMAKE_BUILD_TYPE=Release -DPS_FAST_PEXT=OFF -S . -B build/
> cmake --build build/ --target polaris-native
```
Disabling the CMake option `PS_FAST_PEXT` builds the non-BMI2 attack getters.
