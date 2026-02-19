ifeq ($(OS), Windows_NT)
    DETECTED_OS := Windows
else
    DETECTED_OS := $(shell uname -s)
endif

ifeq ($(DETECTED_OS),Darwin)
VERSION := $(shell cat version.txt)
DEFAULT_NET := $(shell cat network.txt)
else
VERSION := $(file < version.txt)
DEFAULT_NET := $(file < network.txt)
endif

ifndef EXE
    EXE = stormphrax-$(VERSION)
    NO_EXE_SET = true
endif

ifndef EVALFILE
    EVALFILE = $(DEFAULT_NET).nnue
    NO_EVALFILE_SET = true
endif

PGO = off
COMMIT_HASH = off
DISABLE_NEON_DOTPROD = off
USE_LIBNUMA = off

SOURCES_COMMON := src/3rdparty/fmt/src/format.cc src/main.cpp src/core.cpp src/uci.cpp src/util/split.cpp src/move.cpp src/position/position.cpp src/movegen.cpp src/search.cpp src/util/timer.cpp src/ttable.cpp src/eval/nnue.cpp src/perft.cpp src/bench.cpp src/tunable.cpp src/opts.cpp src/3rdparty/pyrrhic/tbprobe.cpp src/datagen/datagen.cpp src/wdl.cpp src/cuckoo.cpp src/datagen/marlinformat.cpp src/datagen/viriformat.cpp src/datagen/fen.cpp src/tb.cpp src/3rdparty/zstd/zstddeclib.c src/eval/nnue/io_impl.cpp src/util/ctrlc.cpp src/stats.cpp src/thread.cpp src/limit.cpp src/util/numa/numa_fallback.cpp src/util/numa/numa_libnuma.cpp
SOURCES_BMI2 := src/attacks/bmi2/attacks.cpp
SOURCES_BLACK_MAGIC := src/attacks/black_magic/attacks.cpp

SUFFIX :=

CXX := clang++

# disable -Wunused-function and -Wunused-const-variable for zstd
CXXFLAGS := -Isrc/3rdparty/fmt/include -std=c++20 -flto -Wall -Wextra -Wno-sign-compare -Wno-unused-function -Wno-unused-const-variable -DSP_NETWORK_FILE=\"$(EVALFILE)\" -DSP_VERSION=$(VERSION)

CXXFLAGS_RELEASE := -O3 -DNDEBUG
CXXFLAGS_SANITIZER := -O1 -g -fsanitize=address,undefined

CXXFLAGS_NATIVE := -DSP_NATIVE -march=native
CXXFLAGS_TUNABLE := -DSP_NATIVE -march=native -DSP_EXTERNAL_TUNE=1
CXXFLAGS_VNNI512 := -DSP_VNNI512 -DSP_FAST_PEXT -march=znver5 -mtune=znver5
CXXFLAGS_AVX512 := -DSP_AVX512 -DSP_FAST_PEXT -march=icelake-client -mtune=znver4
CXXFLAGS_AVX2_BMI2 := -DSP_AVX2_BMI2 -DSP_FAST_PEXT -march=haswell -mtune=znver3
CXXFLAGS_AVX2 := -DSP_AVX2 -march=bdver4 -mno-tbm -mno-sse4a -mno-bmi2 -mtune=znver2

LDFLAGS :=

COMPILER_VERSION := $(shell $(CXX) --version)

ifeq (, $(findstring clang,$(COMPILER_VERSION)))
    ifeq (, $(findstring gcc,$(COMPILER_VERSION)))
        ifeq (, $(findstring g++,$(COMPILER_VERSION)))
            $(error Only Clang and GCC supported)
        endif
    endif
endif

ifeq ($(DETECTED_OS), Windows)
    SUFFIX := .exe
    # for fathom
    CXXFLAGS += -D_CRT_SECURE_NO_WARNINGS
    RM := del
else
    SUFFIX :=
    LDFLAGS += -pthread
    # don't ask
    ifdef IS_COSMO
        CXXFLAGS += -stdlib=libc++
    endif
    RM := rm
endif

ifneq (, $(findstring clang,$(COMPILER_VERSION)))
    ifneq ($(DETECTED_OS),Darwin)
        LDFLAGS += -fuse-ld=lld
    endif
    ifeq ($(DETECTED_OS),Windows)
        ifeq (,$(shell where llvm-profdata))
            $(warning llvm-profdata not found, disabling PGO)
            override PGO := off
        endif
    else
        ifeq (,$(shell which llvm-profdata))
            $(warning llvm-profdata not found, disabling PGO)
            override PGO := off
        endif
    endif
    PGO_GENERATE := -DSP_PGO_PROFILE -fprofile-instr-generate
    PGO_MERGE := llvm-profdata merge -output=sp.profdata *.profraw
    PGO_USE := -fprofile-instr-use=sp.profdata
else
    $(warning GCC currently produces very slow binaries for Stormphrax)
    PGO_GENERATE := -DSP_PGO_PROFILE -fprofile-generate
    PGO_MERGE :=
    PGO_USE := -fprofile-use
endif

ARCH_DEFINES := $(shell echo | $(CXX) -march=native -E -dM -)

ifneq ($(findstring __BMI2__, $(ARCH_DEFINES)),)
    ifeq ($(findstring __znver1, $(ARCH_DEFINES)),)
        ifeq ($(findstring __znver2, $(ARCH_DEFINES)),)
            ifeq ($(findstring __bdver, $(ARCH_DEFINES)),)
                CXXFLAGS_NATIVE += -DSP_FAST_PEXT
            endif
        endif
    endif
endif

ifneq ($(findstring __ARM_ARCH, $(ARCH_DEFINES)),)
    ifeq ($(DISABLE_NEON_DOTPROD),on)
        CXXFLAGS_NATIVE += -DSP_DISABLE_NEON_DOTPROD
    endif
endif

ifeq ($(COMMIT_HASH),on)
    CXXFLAGS += -DSP_COMMIT_HASH=$(shell git log -1 --pretty=format:%h)
endif

ifeq ($(USE_LIBNUMA),on)
	CXXFLAGS += -DSP_USE_LIBNUMA
	LDFLAGS += -lnuma
endif

PROFILE_OUT = sp_profile$(SUFFIX)

ifneq ($(PGO),on)
define build
    $(CXX) $(CXXFLAGS) $(CXXFLAGS_$1) $(CXXFLAGS_$2) $(LDFLAGS) -o $(EXE)$(if $(NO_EXE_SET),-$3)$(SUFFIX) $(filter-out $(EVALFILE),$^)
endef
else
define build
    $(CXX) $(CXXFLAGS) $(CXXFLAGS_$1) $(CXXFLAGS_$2) $(LDFLAGS) -o $(PROFILE_OUT) $(PGO_GENERATE) $(filter-out $(EVALFILE),$^)
    ./$(PROFILE_OUT) bench
    $(RM) $(PROFILE_OUT)
    $(PGO_MERGE)
    $(CXX) $(CXXFLAGS) $(CXXFLAGS_$1) $(CXXFLAGS_$2) $(LDFLAGS) -o $(EXE)$(if $(NO_EXE_SET),-$3)$(SUFFIX) $(PGO_USE) $(filter-out $(EVALFILE),$^)
    $(RM) *.profraw
    $(RM) sp.profdata
endef
endif

release: vnni512 avx512 avx2-bmi2 avx2
all: native release

.PHONY: all

.DEFAULT_GOAL := native

ifdef NO_EVALFILE_SET
$(EVALFILE):
	$(info Downloading default network $(DEFAULT_NET).nnue)
	curl -sOL https://github.com/Ciekce/stormphrax-nets/releases/download/$(DEFAULT_NET)/$(DEFAULT_NET).nnue

download-net: $(EVALFILE)
endif

$(EXE): $(EVALFILE) $(SOURCES_COMMON) $(SOURCES_BLACK_MAGIC) $(SOURCES_BMI2)
	$(call build,RELEASE,NATIVE,native)

native: $(EXE)

tunable: $(EVALFILE) $(SOURCES_COMMON) $(SOURCES_BLACK_MAGIC) $(SOURCES_BMI2)
	$(call build,RELEASE,TUNABLE,tunable)

vnni512: $(EVALFILE) $(SOURCES_COMMON) $(SOURCES_BMI2)
	$(call build,RELEASE,VNNI512,vnni512)

avx512: $(EVALFILE) $(SOURCES_COMMON) $(SOURCES_BMI2)
	$(call build,RELEASE,AVX512,avx512)

avx2-bmi2: $(EVALFILE) $(SOURCES_COMMON) $(SOURCES_BMI2)
	$(call build,RELEASE,AVX2_BMI2,avx2-bmi2)

avx2: $(EVALFILE) $(SOURCES_COMMON) $(SOURCES_BLACK_MAGIC)
	$(call build,RELEASE,AVX2,avx2)

sanitizer: $(EVALFILE) $(SOURCES_COMMON) $(SOURCES_BLACK_MAGIC) $(SOURCES_BMI2)
	$(call build,SANITIZER,NATIVE,native)

clean:

