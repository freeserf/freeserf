cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

option(ENABLE_SDL2_MIXER "Enable audio support using SDL2_mixer" ON)
set(SDL2_BUILDING_LIBRARY 1)
find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
  if(ENABLE_SDL2_MIXER)
    find_package(SDL2_mixer REQUIRED)
  else()
    find_package(SDL2_mixer)
  endif()
endif()

option(ENABLE_XMP "Enable libxmp support" ON)
if(ENABLE_XMP)
  find_package(XMP REQUIRED)
else()
  find_package(XMP)
endif()

set(GAME_SOURCES building.cc
                 debug.cc
                 flag.cc
                 game.cc
                 inventory.cc
                 log.cc
                 map.cc
                 map-generator.cc
                 mission.cc
                 player.cc
                 random.cc
                 savegame.cc
                 serf.cc
                 game-manager.cc
                 configfile.cc)

set(GAME_HEADERS building.h
                 debug.h
                 flag.h
                 game.h
                 inventory.h
                 log.h
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
                 misc.h
                 game-manager.h
                 configfile.h)

set(OTHER_SOURCES data.cc
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
                  video.cc
                  audio.cc
                  version.cc
                  data-source-dos.cc
                  data-source-amiga.cc
                  tpwm.cc
                  event_loop.cc
                  sfx2wav.cc
                  xmi2mid.cc
                  data-source.cc
                  text-input.cc
                  list.cc
                  buffer.cc)

set(OTHER_HEADERS data.h
                  pathfinder.h
                  gfx.h
                  viewport.h
                  minimap.h
                  interface.h
                  gui.h
                  popup.h
                  game-init.h
                  notification.h
                  panel.h
                  video.h
                  audio.h
                  misc.h
                  freeserf_endian.h
                  version.h
                  data-source-dos.h
                  data-source-amiga.h
                  tpwm.h
                  event_loop.h
                  sfx2wav.h
                  xmi2mid.h
                  data-source.h
                  text-input.h
                  list.h
                  buffer.h
                  convertor.h)

if(WIN32)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /wd4290)
endif()

add_library(game STATIC ${GAME_SOURCES} ${GAME_HEADERS})

if(SDL2_FOUND)
  include_directories(${INCLUDE_DIRECTORIES} ${SDL2_INCLUDE_DIR})
  list(APPEND OTHER_SOURCES video-sdl.cc event_loop-sdl.cc)
  list(APPEND OTHER_HEADERS video-sdl.h event_loop-sdl.h)
endif()

if(ENABLE_SDL2_MIXER AND SDL2_MIXER_FOUND)
  include_directories(${INCLUDE_DIRECTORIES} ${SDL2_MIXER_INCLUDE_DIR})
  list(APPEND OTHER_SOURCES audio-sdlmixer.cc)
  list(APPEND OTHER_HEADERS audio-sdlmixer.h)
else()
  list(APPEND OTHER_SOURCES audio-dummy.cc)
  list(APPEND OTHER_HEADERS audio-dummy.h )
endif()

if(ENABLE_XMP AND XMP_FOUND)
  add_definitions(/DENABLE_XMP)
  include_directories(${INCLUDE_DIRECTORIES} ${XMP_INCLUDE_DIR})
endif()

set(FREESERF_SOURCES freeserf.cc ${OTHER_SOURCES})
set(FREESERF_HEADERS freeserf.h ${OTHER_HEADERS})
add_executable(FreeSerf MACOSX_BUNDLE WIN32 ${FREESERF_SOURCES} ${FREESERF_HEADERS})
target_check_style(FreeSerf)
target_link_libraries(FreeSerf game)
if(SDL2_FOUND)
  target_link_libraries(FreeSerf ${SDL2_LIBRARY})
  target_link_libraries(FreeSerf ${SDL2_MAIN_LIBRARY})
  if(SDL2_MIXER_FOUND)
    target_link_libraries(FreeSerf ${SDL2_MIXER_LIBRARY})
  endif()
endif()
if(XMP_FOUND)
  target_link_libraries(FreeSerf ${XMP_LIBRARY})
endif()
install(TARGETS FreeSerf DESTINATION ".")

if(WIN32)
  get_filename_component(SDL2_DIR ${SDL2_LIBRARY} DIRECTORY)
  get_filename_component(SDL2_MIXER_DIR ${SDL2_MIXER_LIBRARY} DIRECTORY)
  get_filename_component(XMP_DIR ${XMP_LIBRARY} DIRECTORY)
  install(CODE "include(BundleUtilities)
                fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/FreeSerf.exe\" \"\" \"${SDL2_DIR};${SDL2_MIXER_DIR};${XMP_DIR}\")")
elseif(APPLE)
  install(CODE "include(BundleUtilities)
                fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/FreeSerf.app\" \"\" \"\")")
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
  set(CPACK_PACKAGE_EXECUTABLES "FreeSerf;FreeSerf")
elseif(APPLE)
  include(CPackDMG)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
else()
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
endif()
include(CPack)
