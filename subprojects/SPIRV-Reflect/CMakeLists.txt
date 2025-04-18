cmake_minimum_required(VERSION 3.16)

project(spirv-reflect)

OPTION(SPIRV_REFLECT_EXECUTABLE     "Build spirv-reflect executable" ON)

OPTION(SPIRV_REFLECT_STATIC_LIB     "Build a SPIRV-Reflect static library" OFF)
OPTION(SPIRV_REFLECT_BUILD_TESTS    "Build the SPIRV-Reflect test suite" OFF)
OPTION(SPIRV_REFLECT_ENABLE_ASSERTS "Enable asserts for debugging" OFF)
OPTION(SPIRV_REFLECT_ENABLE_ASAN    "Use address sanitization" OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if (SPIRV_REFLECT_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if (SPIRV_REFLECT_EXECUTABLE)
    # ==========================================================================
    # Compile spirv_reflect.c as C
    # ==========================================================================
    add_executable(spirv-reflect ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
                                 ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.c
                                 ${CMAKE_CURRENT_SOURCE_DIR}/examples/arg_parser.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/examples/arg_parser.cpp
                                 ${CMAKE_CURRENT_SOURCE_DIR}/examples/common.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/examples/common.cpp
                                 ${CMAKE_CURRENT_SOURCE_DIR}/common/output_stream.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/common/output_stream.cpp)
    target_compile_options(spirv-reflect PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Werror>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Werror>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wall -Werror>)
    if (SPIRV_REFLECT_ENABLE_ASSERTS)
        target_compile_definitions(spirv-reflect PRIVATE SPIRV_REFLECT_ENABLE_ASSERTS)
    endif()

    set_target_properties(spirv-reflect PROPERTIES CXX_STANDARD 11)
    target_include_directories(spirv-reflect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(WIN32)
        target_compile_definitions(spirv-reflect PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    install(TARGETS spirv-reflect RUNTIME DESTINATION bin)

    # ==========================================================================
    # Compile spirv_reflect.c as C++
    # ==========================================================================
    add_executable(spirv-reflect-pp ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.h
                                    ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/examples/arg_parser.h
                                    ${CMAKE_CURRENT_SOURCE_DIR}/examples/arg_parser.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/examples/common.h
                                    ${CMAKE_CURRENT_SOURCE_DIR}/examples/common.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/common/output_stream.h
                                    ${CMAKE_CURRENT_SOURCE_DIR}/common/output_stream.cpp)
    target_compile_options(spirv-reflect-pp PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Werror>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Werror>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wall -Werror>)
    if (SPIRV_REFLECT_ENABLE_ASSERTS)
        target_compile_definitions(spirv-reflect-pp PRIVATE SPIRV_REFLECT_ENABLE_ASSERTS)
    endif()
    set_target_properties(spirv-reflect-pp PROPERTIES CXX_STANDARD 11)
    target_include_directories(spirv-reflect-pp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(WIN32)
        target_compile_definitions(spirv-reflect-pp PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    install(TARGETS spirv-reflect-pp RUNTIME DESTINATION bin)
endif()

if (SPIRV_REFLECT_EXAMPLES)
    add_subdirectory(examples)
endif()

if (SPIRV_REFLECT_BUILD_TESTS)
    message("Tests are enabled!")
    SET(BUILD_GMOCK OFF CACHE BOOL "Builds the googlemock subproject" FORCE)
    SET(BUILD_GTEST ON CACHE BOOL "Builds the googletest subproject" FORCE)
    SET(INSTALL_GTEST OFF CACHE BOOL "Enable installation of googletest. (Projects embedding googletest generally turn this OFF.)" FORCE)
    SET(gtest_force_shared_crt ON CACHE BOOL "Enable DLL for C runtime libraries on Windows" FORCE)

    add_subdirectory(third_party/googletest)
	add_executable(test-spirv-reflect ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-spirv-reflect.cpp
                                      ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.h
                                      ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.c
                                      ${CMAKE_CURRENT_SOURCE_DIR}/common/output_stream.h
                                      ${CMAKE_CURRENT_SOURCE_DIR}/common/output_stream.cpp)
    set_target_properties(test-spirv-reflect PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                          CXX_STANDARD 11)
    target_compile_definitions(test-spirv-reflect PRIVATE
                               $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
    target_link_libraries(test-spirv-reflect gtest_main)
    target_include_directories(test-spirv-reflect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_custom_command(TARGET test-spirv-reflect POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()

if(SPIRV_REFLECT_STATIC_LIB)
    add_library(spirv-reflect-static STATIC ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.h
                                     ${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.c)

    target_include_directories(spirv-reflect-static
                               PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    install(TARGETS spirv-reflect-static
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
endif()

