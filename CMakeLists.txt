cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(FreeSerf)
set(PROJECT_VERSION "0.2.0")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Git)
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --tags --dirty
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE FREESERF_VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(FIND ${FREESERF_VERSION} "." VERSION_FULL)
  if(VERSION_FULL STREQUAL "-1")
    string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" PROJECT_VERSION_MAJOR "${PROJECT_VERSION}")
    string(REGEX REPLACE "^[0-9]+\\.([0-9]+).*" "\\1" PROJECT_VERSION_MINOR "${PROJECT_VERSION}")
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE PROJECT_VERSION_PATCH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(FREESERF_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    set(PROJECT_VERSION "${FREESERF_VERSION}")
  else()
    string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" PROJECT_VERSION_MAJOR "${FREESERF_VERSION}")
    string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" PROJECT_VERSION_MINOR "${FREESERF_VERSION}")
    string(REGEX REPLACE "^v[0-9]+\\.[0-9]+.*-g([0-9a-f]+).*" "\\1" PROJECT_VERSION_PATCH "${FREESERF_VERSION}")
    set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
  endif()
else()
  set(FREESERF_VERSION "${PROJECT_VERSION}")
  string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" PROJECT_VERSION_MAJOR "${PROJECT_VERSION}")
  string(REGEX REPLACE "^[0-9]+\\.([0-9]+).*" "\\1" PROJECT_VERSION_MINOR "${PROJECT_VERSION}")
  string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" PROJECT_VERSION_PATCH "${PROJECT_VERSION}")
endif()

message(STATUS "FREESERF_VERSION = ${FREESERF_VERSION}")
message(STATUS "PROJECT_VERSION = ${PROJECT_VERSION}")
message(STATUS "PROJECT_VERSION_MAJOR = ${PROJECT_VERSION_MAJOR}")
message(STATUS "PROJECT_VERSION_MINOR = ${PROJECT_VERSION_MINOR}")
message(STATUS "PROJECT_VERSION_PATCH = ${PROJECT_VERSION_PATCH}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ ${CMAKE_CXX_FLAGS}")
endif()

include_directories(${INCLUDE_DIRECTORIES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version-vcs.h.in
               "${CMAKE_CURRENT_BINARY_DIR}/src/version-vcs.h"
               @ONLY)

add_definitions(-DPACKAGE_BUGREPORT="https://github.com/freeserf/freeserf/issues")

include(CppLint)
enable_check_style()

add_subdirectory(src)

option(ENABLE_TESTS "Enable compilation of tests" ON)
if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
