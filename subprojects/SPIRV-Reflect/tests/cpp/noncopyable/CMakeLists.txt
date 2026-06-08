cmake_minimum_required(VERSION 3.16)

project(noncopyable)

list(APPEND SPIRV_REFLECT_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../spirv_reflect.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../spirv_reflect.c
)

add_executable(noncopyable ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${COMMON_FILES} ${SPIRV_REFLECT_FILES})
target_include_directories(noncopyable PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set_target_properties(noncopyable PROPERTIES CXX_STANDARD 11)

if(WIN32)
    target_compile_definitions(noncopyable PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(noncopyable PROPERTIES FOLDER "tests")
endif()
