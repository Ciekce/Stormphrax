ifeq ($(OS), Windows_NT)
    override DETECTED_OS := Windows
else
    override DETECTED_OS := $(shell uname -s)
endif

export DETECTED_OS

ifeq ($(DETECTED_OS),Darwin)
    override VERSION := $(shell cat version.txt)
else
    override VERSION := $(file < version.txt)
endif

export VERSION

ifndef EXE
    EXE = northphrax-$(VERSION)
    override NO_EXE_SET = true
    export NO_EXE_SET
endif

export EXE

ifeq ($(DETECTED_OS), Windows)
    override RMDIR := rmdir /s /q
else
    override RMDIR := rm -rf
endif

CC := clang
CXX := clang++

export CC
export CXX

releases: avx512 avx2-bmi2 zen2
all: native releases

.DEFAULT_GOAL := native

.PHONY: native
native:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: tunable
tunable:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: sanitizer
sanitizer:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: avx512
avx512:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: avx2-bmi2
avx2-bmi2:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: zen2
zen2:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: armv8-4
armv8-4:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: apple-m1
apple-m1:
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: bench
bench:
	$(MAKE) -f build.mk TYPE=native bench IS_CALLED_FROM_MAKEFILE=yea

.PHONY: format
format:
	$(MAKE) -f build.mk TYPE=native format IS_CALLED_FROM_MAKEFILE=yea

clean:
	$(RMDIR) tmp

