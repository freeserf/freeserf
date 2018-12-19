#.rst:
# FindSDL2_image
# --------------
#
# Locate SDL2_image library
#
# This module defines:
#
# ::
#
#   SDL2_IMAGE_LIBRARIES, the name of the library to link against
#   SDL2_IMAGE_INCLUDE_DIRS, where to find the headers
#   SDL2_IMAGE_FOUND, if false, do not try to link against
#   SDL2_IMAGE_VERSION_STRING - human-readable string containing the
#                               version of SDL2_image
#
#
#
# $SDL2DIR is an environment variable that would correspond to the
# ./configure --prefix=$SDL2DIR used in building SDL2.
#
# Created by Eric Wing.  This was influenced by the FindSDL.cmake
# module, but with modifications to recognize OS X frameworks and
# additional Unix paths (FreeBSD, etc).

#=============================================================================
# Copyright 2005-2009 Kitware, Inc.
# Copyright 2012 Benjamin Eikel
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

find_package(SDL2 REQUIRED)

if(NOT SDL2_IMAGE_INCLUDE_DIR AND SDL2IMAGE_INCLUDE_DIR)
  set(SDL2_IMAGE_INCLUDE_DIR ${SDL2IMAGE_INCLUDE_DIR} CACHE PATH "directory cache
entry initialized from old variable name")
endif()
find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h
  HINTS
    ${SDL2_INCLUDE_DIR}
    ENV SDL2IMAGEDIR
    ENV SDL2DIR
  PATH_SUFFIXES SDL2
                # path suffixes to search inside ENV{SDL2DIR}
                include/SDL2 include
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VC_LIB_PATH_SUFFIX lib/x64)
else()
  set(VC_LIB_PATH_SUFFIX lib/x86)
endif()

list(GET SDL2_LIBRARY 0 SDL2_LIBRARY_PATH)
get_filename_component(SDL2_LIBRARY_DIR ${SDL2_LIBRARY_PATH} DIRECTORY)

if(NOT SDL2_IMAGE_LIBRARY AND SDL2IMAGE_LIBRARY)
  set(SDL2_IMAGE_LIBRARY ${SDL2IMAGE_LIBRARY} CACHE FILEPATH "file cache entry
initialized from old variable name")
endif()
find_library(SDL2_IMAGE_LIBRARY
             NAMES SDL2_image
             PATHS ${SDL2_LIBRARY_DIR}
             NO_CMAKE_PATH
             NO_DEFAULT_PATH)

if(NOT SDL2_IMAGE_LIBRARY)
  find_library(SDL2_IMAGE_LIBRARY
               NAMES SDL2_image
               HINTS
                 ${SDL2_LIBRARY_DIR}
                 ENV SDL2IMAGEDIR
                 ENV SDL2DIR
               PATH_SUFFIXES
                 lib
                 ${VC_LIB_PATH_SUFFIX})
endif()

list(GET SDL2_LIBRARY_DEBUG 0 SDL2_LIBRARY_DEBUG_PATH)
get_filename_component(SDL2_LIBRARY_DEBUG_DIR ${SDL2_LIBRARY_DEBUG_PATH} DIRECTORY)

find_library(SDL2_IMAGE_LIBRARY_DEBUG
             NAMES SDL2_image
             PATHS ${SDL2_LIBRARY_DEBUG_DIR}
             NO_CMAKE_PATH
             NO_DEFAULT_PATH)

if(NOT SDL2_IMAGE_LIBRARY_DEBUG)
  find_library(SDL2_IMAGE_LIBRARY_DEBUG
               NAMES SDL2_image
               HINTS
                 ${SDL2_LIBRARY_DEBUG_DIR}
                 ENV SDL2IMAGEDIR
                 ENV SDL2DIR
               PATH_SUFFIXES
                 lib
                 ${VC_LIB_PATH_SUFFIX})
endif()

if(NOT SDL2_IMAGE_LIBRARY_DEBUG)
  set(SDL2_IMAGE_LIBRARY_DEBUG ${SDL2_IMAGE_LIBRARY})
endif()

sdl2_extract_version(SDL2_IMAGE_VERSION_STRING "${SDL2_MIXER_INCLUDE_DIR}/SDL_mixer.h" "mixer")

set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARY})
set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL2_IMAGE
                                  REQUIRED_VARS SDL2_IMAGE_LIBRARIES SDL2_IMAGE_INCLUDE_DIRS
                                  VERSION_VAR SDL2_IMAGE_VERSION_STRING)
