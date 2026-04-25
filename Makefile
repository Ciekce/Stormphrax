ifeq ($(OS), Windows_NT)
    override DETECTED_OS := Windows
else
    override DETECTED_OS := $(shell uname -s)
endif

export DETECTED_OS

ifeq ($(DETECTED_OS),Darwin)
    override VERSION := $(shell cat version.txt)
    override DEFAULT_NET := $(shell cat network.txt)
else
    override VERSION := $(file < version.txt)
    override DEFAULT_NET := $(file < network.txt)
endif

export VERSION

ifndef EXE
    EXE = stormphrax-$(VERSION)
    override NO_EXE_SET = true
    export NO_EXE_SET
endif

export EXE

ifndef EVALFILE
    EVALFILE = $(DEFAULT_NET).nnue
    override NO_EVALFILE_SET = true
endif

export EVALFILE

ifeq ($(DETECTED_OS), Windows)
    override RMDIR := rmdir /s /q
else
    override RMDIR := rm -rf
endif

releases: avx512 avx2-bmi2 zen2
all: native releases

.DEFAULT_GOAL := native

ifdef NO_EVALFILE_SET
$(EVALFILE):
	$(info Downloading default network $(DEFAULT_NET).nnue)
	curl -sOL https://github.com/Ciekce/stormphrax-nets/releases/download/$(DEFAULT_NET)/$(DEFAULT_NET).nnue

download-net: $(EVALFILE)
endif

.PHONY: native
native: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: tunable
tunable: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: sanitizer
sanitizer: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: avx512
avx512: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: avx2-bmi2
avx2-bmi2: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: zen2
zen2: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: armv8-4
armv8-4: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=$@ IS_CALLED_FROM_MAKEFILE=yea

.PHONY: bench
bench: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=native bench IS_CALLED_FROM_MAKEFILE=yea

.PHONY: format
format: $(EVALFILE)
	$(MAKE) -f build.mk TYPE=native format IS_CALLED_FROM_MAKEFILE=yea

clean:
	$(RMDIR) tmp

