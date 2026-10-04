function(git_make_version var repo def_ver)

  # Without git or outside a git checkout (e.g. a release tarball) the
  # default version is used.
  set(${var} "${def_ver}" PARENT_SCOPE)

  find_package(Git)

  if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --tags --dirty
                    WORKING_DIRECTORY ${repo}
                    RESULT_VARIABLE _GIT_RESULT
                    OUTPUT_VARIABLE _VERSION
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _GIT_RESULT EQUAL 0 OR "${_VERSION}" STREQUAL "")
      return()
    endif()
    string(FIND "${_VERSION}" "." _VERSION_FULL)
    if(_VERSION_FULL STREQUAL "-1")
      string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" _VERSION_MAJOR "${def_ver}")
      string(REGEX REPLACE "^[0-9]+\\.([0-9]+).*" "\\1" _VERSION_MINOR "${def_ver}")
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
                      WORKING_DIRECTORY ${repo}
                      OUTPUT_VARIABLE _VERSION_PATCH
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
      string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" _VERSION_MAJOR "${_VERSION}")
      string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" _VERSION_MINOR "${_VERSION}")
      # The patch is the commit after the tag; a build of the tag itself
      # ("v0.4", "v0.4-dirty") has none.
      if(_VERSION MATCHES "-g([0-9a-f]+)")
        set(_VERSION_PATCH "${CMAKE_MATCH_1}")
      else()
        set(_VERSION_PATCH "0")
      endif()
    endif()
    set(${var} "${_VERSION_MAJOR}.${_VERSION_MINOR}.${_VERSION_PATCH}" PARENT_SCOPE)
  endif()

endfunction()
