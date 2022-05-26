cmake_minimum_required(VERSION 3.18)
project(LibPressioSperr VERSION 0.0.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
find_package(LibPressio REQUIRED)
find_package(std_compat REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(Sperr REQUIRED IMPORTED_TARGET SPERR)

option(BUILD_SHARED_LIBS "build shared libraries" ON)

add_library(libpressio_sperr ./src/libpressio-sperr.cc)
target_link_libraries(libpressio_sperr PUBLIC LibPressio::libpressio PkgConfig::Sperr)

install(TARGETS libpressio_sperr
  EXPORT LibPressioSperrTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT LibPressioSperrTargets NAMESPACE LibPressioSperr:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibPressioSperr)
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/LibPressioSperrConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/LibPressioSperrConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibPressioSperr
  )
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/LibPressioSperrConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
  )
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/LibPressioSperrConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/LibPressioSperrConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibPressioSperr
  )
