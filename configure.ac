#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([freeserf], [0.2], [https://github.com/freeserf/freeserf/issues])
AC_CONFIG_SRCDIR([src/freeserf.cc])
AC_CONFIG_HEADERS([config.h])
AC_REQUIRE_AUX_FILE([tap-driver.sh])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
PKG_CHECK_MODULES([SDL2], [sdl2])
PKG_CHECK_MODULES([SDL2_mixer], [SDL2_mixer], [have_sdl2_mixer=yes], [have_sdl2_mixer=no])

# Checks for header files.
AC_HEADER_ASSERT
AC_CHECK_HEADERS([byteswap.h endian.h stdint.h getopt.h sys/endian.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN
AC_SYS_LARGEFILE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Check SDL2_mixer support
AC_MSG_CHECKING([whether to enable SDL2_mixer support])
AC_ARG_ENABLE([sdl2-mixer], [AC_HELP_STRING([--enable-sdl2-mixer],
			   [enable SDL2_mixer support (music/sound effects)])],
			   [enable_sdl2_mixer=$enableval], [enable_sdl2_mixer=maybe])
AS_IF([test "x$enable_sdl2_mixer" != xno], [
	AS_IF([test $have_sdl2_mixer = yes], [
		AC_DEFINE([ENABLE_SDL2_MIXER], 1, [Define to 1 to enable SDL2_mixer support])
		AC_MSG_RESULT([yes])
		enable_sdl2_mixer=yes
	], [
		AC_MSG_RESULT([missing dependencies])
		AS_IF([test "x$enable_sdl2_mixer" = xyes], [
			AC_MSG_ERROR([missing dependencies for SDL2_mixer])
		])
		enable_sdl2_mixer=no
	])
], [
	AC_MSG_RESULT([no])
	enable_sdl2_mixer=no
])
AM_CONDITIONAL([ENABLE_SDL2_MIXER], [test "x$enable_sdl2_mixer" = xyes])

# Check debug mode
AC_MSG_CHECKING([whether to enable debug mode])
debug_default="no"
AC_ARG_ENABLE([debug], [AC_HELP_STRING([--enable-debug],
	[enable debug mode])],,[enable_debug=$debug_default])
AS_IF([test "x$enable_debug" != xno], [
	CXXFLAGS="$CXXFLAGS -ggdb -Wall"
	AC_SUBST([CXXFLAGS])
	AC_MSG_RESULT([yes])
	], [
	CXXFLAGS="$CXXFLAGS -DNDEBUG"
	AC_SUBST([CXXFLAGS])
	AC_MSG_RESULT([no])])

# Check profiling mode
AC_MSG_CHECKING([whether to enable profile mode])
profile_default="no"
AC_ARG_ENABLE([profile], [AC_HELP_STRING([--enable-profile],
	[enable profile mode])],,[enable_profile=$profile_default])
AS_IF([test "x$enable_profile" != xno], [
	CXXFLAGS="$CXXFLAGS -p"
	LDFLAGS="$LDFLAGS -pg"
	AC_SUBST([CXXFLAGS])
	AC_MSG_RESULT([yes])
	], [
	AC_MSG_RESULT([no])])

AC_CONFIG_FILES([
	Makefile
])
AC_OUTPUT


echo "
 $PACKAGE_NAME $VERSION

    prefix:		${prefix}
    compiler (c++):	${CXX}
    cxxflags:		${CXXFLAGS}
    ldflags:		${LDFLAGS}

    SDL2_mixer:		${enable_sdl2_mixer}
"
