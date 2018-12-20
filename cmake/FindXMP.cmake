find_path(XMP_INCLUDE_DIR xmp.h
  HINTS
    ENV XMPDIR
  PATH_SUFFIXES
    XMP include
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VC_LIB_PATH_SUFFIX lib/x64)
else()
  set(VC_LIB_PATH_SUFFIX lib/x86)
endif()

find_library(XMP_LIBRARY
  NAMES xmp xmp-lite libxmp libxmp-lite
  HINTS
    ENV XMPDIR
  PATH_SUFFIXES
    lib ${VC_LIB_PATH_SUFFIX})

set(XMP_LIBRARY_DEBUG ${XMP_LIBRARY} CACHE STRING "XMP Debug Library")
if(DEFINED _VCPKG_INSTALLED_DIR)
  if(XMP_LIBRARY MATCHES "/debug/lib/")
    get_filename_component(XMP_LIBRARY_DIR ${XMP_LIBRARY} DIRECTORY)
    get_filename_component(XMP_LIBRARY_DIR ${XMP_LIBRARY_DIR} DIRECTORY)
    get_filename_component(XMP_LIBRARY_DIR ${XMP_LIBRARY_DIR} DIRECTORY)
    set(XMP_LIBRARY_DIR "${XMP_LIBRARY_DIR}/lib")
    unset(XMP_LIBRARY CACHE)
    find_library(XMP_LIBRARY
      NAMES libxmp-lite
      HINTS
        ${XMP_LIBRARY_DIR}
      PATH_SUFFIXES
        lib ${VC_LIB_PATH_SUFFIX}
      NO_CMAKE_PATH
      NO_DEFAULT_PATH)
    message(STATUS "XMP_LIBRARY = ${XMP_LIBRARY}")
  endif()
endif()

if(XMP_INCLUDE_DIR AND EXISTS "${XMP_INCLUDE_DIR}/xmp.h")
  file(STRINGS "${XMP_INCLUDE_DIR}/xmp.h" XMP_VER_MAJOR_LINE REGEX "^#define[ \t]+XMP_VER_MAJOR[ \t]+[0-9]+$")
  file(STRINGS "${XMP_INCLUDE_DIR}/xmp.h" XMP_VER_MINOR_LINE REGEX "^#define[ \t]+XMP_VER_MINOR[ \t]+[0-9]+$")
  file(STRINGS "${XMP_INCLUDE_DIR}/xmp.h" XMP_VER_RELEASE_LINE REGEX "^#define[ \t]+XMP_VER_RELEASE[ \t]+[0-9]+$")
  string(REGEX REPLACE "^#define[ \t]+XMP_VER_MAJOR[ \t]+([0-9]+)$" "\\1" XMP_VER_MAJOR "${XMP_VER_MAJOR_LINE}")
  string(REGEX REPLACE "^#define[ \t]+XMP_VER_MINOR[ \t]+([0-9]+)$" "\\1" XMP_VER_MINOR "${XMP_VER_MINOR_LINE}")
  string(REGEX REPLACE "^#define[ \t]+XMP_VER_RELEASE[ \t]+([0-9]+)$" "\\1" XMP_VER_RELEASE "${XMP_VER_RELEASE_LINE}")
  set(XMP_VERSION_STRING ${XMP_VER_MAJOR}.${XMP_VER_MINOR}.${XMP_VER_RELEASE})
  unset(XMP_VER_MAJOR_LINE)
  unset(XMP_VER_MINOR_LINE)
  unset(XMP_VER_RELEASE_LINE)
  unset(XMP_VER_MAJOR)
  unset(XMP_VER_MINOR)
  unset(XMP_VER_RELEASE)
endif()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(XMP
                                  REQUIRED_VARS XMP_LIBRARY XMP_INCLUDE_DIR
                                  VERSION_VAR XMP_VERSION_STRING)
