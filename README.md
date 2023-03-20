# Polaris

Polaris is a UCI chess engine, a full rewrite of its predecessor (my first attempt at a chess engine, also named Polaris)  
heavily WIP and currently pretty basic


### features
- standard PVS with aspiration windows, nullmove pruning etc
- Texel-tuned HCE (private tuner because that code hurts me to reread)
  - tuner based on Andrew Grant's [paper](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf)
  - tuned on the lichess-big3-resolved dataset
- BMI2 (pext/pdep) attacks in the `bmi2` build, otherwise fancy black magic 

### todo
- finish eval (king safety, hanging and pinned pieces)
- history
- lazy smp
- better time management
- tune search constants
- contempt for funsies
- general cleanups ig, code quality's pretty scuffed
- make it stronger uwu
- nnue maybe? idk

### builds
`bmi2`: requires BMI2 and assumes fast pext and pdep (i.e. no zen1 and 2)  
`modern`: requires BMI (blsi, blsr, tzcnt) - primarily useful for pre-zen3 amd cpus back to piledriver  
`popcnt`: just needs popcnt  
`compat`: should run on anything back to an original core 2

alternatively build the cmake target `polaris-native` for a binary tuned for your specific CPU (see below)  
note that this does *not* automatically disable pext and pdep for pre-zen3 AMD CPUs that implement them in microcode, see the cmake option `PS_FAST_PEXT`

builds other than `bmi2` are untested and might crash on CPUs lacking newer instructions, I don't have older hardware to test them on  
`compat` is likely quite a bit weaker than the others due to heavy use of popcnt in evaluation

### building
requires a competent c++20 compiler (tested with clang 15 and gcc 11)
```bash
> cmake -DCMAKE_BUILD_TYPE=Release -S . -B build/
> cmake --build build/ --target polaris-<BUILD>
```
replace `<BUILD>` with your preferred build (`native`/`bmi2`/`modern`/`popcnt`/`compat`)
