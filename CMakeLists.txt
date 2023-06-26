cmake_minimum_required(VERSION 3.15)

project(polaris VERSION 1.8.0)

set(CMAKE_CXX_STANDARD 20)

set(PS_CLANG CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

if(MSVC AND NOT PS_CLANG)
	message(WARNING Polaris is untested on MSVC, and builds targeted to specific CPUs may not work as intended)
endif()

if(MSVC)
	add_compile_options(/EHsc)
	# for fathom
	add_compile_definitions(_SILENCE_CXX20_ATOMIC_INIT_DEPRECATION_WARNING _CRT_SECURE_NO_WARNINGS)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
endif()

find_package(Threads REQUIRED)

include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED)

if(LTO_SUPPORTED)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
endif()

option(PS_FAST_PEXT "whether pext and pdep are usably fast on this architecture, for building native binaries" ON)

set(POLARIS_COMMON_SRC src/types.h src/main.cpp src/uci.h src/uci.cpp src/core.h src/util/bitfield.h src/util/bits.h src/util/parse.h src/util/split.h src/util/split.cpp src/util/rng.h src/util/static_vector.h src/bitboard.h src/move.h src/hash.h src/hash.cpp src/position/position.h src/position/position.cpp src/search.h src/search.cpp src/eval/material.h src/eval/material.cpp src/movegen.h src/movegen.cpp src/attacks/util.h src/attacks/attacks.h src/util/timer.h src/util/timer.cpp src/pretty.h src/pretty.cpp src/rays.h src/ttable.h src/ttable.cpp src/limit/limit.h src/limit/trivial.h src/limit/time.h src/limit/time.cpp src/util/cemath.h src/eval/eval.h src/eval/eval.cpp src/util/range.h src/arch.h src/perft.h src/perft.cpp src/search_fwd.h src/see.h src/bench.h src/bench.cpp src/tunable.h src/opts.h src/position/boards.h src/history.h src/3rdparty/fathom/stdendian.h src/3rdparty/fathom/tbconfig.h src/3rdparty/fathom/tbprobe.h src/3rdparty/fathom/tbprobe.cpp)

set(POLARIS_BMI2_SRC src/attacks/bmi2/data.h src/attacks/bmi2/attacks.h src/attacks/bmi2/attacks.cpp)
set(POLARIS_NON_BMI2_SRC src/attacks/black_magic/data.h src/attacks/black_magic/attacks.h src/attacks/black_magic/attacks.cpp)

add_executable(polaris-native ${POLARIS_COMMON_SRC} ${POLARIS_BMI2_SRC} ${POLARIS_NON_BMI2_SRC})
add_executable(polaris-bmi2 ${POLARIS_COMMON_SRC} ${POLARIS_BMI2_SRC})
add_executable(polaris-modern ${POLARIS_COMMON_SRC} ${POLARIS_NON_BMI2_SRC})
add_executable(polaris-popcnt ${POLARIS_COMMON_SRC} ${POLARIS_NON_BMI2_SRC})
add_executable(polaris-compat ${POLARIS_COMMON_SRC} ${POLARIS_NON_BMI2_SRC})

if(NOT MSVC OR PS_CLANG)
	target_compile_options(polaris-native PUBLIC -march=native)
	target_compile_options(polaris-bmi2 PUBLIC -march=haswell)
	target_compile_options(polaris-modern PUBLIC -march=bdver2 -mno-tbm -mno-sse4a) # piledriver without amd-specific extensions
	target_compile_options(polaris-popcnt PUBLIC -march=nehalem)
	target_compile_options(polaris-compat PUBLIC -march=core2)
endif()

if(NOT MSVC)
	target_compile_options(polaris-native PUBLIC -mtune=native)
	target_compile_options(polaris-bmi2 PUBLIC -mtune=haswell)
	target_compile_options(polaris-modern PUBLIC -mtune=znver2) # zen 2
	target_compile_options(polaris-popcnt PUBLIC -mtune=sandybridge)
	target_compile_options(polaris-compat PUBLIC -mtune=core2)
elseif(MSVC AND PS_CLANG)
	target_compile_options(polaris-native PUBLIC /tune:native)
	target_compile_options(polaris-bmi2 PUBLIC /tune:skylake)
	target_compile_options(polaris-modern PUBLIC /tune:znver2) # zen 2
	target_compile_options(polaris-popcnt PUBLIC /tune:sandybridge)
	target_compile_options(polaris-compat PUBLIC /tune:core2)
endif()

if(PS_FAST_PEXT)
	target_compile_definitions(polaris-native PUBLIC PS_FAST_PEXT)
endif()

get_directory_property(TARGETS BUILDSYSTEM_TARGETS)

foreach(TARGET ${TARGETS})
	string(REPLACE "polaris-" "" ARCH_NAME "${TARGET}")
	string(REPLACE "-" "_" ARCH_NAME "${ARCH_NAME}")
	string(TOUPPER ${ARCH_NAME} ARCH_NAME)

	target_compile_definitions(${TARGET} PUBLIC PS_VERSION=${CMAKE_PROJECT_VERSION} PS_${ARCH_NAME})

	string(REPLACE "-" "-${CMAKE_PROJECT_VERSION}-" TARGET_NAME "${TARGET}")
	set_property(TARGET ${TARGET} PROPERTY OUTPUT_NAME "${TARGET_NAME}")

	target_link_libraries(${TARGET} Threads::Threads)
endforeach()
