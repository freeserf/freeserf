if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(Forkserf)
set(PROJECT_VERSION "0.5.1")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(GitTools)
git_make_version(FORKSERF_VERSION ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_VERSION})
message(STATUS "FORKSERF_VERSION = ${FORKSERF_VERSION}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "-std=gnu++14 ${CMAKE_CXX_FLAGS}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "-std=c++14 -stdlib=libc++ ${CMAKE_CXX_FLAGS}")
endif()

include_directories(${INCLUDE_DIRECTORIES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(VCS_TAG "${FORKSERF_VERSION}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version-vcs.h.in
               "${CMAKE_CURRENT_BINARY_DIR}/src/version-vcs.h"
               @ONLY)

add_definitions(-DPACKAGE_BUGREPORT="https://github.com/forkserf/forkserf/issues")

include(CppLint)
enable_check_style()

add_subdirectory(src)

# tests not working right
#option(ENABLE_TESTS "Enable compilation of tests" ON)
#if(ENABLE_TESTS)
#  enable_testing()
#  add_subdirectory(tests)
#endif()
