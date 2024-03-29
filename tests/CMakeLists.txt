cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

find_package(Threads REQUIRED)

cmake_policy(SET CMP0135 NEW)
include(ExternalProject)
ExternalProject_Add(googletest
                    URL https://github.com/google/googletest/archive/release-1.12.1.tar.gz
                    CMAKE_ARGS -Dgtest_force_shared_crt=ON
                               -DBUILD_GMOCK=OFF
                               -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/googletest_src
                    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/googletest_bin
                    INSTALL_COMMAND "")
ExternalProject_Get_Property(googletest SOURCE_DIR BINARY_DIR)
include_directories(${INCLUDE_DIRECTORIES} "${SOURCE_DIR}/googletest/include")
set(GTEST_BIN_DIR "${BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib")

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(GTEST_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest_bin/lib/${CMAKE_CFG_INTDIR}")
endif()

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
  add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif()

set(TEST_MAP_SOURCES test_map.cc)
add_executable(test_map ${TEST_MAP_SOURCES})
target_check_style(test_map)
set_property(TARGET test_map PROPERTY FOLDER "Tests")
target_link_libraries(test_map game tools gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
gtest_add_tests(TARGET test_map
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
                TEST_LIST test_list)
foreach(test IN LISTS test_list)
  set_tests_properties(${test} PROPERTIES ENVIRONMENT "GTEST_OUTPUT=xml:${PROJECT_BINARY_DIR}/${test}.xml")
endforeach(test)

set(TEST_SAVE_GAME_SOURCES test_save_game.cc)
add_executable(test_save_game ${TEST_SAVE_GAME_SOURCES})
target_check_style(test_save_game)
set_property(TARGET test_save_game PROPERTY FOLDER "Tests")
target_link_libraries(test_save_game game tools gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
gtest_add_tests(TARGET test_save_game
                TEST_LIST test_list)
foreach(test IN LISTS test_list)
  set_tests_properties(${test} PROPERTIES ENVIRONMENT "GTEST_OUTPUT=xml:${PROJECT_BINARY_DIR}/${test}.xml")
endforeach(test)

set(TEST_MAP_GEOMETRY_SOURCES test_map_geometry.cc)
add_executable(test_map_geometry ${TEST_MAP_GEOMETRY_SOURCES})
target_check_style(test_map_geometry)
set_property(TARGET test_map_geometry PROPERTY FOLDER "Tests")
target_link_libraries(test_map_geometry game tools gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
gtest_add_tests(TARGET test_map_geometry
                TEST_LIST test_list)
foreach(test IN LISTS test_list)
  set_tests_properties(${test} PROPERTIES ENVIRONMENT "GTEST_OUTPUT=xml:${PROJECT_BINARY_DIR}/${test}.xml")
endforeach(test)
