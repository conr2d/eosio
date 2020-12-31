# suppress find_package(Boost ...)
macro(find_package)
  if(NOT "${ARGV0}" STREQUAL "Boost")
    _find_package(${ARGV})
  endif()
endmacro()

# suppress unnecessary install
macro(install)
  cmake_parse_arguments(EOSIO_INSTALL "" "COMPONENT" "" ${ARGN})
  if(EOSIO_INSTALL_COMPONENT STREQUAL "base")
    _install(${ARGV})
  endif()
endmacro()

# set default build type to Release
set(CMAKE_BUILD_TYPE "Release")
