cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

option(ENABLE_SDL2_MIXER "Enable audio support using SDL2_mixer" ON)
option(ENABLE_SDL2_IMAGE "Enable image loading using SDL2_image" ON)
set(SDL2_BUILDING_LIBRARY 1)
find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
  if(ENABLE_SDL2_MIXER)
    find_package(SDL2_mixer)
  endif()
  if(ENABLE_SDL2_IMAGE)
    find_package(SDL2_image)
  endif()
endif()

if(WIN32)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS)
endif()

# Tools library

set(TOOLS_SOURCES debug.cc
                  log.cc
                  configfile.cc
                  buffer.cc)

set(TOOLS_HEADERS debug.h
                  log.h
                  misc.h
                  configfile.h
                  buffer.h)

add_library(tools STATIC ${TOOLS_SOURCES} ${TOOLS_HEADERS})
target_check_style(tools)

# Game library

set(GAME_SOURCES ai.cc
                 ai_roadbuilder.cc
                 ai_util.cc
                 building.cc
                 flag.cc
                 game.cc
                 inventory.cc
                 map.cc
                 map-generator.cc
                 mission.cc
                 player.cc
                 random.cc
                 savegame.cc
                 serf.cc
                 game-manager.cc
                 game-options.cc)

set(GAME_HEADERS ai.h
                 ai_roadbuilder.h
                 building.h
                 flag.h
                 game.h
                 inventory.h
                 lookup.h
                 map.h
                 map-generator.h
                 map-geometry.h
                 mission.h
                 objects.h
                 player.h
                 random.h
                 resource.h
                 savegame.h
                 serf.h
                 game-manager.h
                 game-options.h)

add_library(game STATIC ${GAME_SOURCES} ${GAME_HEADERS})
target_check_style(game)

# Platform library

set(PLATFORM_SOURCES video.cc
                     audio.cc
                     event_loop.cc)

set(PLATFORM_HEADERS video.h
                     audio.h
                     event_loop.h)

if(SDL2_FOUND)
  include_directories(${INCLUDE_DIRECTORIES} ${SDL2_INCLUDE_DIR})
  list(APPEND PLATFORM_SOURCES video-sdl.cc event_loop-sdl.cc)
  list(APPEND PLATFORM_HEADERS video-sdl.h event_loop-sdl.h)
endif()

if(ENABLE_SDL2_MIXER AND SDL2_MIXER_FOUND)
  include_directories(${INCLUDE_DIRECTORIES} ${SDL2_MIXER_INCLUDE_DIR})
  list(APPEND PLATFORM_SOURCES audio-sdlmixer.cc)
  list(APPEND PLATFORM_HEADERS audio-sdlmixer.h)
else()
  list(APPEND PLATFORM_SOURCES audio-dummy.cc)
  list(APPEND PLATFORM_HEADERS audio-dummy.h )
endif()

add_library(platform STATIC ${PLATFORM_SOURCES} ${PLATFORM_HEADERS})
target_check_style(platform)

# Data library

set(DATA_SOURCES data.cc
                 data-source-dos.cc
                 data-source-amiga.cc
                 data-source-legacy.cc
                 data-source-custom.cc
                 tpwm.cc
                 sfx2wav.cc
                 xmi2mid.cc
                 pcm2wav.cc
                 data-source.cc)

set(DATA_HEADERS data.h
                 data-source-dos.h
                 data-source-amiga.h
                 data-source-legacy.h
                 data-source-custom.h
                 tpwm.h
                 sfx2wav.h
                 xmi2mid.h
                 pcm2wav.h
                 data-source.h
                 sprite-file.h)

if(ENABLE_SDL2_IMAGE AND SDL2_IMAGE_FOUND)
  include_directories(${INCLUDE_DIRECTORIES} ${SDL2_IMAGE_INCLUDE_DIR})
  list(APPEND DATA_SOURCES sprite-file-sdl.cc)
else()
  list(APPEND DATA_SOURCES sprite-file-dummy.cc)
endif()

add_library(data STATIC ${DATA_SOURCES} ${DATA_HEADERS})
target_check_style(data)

# Forkserf executable

set(OTHER_SOURCES ai_pathfinder.cc
                  pathfinder.cc
                  gfx.cc
                  viewport.cc
                  minimap.cc
                  interface.cc
                  gui.cc
                  popup.cc
                  game-init.cc
                  notification.cc
                  panel.cc
                  version.cc
                  text-input.cc
                  list.cc
                  command_line.cc)

set(OTHER_HEADERS pathfinder.h
                  gfx.h
                  viewport.h
                  minimap.h
                  interface.h
                  gui.h
                  popup.h
                  game-init.h
                  notification.h
                  panel.h
                  misc.h
                  freeserf_endian.h
                  version.h
                  text-input.h
                  list.h
                  convertor.h
                  command_line.h)

set(FORKSERF_SOURCES freeserf.cc ${OTHER_SOURCES})
set(FORKSERF_HEADERS freeserf.h ${OTHER_HEADERS})
add_executable(Forkserf MACOSX_BUNDLE WIN32 ${FORKSERF_SOURCES} ${FORKSERF_HEADERS})
target_check_style(Forkserf)

target_link_libraries(Forkserf game platform data tools)
if(SDL2_FOUND)
  target_link_libraries(Forkserf optimized ${SDL2_LIBRARY} debug ${SDL2_LIBRARY_DEBUG})
  if(WIN32)
    target_link_libraries(Forkserf optimized ${SDL2_MAIN_LIBRARY} debug ${SDL2_MAIN_LIBRARY_DEBUG})
  endif()
  if(SDL2_MIXER_FOUND)
    target_link_libraries(Forkserf optimized ${SDL2_MIXER_LIBRARY} debug ${SDL2_MIXER_LIBRARY_DEBUG})
  endif()
  if(SDL2_IMAGE_FOUND)
    target_link_libraries(Forkserf optimized ${SDL2_IMAGE_LIBRARY} debug ${SDL2_IMAGE_LIBRARY_DEBUG})
  endif()
endif()
install(TARGETS Forkserf DESTINATION ".")

if(WIN32)
  if(DEFINED _VCPKG_INSTALLED_DIR)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      get_filename_component(SDL2_LIBRARY_DIR ${SDL2_LIBRARY_DEBUG} DIRECTORY)
	else()
      get_filename_component(SDL2_LIBRARY_DIR ${SDL2_LIBRARY} DIRECTORY)
    endif()
    get_filename_component(BIN_DIRS ${SDL2_LIBRARY_DIR} DIRECTORY)
    set(BIN_DIRS "${BIN_DIRS}/bin")
  else()
    get_filename_component(SDL2_DIR ${SDL2_LIBRARY} DIRECTORY)
    get_filename_component(SDL2_MIXER_DIR ${SDL2_MIXER_LIBRARY} DIRECTORY)
    get_filename_component(SDL2_IMAGE_DIR ${SDL2_IMAGE_LIBRARY} DIRECTORY)
    set(BIN_DIRS "${SDL2_DIR};${SDL2_MIXER_DIR};${SDL2_IMAGE_DIR};${XMP_DIR}/bin")
  endif()

  install(CODE "include(BundleUtilities)
                fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/Forkserf.exe\" \"\" \"${BIN_DIRS}\")")
elseif(APPLE)
  install(CODE "include(BundleUtilities)
                fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/Forkserf.app\" \"\" \"\")")
endif()

set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/COPYING)
set(CPACK_PACKAGE_VERSION_MAJOR	${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR	${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH	${PROJECT_VERSION_PATCH})
if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WIN_ARCH_STRING x64)
  else()
    set(WIN_ARCH_STRING x86)
  endif()
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${WIN_ARCH_STRING}-${CMAKE_BUILD_TYPE}")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
  set(CPACK_PACKAGE_EXECUTABLES "Forkserf;Forkserf")
  # install VC redistributables
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
    set(CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY TRUE)
  endif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
  include(InstallRequiredSystemLibraries)
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
else()
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
endif()
include(CPack)

#     tlongstretch, copy custom graphics into build dir on successful build
# top level metadata file, remember to uncomment each dir/section as new types added
add_custom_command(TARGET Forkserf POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/custom_graphics/meta.ini $<TARGET_FILE_DIR:Forkserf>)
# custom tree graphics for spring, fall, winter
add_custom_command(TARGET Forkserf POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/custom_graphics/map_object $<TARGET_FILE_DIR:Forkserf>/map_object)
# custom tree shadows for spring, fall, winter custom trees
add_custom_command(TARGET Forkserf POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/custom_graphics/map_shadow $<TARGET_FILE_DIR:Forkserf>/map_shadow)
# custom panel bar / frame_bottom graphics for season dial display
add_custom_command(TARGET Forkserf POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/custom_graphics/frame_bottom $<TARGET_FILE_DIR:Forkserf>/frame_bottom)
# custom game-init icons for human vs AI
add_custom_command(TARGET Forkserf POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/custom_graphics/icon $<TARGET_FILE_DIR:Forkserf>/icon)
# custom splashes for water-objects
add_custom_command(TARGET Forkserf POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/custom_graphics/map_waves $<TARGET_FILE_DIR:Forkserf>/map_waves)


# Profiler executable

set(PROFILER_SOURCES profiler.cc
                     version.cc
                     command_line.cc
                     pathfinder.cc)

set(PROFILER_HEADERS profiler.h
                     version.h
                     command_line.h
                     pathfinder.h)

add_executable(profiler ${PROFILER_SOURCES} ${PROFILER_HEADERS})
target_check_style(profiler)
target_link_libraries(profiler game tools)
