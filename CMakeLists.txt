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
	add_compile_options(/EHsc /W4 /clang:-Wno-sign-compare /clang:-Wno-unused-function /clang:-Wno-unused-const-variable /WX /clang:-fconstexpr-steps=2097152)
	# for pyrrhic
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
else()
	add_compile_options(-Wall -Wextra -Wno-sign-compare -Wno-unused-function -Wno-unused-const-variable -Werror -fconstexpr-steps=2097152)
endif()

find_package(Threads REQUIRED)

option(SP_FAST_PEXT "whether pext and pdep are usably fast on this architecture, for building native binaries" ON)
option(SP_DISABLE_NEON_DOTPROD "whether to disable NEON dotprod on ARM machines" OFF)

add_executable(stormphrax-native src/types.h src/main.cpp src/uci.h src/uci.cpp src/core.h src/core.cpp src/util/bitfield.h
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
	src/eval/nnue/input.h src/util/memstream.h src/util/aligned_array.h src/eval/nnue/loader.h src/eval/nnue/features/psq.h
	src/datagen/format.h src/datagen/common.h src/datagen/marlinformat.h src/datagen/marlinformat.cpp
	src/datagen/viriformat.h src/datagen/viriformat.cpp src/tb.h src/tb.cpp src/movepick.h src/history.h
	src/util/multi_array.h src/correction.h src/util/simd/avx512.h src/util/simd/avx2.h
	src/util/simd/neon.h src/util/align.h src/3rdparty/zstd/zstddeclib.c
	src/eval/nnue/loader.cpp src/datagen/fen.h src/datagen/fen.cpp src/util/ctrlc.h src/util/ctrlc.cpp
	src/eval/nnue/arch/singlelayer.h src/eval/nnue/arch/multilayer.h src/stats.h src/stats.cpp
	src/3rdparty/fmt/src/format.cc src/eval/nnue/arch/util/sparse.h src/thread.cpp src/root_move.h src/pv.h src/limit.h
	src/limit.cpp src/util/numa/numa.h src/util/numa/numa_libnuma.cpp src/util/numa/numa_fallback.cpp
	src/eval/nnue/features/threats.h src/eval/nnue/features/threats.cpp src/attacks/bmi2/data.h
	src/attacks/bmi2/attacks.h src/attacks/bmi2/attacks.cpp src/attacks/black_magic/data.h
	src/attacks/black_magic/attacks.h src/attacks/black_magic/attacks.cpp
	src/eval/header.h)

target_include_directories(stormphrax-native PUBLIC src/3rdparty/fmt/include)
target_compile_options(stormphrax-native PUBLIC -march=native $<$<CONFIG:Release>:-flto>)
target_link_options(stormphrax-native PUBLIC -fuse-ld=lld)
target_link_libraries(stormphrax-native PUBLIC Threads::Threads)
target_compile_definitions(stormphrax-native PUBLIC SP_NATIVE SP_VERSION=${CMAKE_PROJECT_VERSION})

if(SP_EMBED_COMMIT_HASH)
	target_compile_definitions(stormphrax-native PUBLIC SP_COMMIT_HASH=${SP_COMMIT_HASH})
endif()

if(SP_FAST_PEXT)
	target_compile_definitions(stormphrax-native PUBLIC SP_FAST_PEXT)
endif()

if(SP_DISABLE_NEON_DOTPROD)
	target_compile_definitions(stormphrax-native PUBLIC SP_DISABLE_NEON_DOTPROD)
endif()

add_executable(permute-native preprocess/permute.cpp src/3rdparty/fmt/src/format.cc)
target_include_directories(permute-native PUBLIC src/3rdparty/fmt/include)
target_compile_definitions(permute-native PUBLIC SP_NATIVE)
target_compile_options(permute-native PUBLIC -march=native)

add_custom_command(
	COMMAND permute-native "${PROJECT_SOURCE_DIR}/${SP_DEFAULT_NET_NAME}.nnue" "${SP_DEFAULT_NET_NAME}.nnue_permuted"
	OUTPUT ${SP_DEFAULT_NET_NAME}.nnue_permuted
	DEPENDS permute-native
	COMMENT "Prepermuting network"
)

add_custom_target(permuted_net DEPENDS ${SP_DEFAULT_NET_NAME}.nnue_permuted)
add_dependencies(stormphrax-native permuted_net)
target_compile_definitions(stormphrax-native PUBLIC SP_NETWORK_FILE="${SP_DEFAULT_NET_NAME}.nnue_permuted")
