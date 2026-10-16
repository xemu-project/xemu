cmake_minimum_required(VERSION 3.18)

project(nv2a_vsh_cpu)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

set(CMAKE_CXX_STANDARD 17)
include(CMakePushCheckState)
set(_CMAKE_PROCESSING_LANGUAGE "C")
include(CheckSymbolExists)
include (ExternalProject)

option(nv2a_vsh_cpu_UNIT_TEST "Enable nv2a_vsh_cpu's various unit tests" ON)

# Unit Test's requirement ----------------------------
if (nv2a_vsh_cpu_UNIT_TEST)
include(FindPkgConfig)
find_package(
        Boost 1.71
        COMPONENTS
        unit_test_framework
        REQUIRED
)

set(GENERATED_FILES_DIR "${CMAKE_BINARY_DIR}/generated")
include_directories("${Boost_INCLUDE_DIR}" "${GENERATED_FILES_DIR}")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_library(
        nv2a_vsh_disassembler
        src/nv2a_vsh_disassembler.c
        src/nv2a_vsh_disassembler.h
)

target_include_directories(
        nv2a_vsh_disassembler
        PRIVATE
        src
)

add_library(
        nv2a_vsh_cpu
        src/nv2a_vsh_cpu.c
        src/nv2a_vsh_cpu.h
)

target_include_directories(
        nv2a_vsh_cpu
        PRIVATE
        src
)

add_library(
        nv2a_vsh_emulator
        src/nv2a_vsh_emulator.c
        src/nv2a_vsh_emulator.h
        src/nv2a_vsh_emulator_execution_state.c
        src/nv2a_vsh_emulator_execution_state.h
)

target_include_directories(
        nv2a_vsh_emulator
        PUBLIC
        src
)

target_link_libraries(
        nv2a_vsh_emulator
        PRIVATE nv2a_vsh_cpu
        PRIVATE nv2a_vsh_disassembler
)
add_dependencies(nv2a_vsh_emulator nv2a_vsh_cpu nv2a_vsh_disassembler)


# Unit Tests -----------------------------------------
if (nv2a_vsh_cpu_UNIT_TEST)
enable_testing()
add_executable(
        operations_tests
        test/operations/test_main.cpp
        test/operations/test_basic.cpp
)
target_include_directories(
        operations_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        operations_tests
        LINK_PRIVATE
        nv2a_vsh_cpu
        ${Boost_LIBRARIES}
)
add_test(NAME operations_tests COMMAND operations_tests)
add_dependencies(operations_tests nv2a_vsh_cpu)


add_executable(
        disassembler_tests
        test/disassembler/test_main.cpp
        test/disassembler/test_basic.cpp
)
target_include_directories(
        disassembler_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        disassembler_tests
        LINK_PRIVATE
        nv2a_vsh_disassembler
        ${Boost_LIBRARIES}
)
add_test(NAME disassembler_tests COMMAND disassembler_tests)
add_dependencies(disassembler_tests nv2a_vsh_disassembler)


add_executable(
        emulator_tests
        test/emulator/test_main.cpp
        test/emulator/test_basic.cpp
)
target_include_directories(
        emulator_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        emulator_tests
        LINK_PRIVATE
        nv2a_vsh_emulator
        ${Boost_LIBRARIES}
)
add_test(NAME emulator_tests COMMAND emulator_tests)
add_dependencies(emulator_tests nv2a_vsh_emulator)
endif()
