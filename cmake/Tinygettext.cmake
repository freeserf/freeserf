# tinygettext (https://github.com/tinygettext/tinygettext), the translations
# of the texts from gettext .po files. Its sources are downloaded at
# configure time and built here as a static library: UTF-8 only (no
# iconv), without its install rules.

include(FetchContent)

# The files get the time of the extraction (CMake 3.24).
if(NOT CMAKE_VERSION VERSION_LESS 3.24)
  set(TINYGETTEXT_TIMESTAMP DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

FetchContent_Declare(tinygettext
  URL https://github.com/tinygettext/tinygettext/archive/0ebef71ef7aa07ec6d026164fbe9472e6f82a583.tar.gz
  URL_HASH SHA256=f7da276a77e63678a1968aa818790165280529535c1e527fb510a3a863b5aa9d
  ${TINYGETTEXT_TIMESTAMP}
  # No CMakeLists.txt there: only the sources are made available.
  SOURCE_SUBDIR sources-only)
FetchContent_MakeAvailable(tinygettext)

file(GLOB TINYGETTEXT_SOURCES "${tinygettext_SOURCE_DIR}/src/*.cpp")
add_library(tinygettext STATIC ${TINYGETTEXT_SOURCES})
set_target_properties(tinygettext PROPERTIES
                      CXX_STANDARD 17
                      CXX_STANDARD_REQUIRED ON
                      FOLDER "External")
target_include_directories(tinygettext SYSTEM PUBLIC
                           "${tinygettext_SOURCE_DIR}/include")
target_compile_definitions(tinygettext PUBLIC TINYGETTEXT_UTF8_ONLY)
if(WIN32)
  target_compile_definitions(tinygettext PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
