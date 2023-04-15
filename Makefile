# THIS MAKEFILE IS ONLY INTENDED FOR OPENBENCH
# BUILD WITH CMAKE PER THE INSTRUCTIONS IN THE README

EXE = polaris

SOURCES := src/main.cpp src/uci.cpp src/util/split.cpp src/hash.cpp src/position/position.cpp src/eval/material.cpp src/movegen.cpp src/attacks/black_magic/attacks.cpp src/search/search.cpp src/search/pvs/pvs_search.cpp src/util/timer.cpp src/pretty.cpp src/ttable.cpp src/limit/time.cpp src/eval/eval.cpp src/perft.cpp src/bench.cpp

SUFFIX :=

CXX := clang++
CXXFLAGS := -std=c++20 -O3 -flto -march=native -DNDEBUG -DPS_NATIVE -DPS_VERSION=$(shell git rev-parse --short HEAD)

LDFLAGS :=

ifeq ($(OS), Windows_NT)
	SUFFIX := .exe
	LDFLAGS += -fuse-ld=lld-link
else
	LDFLAGS += -lpthread
endif

OUT := $(EXE)$(SUFFIX)

all: $(EXE)

$(EXE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(OUT) $^

clean:

