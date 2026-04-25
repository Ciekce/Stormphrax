ifneq ($(IS_CALLED_FROM_MAKEFILE), yea)
    $(error Use Makefile, rather than build.mk!)
endif

TYPE = native

COMMIT_HASH = off
DISABLE_NEON_DOTPROD = off
USE_LIBNUMA = off

# https://stackoverflow.com/a/1825832
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SOURCES_3RDPARTY := 3rdparty/fmt/src/format.cc 3rdparty/pyrrhic/tbprobe.cpp 3rdparty/zstd/zstddeclib.c
SOURCES_PERMUTE := preprocess/permute.cpp 3rdparty/fmt/src/format.cc

HEADERS := $(call rwildcard,src,*.h)
SOURCES := $(call rwildcard,src,*.cpp)

# Sources including 3rdparty
SOURCES_ALL := $(SOURCES) $(SOURCES_3RDPARTY)

CFLAGS := -std=c11
CXXFLAGS := -std=c++20 -fconstexpr-steps=2097152

# disable -Wunused-function and -Wunused-const-variable for zstd
FLAGS := -I3rdparty/fmt/include -flto -Wall -Wextra -Wno-sign-compare -Wno-unused-function -Wno-unused-const-variable -DSP_VERSION=$(VERSION)

FLAGS_RELEASE := -O3 -DNDEBUG
FLAGS_SANITIZER := -O1 -g -fsanitize=address,undefined

FLAGS_NATIVE := -DSP_NATIVE -march=native
FLAGS_TUNABLE := -DSP_NATIVE -march=native -DSP_EXTERNAL_TUNE=1
FLAGS_AVX512 := -DSP_AVX512 -DSP_FAST_PEXT -march=icelake-client -mtune=znver4
FLAGS_AVX2_BMI2 := -DSP_AVX2_BMI2 -DSP_FAST_PEXT -march=haswell -mtune=znver3
FLAGS_ZEN2 := -DSP_ZEN2 -march=bdver4 -mno-tbm -mno-sse4a -mtune=znver2
FLAGS_ARMV8_4 := -DSP_ARMV8_4 -march=armv8.4-a

ifdef NO_EXE_SET
    EXE := $(EXE)-$(TYPE)
endif

LDFLAGS :=

CC_VERSION := $(shell $(CC) --version)
ifeq (, $(findstring clang, $(CC_VERSION)))
	$(error Only Clang supported)
endif

CXX_VERSION := $(shell $(CXX) --version)
ifeq (, $(findstring clang, $(CXX_VERSION)))
	$(error Only Clang supported)
endif

MKDIR := mkdir -p

ifeq ($(DETECTED_OS), Windows)
    SUFFIX := .exe
    RM := del
    ifeq (.exe,$(findstring .exe,$(SHELL)))
        MKDIR := -mkdir
    endif
else
    SUFFIX :=
    RM := rm
    LDFLAGS += -pthread
endif

ifneq ($(DETECTED_OS), Darwin)
	LDFLAGS += -fuse-ld=lld
endif

ARCH_DEFINES := $(shell echo | $(CXX) -march=native -E -dM -)

ifneq ($(findstring __BMI2__, $(ARCH_DEFINES)),)
    ifeq ($(findstring __znver1, $(ARCH_DEFINES)),)
        ifeq ($(findstring __znver2, $(ARCH_DEFINES)),)
            ifeq ($(findstring __bdver, $(ARCH_DEFINES)),)
                FLAGS_NATIVE += -DSP_FAST_PEXT
            endif
        endif
    endif
endif

ifeq ($(COMMIT_HASH),on)
    FLAGS += -DSP_COMMIT_HASH=$(shell git log -1 --pretty=format:%h)
endif

ifeq ($(USE_LIBNUMA),on)
	FLAGS += -DSP_USE_LIBNUMA
	LDFLAGS += -lnuma
endif

OUTFILE = $(subst .exe,,$(EXE))$(SUFFIX)

ifeq ($(TYPE), native)
    FLAGS += $(FLAGS_NATIVE) $(FLAGS_RELEASE)
else ifeq ($(TYPE), tunable)
    FLAGS += $(FLAGS_TUNABLE) $(FLAGS_RELEASE)
else ifeq ($(TYPE), sanitizer)
    FLAGS += $(FLAGS_NATIVE) $(FLAGS_SANITIZER)
else ifeq ($(TYPE), avx512)
    FLAGS += $(FLAGS_AVX512) $(FLAGS_RELEASE)
else ifeq ($(TYPE), avx2-bmi2)
    FLAGS += $(FLAGS_AVX2_BMI2) $(FLAGS_RELEASE)
else ifeq ($(TYPE), zen2)
    FLAGS += $(FLAGS_ZEN2) $(FLAGS_RELEASE)
else ifeq ($(TYPE), armv8-4)
    FLAGS += $(FLAGS_ARMV8_4) $(FLAGS_RELEASE)
else
    $(error Unknown build type)
endif

CFLAGS += $(FLAGS)
CXXFLAGS += $(FLAGS)

BUILD_DIR := build-$(TYPE)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(filter %.o,$(SOURCES_ALL:.c=.o) $(SOURCES_ALL:.cpp=.o) $(SOURCES_ALL:.cc=.o)))

define create_mkdir_target
$1:
	$(MKDIR) "$1"

.PRECIOUS: $1
endef

$(foreach dir,$(sort $(dir $(OBJECTS))),$(eval $(call create_mkdir_target,$(dir))))

tmp:
	$(MKDIR) tmp

EVALFILE_NAME := $(notdir $(EVALFILE))

.DEFAULT_GOAL := $(OUTFILE)

.SECONDEXPANSION:

tmp/permute-$(TYPE): tmp $(SOURCES_PERMUTE)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o tmp/permute-$(TYPE) $(filter-out $<,$^)

tmp/$(EVALFILE_NAME)_permuted_$(TYPE): $(EVALFILE) tmp/permute-$(TYPE)
	tmp/permute-$(TYPE) $< $@

$(BUILD_DIR)/%.o: %.c version.txt tmp/$(EVALFILE_NAME)_permuted_$(TYPE) | $$(@D)/
	$(CC) $(CFLAGS) -DSP_NETWORK_FILE=\"tmp/$(EVALFILE_NAME)_permuted_$(TYPE)\" -c -o $@ $<

$(BUILD_DIR)/%.o: %.cpp version.txt tmp/$(EVALFILE_NAME)_permuted_$(TYPE) | $$(@D)/
	$(CXX) $(CXXFLAGS) -DSP_NETWORK_FILE=\"tmp/$(EVALFILE_NAME)_permuted_$(TYPE)\" -c -o $@ $<

$(BUILD_DIR)/%.o: %.cc version.txt tmp/$(EVALFILE_NAME)_permuted_$(TYPE) | $$(@D)/
	$(CXX) $(CXXFLAGS) -DSP_NETWORK_FILE=\"tmp/$(EVALFILE_NAME)_permuted_$(TYPE)\" -c -o $@ $<

$(OUTFILE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(OUTFILE) $(OBJECTS)

bench: $(OUTFILE)
	./$(OUTFILE) bench

format: $(HEADERS) $(SOURCES) preprocess/permute.cpp
	clang-format -i $^

