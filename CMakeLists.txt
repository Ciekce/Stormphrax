cmake_minimum_required(VERSION 3.15)

file(STRINGS version.txt SP_VERSION LIMIT_COUNT 1)
project(stormphrax VERSION ${SP_VERSION})

set(CMAKE_CXX_STANDARD 20)

file(STRINGS network.txt SP_DEFAULT_NET_NAME LIMIT_COUNT 1)

set(SP_CLANG CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

if(MSVC AND NOT SP_CLANG)
	message(FATAL_ERROR Stormphrax does not support building with MSVC)
endif()

option(SP_EMBED_COMMIT_HASH "whether to include the current git commit in the UCI version string" ON)

if(SP_EMBED_COMMIT_HASH)
	set(SP_COMMIT_HASH "unknown")
	find_package(Git QUIET)
	if(GIT_FOUND)
		execute_process(
			COMMAND git log -1 --pretty=format:%h
			OUTPUT_VARIABLE SP_COMMIT_HASH
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
	endif()
endif()

# disable -Wunused-function and -Wunused-const-variable for zstd
if(MSVC)
	add_compile_options(/EHsc /W4 /clang:-Wno-sign-compare /clang:-Wno-unused-function /clang:-Wno-unused-const-variable /WX)
	# for pyrrhic
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
else()
	add_compile_options(-Wall -Wextra -Wno-sign-compare -Wno-unused-function -Wno-unused-const-variable -Werror)
endif()

find_package(Threads REQUIRED)

# cmake forces thin lto on clang for CMAKE_INTERPROCEDURAL_OPTIMIZATION, thanks cmake
add_compile_options($<$<CONFIG:Release>:-flto>)

option(SP_FAST_PEXT "whether pext and pdep are usably fast on this architecture, for building native binaries" ON)
option(SP_DISABLE_AVX512 "whether to disable AVX-512 (zen 4)" OFF)
option(SP_DISABLE_NEON_DOTPROD "whether to disable NEON dotprod on ARM machines" OFF)

set(STORMPHRAX_COMMON_SRC src/types.h src/main.cpp src/uci.h src/uci.cpp src/core.h src/core.cpp src/util/bitfield.h
	src/util/bits.h src/util/parse.h src/util/split.h src/util/split.cpp src/util/rng.h src/util/static_vector.h
	src/bitboard.h src/move.h src/move.cpp src/keys.h src/position/position.h src/position/position.cpp src/search.h
	src/search.cpp src/movegen.h src/movegen.cpp src/attacks/util.h src/attacks/attacks.h src/util/timer.h
	src/util/timer.cpp src/rays.h src/ttable.h src/ttable.cpp
	src/util/cemath.h src/eval/nnue.h src/eval/nnue.cpp src/util/range.h src/arch.h src/perft.h
	src/perft.cpp src/thread.h src/see.h src/bench.h src/bench.cpp src/tunable.h src/tunable.cpp src/opts.h
	src/opts.cpp src/position/boards.h src/3rdparty/pyrrhic/stdendian.h src/3rdparty/pyrrhic/tbconfig.h
	src/3rdparty/pyrrhic/tbprobe.h src/3rdparty/pyrrhic/tbprobe.cpp src/datagen/datagen.h src/datagen/datagen.cpp
	src/util/u4array.h src/eval/eval.h src/util/barrier.h src/util/simd.h src/wdl.h src/wdl.cpp src/eval/arch.h
	src/cuckoo.h src/cuckoo.cpp src/eval/nnue/network.h src/eval/nnue/activation.h src/eval/nnue/output.h
	src/eval/nnue/input.h src/util/memstream.h src/util/aligned_array.h src/eval/nnue/io.h src/eval/nnue/features.h
	src/datagen/format.h src/datagen/common.h src/datagen/marlinformat.h src/datagen/marlinformat.cpp
	src/datagen/viriformat.h src/datagen/viriformat.cpp src/tb.h src/tb.cpp src/movepick.h src/history.h
	src/util/multi_array.h src/correction.h src/util/simd/avx512.h src/util/simd/avx2.h
	src/util/simd/neon.h src/util/align.h src/3rdparty/zstd/zstddeclib.c src/eval/nnue/io_impl.h
	src/eval/nnue/io_impl.cpp src/datagen/fen.h src/datagen/fen.cpp src/util/ctrlc.h src/util/ctrlc.cpp
	src/eval/nnue/arch/singlelayer.h src/eval/nnue/arch/multilayer.h src/stats.h src/stats.cpp
	src/3rdparty/fmt/src/format.cc src/eval/nnue/arch/util/sparse.h src/thread.cpp src/root_move.h src/pv.h src/limit.h
	src/limit.cpp)

set(STORMPHRAX_BMI2_SRC src/attacks/bmi2/data.h src/attacks/bmi2/attacks.h src/attacks/bmi2/attacks.cpp)
set(STORMPHRAX_NON_BMI2_SRC src/attacks/black_magic/data.h src/attacks/black_magic/attacks.h
	src/attacks/black_magic/attacks.cpp)

add_executable(stormphrax-native ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_BMI2_SRC} ${STORMPHRAX_NON_BMI2_SRC})
add_executable(stormphrax-vnni512 ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_BMI2_SRC})
add_executable(stormphrax-avx512 ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_BMI2_SRC})
add_executable(stormphrax-avx2-bmi2 ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_BMI2_SRC})
add_executable(stormphrax-avx2 ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_NON_BMI2_SRC})

target_compile_options(stormphrax-native PUBLIC -march=native)
target_compile_options(stormphrax-vnni512 PUBLIC -march=znver5)
target_compile_options(stormphrax-avx512 PUBLIC -march=skylake-avx512)
target_compile_options(stormphrax-avx2-bmi2 PUBLIC -march=haswell)
# excavator without amd-specific extensions and bmi2
target_compile_options(stormphrax-avx2 PUBLIC -march=bdver4 -mno-tbm -mno-sse4a -mno-bmi2)

if(NOT MSVC)
	target_compile_options(stormphrax-native PUBLIC -mtune=native)
	target_compile_options(stormphrax-vnni512 PUBLIC -mtune=znver5)
	target_compile_options(stormphrax-avx512 PUBLIC -mtune=znver4)
	target_compile_options(stormphrax-avx2-bmi2 PUBLIC -mtune=haswell)
	target_compile_options(stormphrax-avx2 PUBLIC -mtune=znver2) # zen 2
else() # clang
	target_compile_options(stormphrax-native PUBLIC /tune:native)
	target_compile_options(stormphrax-vnni512 PUBLIC /tune:znver5)
	target_compile_options(stormphrax-avx512 PUBLIC /tune:znver4)
	target_compile_options(stormphrax-avx2-bmi2 PUBLIC /tune:znver3)
	target_compile_options(stormphrax-avx2 PUBLIC /tune:znver2) # zen 2
endif()

if(SP_FAST_PEXT)
	target_compile_definitions(stormphrax-native PUBLIC SP_FAST_PEXT)
endif()

if(SP_DISABLE_AVX512)
	target_compile_definitions(stormphrax-native PUBLIC SP_DISABLE_AVX512)
endif()

if(SP_DISABLE_NEON_DOTPROD)
	target_compile_definitions(stormphrax-native PUBLIC SP_DISABLE_NEON_DOTPROD)
endif()

get_directory_property(TARGETS BUILDSYSTEM_TARGETS)

foreach(TARGET ${TARGETS})
	string(REPLACE "stormphrax-" "" ARCH_NAME "${TARGET}")
	string(REPLACE "-" "_" ARCH_NAME "${ARCH_NAME}")
	string(TOUPPER ${ARCH_NAME} ARCH_NAME)

	target_compile_definitions(${TARGET} PUBLIC SP_VERSION=${CMAKE_PROJECT_VERSION}
		SP_${ARCH_NAME} SP_NETWORK_FILE="${PROJECT_SOURCE_DIR}/${SP_DEFAULT_NET_NAME}.nnue")

	target_include_directories(${TARGET} PUBLIC src/3rdparty/fmt/include)

	string(REPLACE "stormphrax-" "stormphrax-${CMAKE_PROJECT_VERSION}-" TARGET_NAME "${TARGET}")
	set_property(TARGET ${TARGET} PROPERTY OUTPUT_NAME "${TARGET_NAME}")

	if(SP_EMBED_COMMIT_HASH)
		target_compile_definitions(${TARGET} PUBLIC SP_COMMIT_HASH=${SP_COMMIT_HASH})
	endif()

	target_link_libraries(${TARGET} Threads::Threads)
endforeach()
