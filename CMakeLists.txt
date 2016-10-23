cmake_minimum_required(VERSION 2.4)
project(freeserf)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CreateSymlink)
include(RunTests)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
add_definitions(-DHAVE_CONFIG_H)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

include(VcsVersion)
find_package(SDL2_mixer REQUIRED)

if(SDLMIXER_FOUND)
    include_directories(${SDLMIXER_INCLUDE_DIR})
    set(ENABLE_SDL2_MIXER 1)
endif()

check_include_files(byteswap.h CMAKE_HAVE_BYTESWAP_H)
check_include_files(endian.h CMAKE_HAVE_ENDIAN_H)
check_include_files(fcntl.h CMAKE_HAVE_FCNTL_H)
check_include_files(inttypes.h CMAKE_HAVE_INTTYPES_H)
check_include_files(memory.h CMAKE_HAVE_MEMORY_H)
check_include_files(stdint.h CMAKE_HAVE_STDINT_H)
check_include_files(stdlib.h CMAKE_HAVE_STDLIB_H)
check_include_files(strings.h CMAKE_HAVE_STRINGS_H)
check_include_files(string.h CMAKE_HAVE_STRING_H)
check_include_files(sys/endian.h CMAKE_HAVE_SYS_ENDIAN_H)
check_include_files(sys/mman.h CMAKE_HAVE_SYS_MMAN_H)
check_include_files(sys/param.h CMAKE_HAVE_SYS_PARAM_H)
check_include_files(sys/stat.h CMAKE_HAVE_SYS_STAT_H)
check_include_files(sys/types.h CMAKE_HAVE_SYS_TYPES_H)
check_include_files(unistd.h CMAKE_HAVE_UNISTD_H)

check_function_exists(atexit CMAKE_HAVE_ATEXIT)
check_function_exists(getpagesize CMAKE_HAVE_GETPAGESIZE)
check_function_exists(memset CMAKE_HAVE_MEMSET)
check_function_exists(mmap CMAKE_HAVE_MMAP)
check_function_exists(munmap CMAKE_HAVE_MUNMAP)
check_function_exists(strtol CMAKE_HAVE_STRTOL)

configure_file(config.h.cmake config.h @ONLY)

set(PACKAGE_BUGREPORT "https://github.com/khillah/freeserf/issues")
configure_file(definitions.h.cmake definitions.h)

set(HEADERS
	src/tpwm.h
	src/map.h
	src/gfx.h
	src/debug.h
	src/audio-sdlmixer.h
	src/event_loop-sdl.h
	src/map-geometry.h
	src/data-source-dos.h
	src/resource.h
	src/freeserf_endian.h
	src/version.h
	src/video.h
	src/player.h
	src/audio-dummy.h
	src/mission.h
	src/event_loop.h
	src/interface.h
	src/notification.h
	src/flag.h
	src/savegame.h
	src/log.h
	src/video-sdl.h
	src/map-generator.h
	src/objects.h
	src/random.h
	src/inventory.h
	src/text-input.h
	src/minimap.h
	src/panel.h
	src/data-source.h
	src/data.h
	src/sfx2wav.h
	src/freeserf.h
	src/viewport.h
	src/game.h
	src/gui.h
	src/audio.h
	src/misc.h
	src/building.h
	src/game-init.h
	src/serf.h
	src/popup.h
	src/pathfinder.h
	src/xmi2mid.h
)

if(SDLMIXER_FOUND)
	set(AUDIO_CC src/audio-sdlmixer.cc)
else()
	set(AUDIO_CC src/audio-dummy.cc)
endif()

set(SOURCES
	src/tpwm.cc
	src/minimap.cc
	src/building.cc
	src/savegame.cc
	src/random.cc
	src/log.cc
	src/serf.cc
	src/data-source-dos.cc
	src/gfx.cc
	src/interface.cc
	src/game-init.cc
	src/notification.cc
	src/version.cc
	src/video-sdl.cc
	src/viewport.cc
	src/event_loop.cc
	src/data-source.cc
	src/map-generator.cc
	src/debug.cc
	src/player.cc
	src/sfx2wav.cc
	src/map.cc
	src/video.cc
	src/text-input.cc
	src/inventory.cc
	src/pathfinder.cc
	src/popup.cc
	src/event_loop-sdl.cc
	src/flag.cc
	src/audio.cc
	src/mission.cc
	src/game.cc
	src/data.cc
	src/gui.cc
	src/panel.cc
	src/xmi2mid.cc
	${AUDIO_CC}
)

set(CMAKE_CXX_FLAGS "-std=c++14") #-Wall --pedantic")

set(LIBRARY freeserf_static)
add_library(${LIBRARY} STATIC ${SOURCES} ${HEADERS})

set(EXECUTABLE freeserf)
add_executable(${EXECUTABLE} src/freeserf.cc)
add_dependencies(${EXECUTABLE} ${LIBRARY})
target_link_libraries(${EXECUTABLE} ${LIBRARY} ${SDL2_LIBRARY} ${SDLMIXER_LIBRARY})

set(EXECUTABLE freeserf_map_test)
add_executable(${EXECUTABLE} tests/test_map.cc)
add_dependencies(${EXECUTABLE} ${LIBRARY})
target_link_libraries(${EXECUTABLE} ${LIBRARY})
create_symlink(tests tests)

set(EXECUTABLE freeserf_map_geometry_test)
add_executable(${EXECUTABLE} tests/test_map_geometry.cc)
add_dependencies(${EXECUTABLE} ${LIBRARY})
target_link_libraries(${EXECUTABLE} ${LIBRARY})

set(EXECUTABLE freeserf_save_game_test)
add_executable(${EXECUTABLE} tests/test_save_game.cc)
add_dependencies(${EXECUTABLE} ${LIBRARY})
target_link_libraries(${EXECUTABLE} ${LIBRARY})

set(TEST_TARGETS
	freeserf_map_test
	freeserf_map_geometry_test
	freeserf_save_game_test
)

run_tests(TEST_TARGETS)
