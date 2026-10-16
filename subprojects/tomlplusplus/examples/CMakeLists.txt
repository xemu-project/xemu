cmake_minimum_required(VERSION 3.14)

project(Examples LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(tomlplusplus REQUIRED)
endif()

add_custom_target(run_examples COMMENT "Running all examples")

function(add_example name)
  cmake_parse_arguments(PARSE_ARGV 1 "" "" "" ARGS)
  add_executable("${name}" "${name}.cpp")
  target_link_libraries("${name}" PRIVATE tomlplusplus::tomlplusplus)
  target_compile_features("${name}" PRIVATE cxx_std_17)
  add_custom_target("run_${name}" COMMAND "${name}" ${_ARGS} VERBATIM)
  add_dependencies(run_examples "run_${name}")
endfunction()

add_example(error_printer)
add_example(parse_benchmark)
add_example(simple_parser)
add_example(toml_generator)
add_example(toml_merger)
add_example(toml_to_json_transcoder ARGS "${PROJECT_SOURCE_DIR}/example.toml")
