include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

install(
    FILES "${PROJECT_SOURCE_DIR}/toml++.natvis" "${PROJECT_SOURCE_DIR}/cpp.hint"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/tomlplusplus"
    COMPONENT tomlplusplus_Development
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT tomlplusplus_Development
)

install(
    TARGETS tomlplusplus_tomlplusplus
    EXPORT tomlplusplusTargets
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    tomlplusplusConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

set(
    tomlplusplus_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/tomlplusplus"
    CACHE STRING "CMake package config location relative to the install prefix"
)

mark_as_advanced(tomlplusplus_INSTALL_CMAKEDIR)

install(
    FILES
    "${PROJECT_SOURCE_DIR}/cmake/tomlplusplusConfig.cmake"
    "${PROJECT_BINARY_DIR}/tomlplusplusConfigVersion.cmake"
    DESTINATION "${tomlplusplus_INSTALL_CMAKEDIR}"
    COMPONENT tomlplusplus_Development
)

install(
    EXPORT tomlplusplusTargets
    NAMESPACE tomlplusplus::
    DESTINATION "${tomlplusplus_INSTALL_CMAKEDIR}"
    COMPONENT tomlplusplus_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
