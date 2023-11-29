cmake_minimum_required(VERSION 3.15)

project(stormphrax VERSION 3.0.24)

set(CMAKE_CXX_STANDARD 20)

set(SP_DEFAULT_NET_NAME net023)

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

# for fathom
add_compile_definitions(_SILENCE_CXX20_ATOMIC_INIT_DEPRECATION_WARNING)

if(MSVC)
	add_compile_options(/EHsc)
	# for fathom
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
endif()

find_package(Threads REQUIRED)

# cmake forces thin lto on clang for CMAKE_INTERPROCEDURAL_OPTIMIZATION, thanks cmake
add_compile_options($<$<CONFIG:Release>:-flto>)

option(SP_FAST_PEXT "whether pext and pdep are usably fast on this architecture, for building native binaries" ON)

set(STORMPHRAX_COMMON_SRC src/types.h src/main.cpp src/uci.h src/uci.cpp src/core.h src/util/bitfield.h src/util/bits.h
	src/util/parse.h src/util/split.h src/util/split.cpp src/util/rng.h src/util/static_vector.h src/bitboard.h
	src/move.h src/hash.h src/position/position.h src/position/position.cpp src/search.h src/search.cpp src/movegen.h
	src/movegen.cpp src/attacks/util.h src/attacks/attacks.h src/util/timer.h src/util/timer.cpp src/pretty.h
	src/pretty.cpp src/rays.h src/ttable.h src/ttable.cpp src/limit/limit.h src/limit/trivial.h src/limit/time.h
	src/limit/time.cpp src/util/cemath.h src/eval/nnue.h src/eval/nnue.cpp src/util/range.h src/arch.h src/perft.h
	src/perft.cpp src/search_fwd.h src/see.h src/bench.h src/bench.cpp src/tunable.h src/tunable.cpp src/opts.h
	src/opts.cpp src/position/boards.h src/history.h src/3rdparty/fathom/stdendian.h src/3rdparty/fathom/tbconfig.h
	src/3rdparty/fathom/tbprobe.h src/3rdparty/fathom/tbprobe.cpp src/datagen.h src/datagen.cpp src/eval/activation.h
	src/util/u4array.h src/eval/eval.h src/util/barrier.h src/util/simd.h src/wdl.h src/wdl.cpp src/eval/arch.h)

set(STORMPHRAX_BMI2_SRC src/attacks/bmi2/data.h src/attacks/bmi2/attacks.h src/attacks/bmi2/attacks.cpp)
set(STORMPHRAX_NON_BMI2_SRC src/attacks/black_magic/data.h src/attacks/black_magic/attacks.h
	src/attacks/black_magic/attacks.cpp)

add_executable(stormphrax-native ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_BMI2_SRC} ${STORMPHRAX_NON_BMI2_SRC})
add_executable(stormphrax-avx512 ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_BMI2_SRC})
add_executable(stormphrax-avx2-bmi2 ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_BMI2_SRC})
add_executable(stormphrax-avx2 ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_NON_BMI2_SRC})
add_executable(stormphrax-sse41-popcnt ${STORMPHRAX_COMMON_SRC} ${STORMPHRAX_NON_BMI2_SRC})

target_compile_options(stormphrax-native PUBLIC -march=native)
target_compile_options(stormphrax-avx512 PUBLIC -march=x86-64-v4)
target_compile_options(stormphrax-avx2-bmi2 PUBLIC -march=haswell)
# excavator without amd-specific extensions and bmi2
target_compile_options(stormphrax-avx2 PUBLIC -march=bdver4 -mno-tbm -mno-sse4a -mno-bmi2)
target_compile_options(stormphrax-sse41-popcnt PUBLIC -march=nehalem)

if(NOT MSVC)
	target_compile_options(stormphrax-native PUBLIC -mtune=native)
	target_compile_options(stormphrax-avx512 PUBLIC -mtune=znver4)
	target_compile_options(stormphrax-avx2-bmi2 PUBLIC -mtune=haswell)
	target_compile_options(stormphrax-avx2 PUBLIC -mtune=znver2) # zen 2
	target_compile_options(stormphrax-sse41-popcnt PUBLIC -mtune=sandybridge)
else() # clang
	target_compile_options(stormphrax-native PUBLIC /tune:native)
	target_compile_options(stormphrax-avx512 PUBLIC /tune:znver4)
	target_compile_options(stormphrax-avx2-bmi2 PUBLIC /tune:haswell)
	target_compile_options(stormphrax-avx2 PUBLIC /tune:znver2) # zen 2
	target_compile_options(stormphrax-sse41-popcnt PUBLIC /tune:sandybridge)
endif()

if(SP_FAST_PEXT)
	target_compile_definitions(stormphrax-native PUBLIC SP_FAST_PEXT)
endif()

get_directory_property(TARGETS BUILDSYSTEM_TARGETS)

foreach(TARGET ${TARGETS})
	string(REPLACE "stormphrax-" "" ARCH_NAME "${TARGET}")
	string(REPLACE "-" "_" ARCH_NAME "${ARCH_NAME}")
	string(TOUPPER ${ARCH_NAME} ARCH_NAME)

	target_compile_definitions(${TARGET} PUBLIC SP_VERSION=${CMAKE_PROJECT_VERSION}
		SP_${ARCH_NAME} SP_NETWORK_FILE="${PROJECT_SOURCE_DIR}/src/eval/${SP_DEFAULT_NET_NAME}.nnue")

	string(REPLACE "stormphrax-" "stormphrax-${CMAKE_PROJECT_VERSION}-" TARGET_NAME "${TARGET}")
	set_property(TARGET ${TARGET} PROPERTY OUTPUT_NAME "${TARGET_NAME}")

	if(SP_EMBED_COMMIT_HASH)
		target_compile_definitions(${TARGET} PUBLIC SP_COMMIT_HASH=${SP_COMMIT_HASH})
	endif()

	target_link_libraries(${TARGET} Threads::Threads)
endforeach()
