cmake_minimum_required(VERSION 3.14)

project(
    tomlplusplus
    VERSION 3.3.0
    DESCRIPTION "Header-only TOML config file parser and serializer for C++17"
    HOMEPAGE_URL "https://marzer.github.io/tomlplusplus/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(tomlplusplus_tomlplusplus INTERFACE)
add_library(tomlplusplus::tomlplusplus ALIAS tomlplusplus_tomlplusplus)

set_property(
    TARGET tomlplusplus_tomlplusplus PROPERTY
    EXPORT_NAME tomlplusplus
)

target_include_directories(
  tomlplusplus_tomlplusplus
  ${tomlplusplus_warning_guard} # unquoted for list expansion
  INTERFACE
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(tomlplusplus_tomlplusplus INTERFACE cxx_std_17)

# ---- Install rules and examples ----
if(PROJECT_IS_TOP_LEVEL)
  include(cmake/install-rules.cmake)
  option(BUILD_EXAMPLES "Build examples tree." OFF)
  if(BUILD_EXAMPLES)
    add_subdirectory(examples)
  endif()
endif()
