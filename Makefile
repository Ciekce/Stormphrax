# THIS MAKEFILE IS ONLY INTENDED FOR OPENBENCH
# BUILD WITH CMAKE PER THE INSTRUCTIONS IN THE README

EXE = stormphrax_default
EVALFILE = src/eval/net006.nnue

SOURCES := src/main.cpp src/uci.cpp src/util/split.cpp src/hash.cpp src/position/position.cpp src/movegen.cpp src/attacks/black_magic/attacks.cpp src/search.cpp src/util/timer.cpp src/pretty.cpp src/ttable.cpp src/limit/time.cpp src/eval/eval.cpp src/perft.cpp src/bench.cpp src/opts.cpp src/3rdparty/fathom/tbprobe.cpp src/datagen.cpp

SUFFIX :=

CXX := clang++
# silence warning for fathom
CXXFLAGS := -std=c++20 -O3 -flto -march=native -DNDEBUG -DSP_NATIVE -DSP_NETWORK_FILE=\"$(EVALFILE)\" -DSP_VERSION=OpenBench -D_SILENCE_CXX20_ATOMIC_INIT_DEPRECATION_WARNING

LDFLAGS :=

ifeq ($(EXE), stormphrax_default)
    $(warning If you are compiling Stormphrax using this makefile, you probably did not read the build instructions.)
endif

COMPILER_VERSION := $(shell $(CXX) --version)

ifeq (, $(findstring clang,$(COMPILER_VERSION)))
    ifeq (, $(findstring gcc,$(COMPILER_VERSION)))
        ifeq (, $(findstring g++,$(COMPILER_VERSION)))
            $(error Only Clang and GCC supported)
        endif
    endif
endif

ifeq ($(OS), Windows_NT)
    DETECTED_OS := Windows
    SUFFIX := .exe
    # for fathom
    CXXFLAGS += -D_CRT_SECURE_NO_WARNINGS
else
    DETECTED_OS := $(shell uname -s)
    SUFFIX :=
    LDFLAGS += -pthread
    # don't ask
    ifdef IS_COSMO
        CXXFLAGS += -stdlib=libc++
    endif
endif

ifneq (, $(findstring clang,$(COMPILER_VERSION)))
    ifneq ($(DETECTED_OS),Darwin)
        LDFLAGS += -fuse-ld=lld
    endif
endif

OUT := $(EXE)$(SUFFIX)

all: $(EXE)

$(EXE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(OUT) $^

clean:

