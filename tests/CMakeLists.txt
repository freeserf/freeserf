cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

if(WIN32)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /wd4290)
endif()

set(TEST_MAP_SOURCES ../tests/test_map.cc)
add_executable(test_map ${TEST_MAP_SOURCES})
target_link_libraries(test_map game)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/map-memdump-1 ${CMAKE_CURRENT_BINARY_DIR}/tests/data/map-memdump-1 COPYONLY)
add_test(NAME test_map COMMAND test_map WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set(TEST_SAVE_GAME_SOURCES ../tests/test_save_game.cc)
add_executable(test_save_game ${TEST_SAVE_GAME_SOURCES})
target_link_libraries(test_save_game game)
add_test(test_save_game test_save_game)

set(TEST_MAP_GEOMETRY_SOURCES ../tests/test_map_geometry.cc)
add_executable(test_map_geometry ${TEST_MAP_GEOMETRY_SOURCES})
target_link_libraries(test_map_geometry game)
add_test(test_map_geometry test_map_geometry)
