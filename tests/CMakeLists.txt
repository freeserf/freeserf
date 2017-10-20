cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

find_package(Threads REQUIRED)

include(ExternalProject)
ExternalProject_Add(googletest
                    URL https://github.com/google/googletest/archive/release-1.8.0.tar.gz
                    CMAKE_ARGS -Dgtest_force_shared_crt=ON
                               -DBUILD_GTEST=ON
                               -DBUILD_GMOCK=OFF
                               -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/googletest_src
                    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/googletest_bin
                    INSTALL_COMMAND "")
ExternalProject_Get_Property(googletest SOURCE_DIR BINARY_DIR)
include_directories(${INCLUDE_DIRECTORIES} "${SOURCE_DIR}/googletest/include")
set(GTEST_BIN_DIR "${BINARY_DIR}/googletest/${CMAKE_CFG_INTDIR}")

add_library(gtest STATIC IMPORTED)
set_target_properties(gtest PROPERTIES
                      IMPORTED_LOCATION "${GTEST_BIN_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}")
add_dependencies(gtest googletest)

add_library(gtest_main STATIC IMPORTED)
set_target_properties(gtest_main PROPERTIES
                      IMPORTED_LOCATION "${GTEST_BIN_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX}")
add_dependencies(gtest_main googletest)

include(GoogleTest)

if(WIN32)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /wd4290)
endif()

set(TEST_MAP_SOURCES test_map.cc)
add_executable(test_map ${TEST_MAP_SOURCES})
target_check_style(test_map)
target_link_libraries(test_map game gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
gtest_add_tests(TARGET test_map WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(TEST_SAVE_GAME_SOURCES test_save_game.cc)
add_executable(test_save_game ${TEST_SAVE_GAME_SOURCES})
target_check_style(test_save_game)
target_link_libraries(test_save_game game gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
gtest_add_tests(TARGET test_save_game)

set(TEST_MAP_GEOMETRY_SOURCES test_map_geometry.cc)
add_executable(test_map_geometry ${TEST_MAP_GEOMETRY_SOURCES})
target_check_style(test_map_geometry)
target_link_libraries(test_map_geometry game gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
gtest_add_tests(TARGET test_map_geometry)
