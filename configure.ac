#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.64)
AC_INIT([freeserf], [0.1], [jonlst@gmail.com])
AC_CONFIG_SRCDIR([src/freeserf.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC_C99

# Checks for libraries.
AM_PATH_SDL

# Checks for header files.
AC_HEADER_ASSERT
AC_HEADER_STDC
AC_CHECK_HEADERS([byteswap.h endian.h fcntl.h stdint.h stdlib.h string.h unistd.h sys/endian.h sys/mman.h])

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

# Checks for library functions.
AC_FUNC_MMAP
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([atexit memset munmap strtol])

# Check debug mode
AC_MSG_CHECKING([whether to enable debug mode])
debug_default="no"
AC_ARG_ENABLE([debug], [AC_HELP_STRING([--enable-debug],
	[enable debug mode])],,[enable_debug=$debug_default])
AS_IF([test "x$enable_debug" != xno], [
	CFLAGS="$CFLAGS -pedantic -Wall -ggdb"
	AC_SUBST([CFLAGS])
	AC_MSG_RESULT([yes])
	], [
	CFLAGS="$CFLAGS -DNDEBUG"
	AC_SUBST([CFLAGS])
	AC_MSG_RESULT([no])])

# Check profiling mode
AC_MSG_CHECKING([whether to enable profile mode])
profile_default="no"
AC_ARG_ENABLE([profile], [AC_HELP_STRING([--enable-profile],
	[enable profile mode])],,[enable_profile=$profile_default])
AS_IF([test "x$enable_profile" != xno], [
	CFLAGS="$CFLAGS -p"
	LDFLAGS="$LDFLAGS -pg"
	AC_SUBST([CFLAGS])
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
    compiler:		${CC}
    cflags:		${CFLAGS}
    ldflags:		${LDFLAGS}
"
