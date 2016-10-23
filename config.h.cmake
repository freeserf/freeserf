#include <definitions.h>

#cmakedefine ENABLE_SDL2_MIXER

#cmakedefine CMAKE_HAVE_BYTESWAP_H
#ifdef CMAKE_HAVE_BYTESWAP_H
    #define HAVE_BYTESWAP_H 1
#else
    #undef HAVE_BYTESWAP_H
#endif

#cmakedefine CMAKE_HAVE_ENDIAN_H
#ifdef CMAKE_HAVE_ENDIAN_H
    #define HAVE_ENDIAN_H 1
#else
    #undef HAVE_ENDIAN_H
#endif

#cmakedefine CMAKE_HAVE_FCNTL_H
#ifdef CMAKE_HAVE_FCNTL_H
    #define HAVE_FCNTL_H 1
#else
    #undef HAVE_FCNTL_H
#endif

#cmakedefine CMAKE_HAVE_INTTYPES_H
#ifdef CMAKE_HAVE_INTTYPES_H
    #define HAVE_INTTYPES_H 1
#else
    #undef HAVE_INTTYPES_H
#endif

#cmakedefine CMAKE_HAVE_MEMORY_H
#ifdef CMAKE_HAVE_MEMORY_H
    #define HAVE_MEMORY_H 1
#else
    #undef HAVE_MEMORY_H
#endif

#cmakedefine CMAKE_HAVE_STDINT_H
#ifdef CMAKE_HAVE_STDINT_H
    #define HAVE_STDINT_H 1
#else
    #undef HAVE_STDINT_H
#endif

#cmakedefine CMAKE_HAVE_STDLIB_H
#ifdef CMAKE_HAVE_STDLIB_H
    #define HAVE_STDLIB_H 1
#else
    #undef HAVE_STDLIB_H
#endif

#cmakedefine CMAKE_HAVE_STRINGS_H
#ifdef CMAKE_HAVE_STRINGS_H
    #define HAVE_STRINGS_H 1
#else
    #undef HAVE_STRINGS_H
#endif

#cmakedefine CMAKE_HAVE_STRING_H
#ifdef CMAKE_HAVE_STRING_H
    #define HAVE_STRING_H 1
#else
    #undef HAVE_STRING_H
#endif

#cmakedefine CMAKE_HAVE_SYS_ENDIAN_H
#ifdef CMAKE_HAVE_SYS_ENDIAN_H
    #define HAVE_SYS_ENDIAN_H 1
#else
    #undef HAVE_SYS_ENDIAN_H
#endif

#cmakedefine CMAKE_HAVE_SYS_MMAN_H
#ifdef CMAKE_HAVE_SYS_MMAN_H
    #define HAVE_SYS_MMAN_H 1
#else
    #undef HAVE_SYS_MMAN_H
#endif

#cmakedefine CMAKE_HAVE_SYS_PARAM_H
#ifdef CMAKE_HAVE_SYS_PARAM_H
    #define HAVE_SYS_PARAM_H 1
#else
    #undef HAVE_SYS_PARAM_H
#endif

#cmakedefine CMAKE_HAVE_SYS_STAT_H
#ifdef CMAKE_HAVE_SYS_STAT_H
    #define HAVE_SYS_STAT_H 1
#else
    #undef HAVE_SYS_STAT_H
#endif

#cmakedefine CMAKE_HAVE_SYS_TYPES_H
#ifdef CMAKE_HAVE_SYS_TYPES_H
    #define HAVE_SYS_TYPES_H 1
#else
    #undef HAVE_SYS_TYPES_H
#endif

#cmakedefine CMAKE_HAVE_UNISTD_H
#ifdef CMAKE_CMAKE_HAVE_UNISTD_H
    #define CMAKE_HAVE_UNISTD_H 1
#else
    #undef CMAKE_HAVE_UNISTD_H
#endif



#cmakedefine CMAKE_HAVE_ATEXIT
#ifdef CMAKE_HAVE_ATEXIT
    #define HAVE_ATEXIT 1
#else
    #undef HAVE_ATEXIT
#endif

#cmakedefine CMAKE_HAVE_GETPAGESIZE
#ifdef CMAKE_HAVE_GETPAGESIZE
    #define HAVE_GETPAGESIZE 1
#else
    #undef HAVE_GETPAGESIZE
#endif

#cmakedefine CMAKE_HAVE_MEMSET
#ifdef CMAKE_HAVE_MEMSET
    #define HAVE_MEMSET 1
#else
    #undef HAVE_MEMSET
#endif

#cmakedefine CMAKE_HAVE_MMAP
#ifdef CMAKE_HAVE_MMAP
    #define HAVE_MMAP 1
#else
    #undef HAVE_MMAP
#endif

#cmakedefine CMAKE_HAVE_MUNMAP
#ifdef CMAKE_HAVE_MUNMAP
    #define HAVE_MUNMAP 1
#else
    #undef HAVE_MUNMAP
#endif

#cmakedefine CMAKE_HAVE_STRTOL
#ifdef CMAKE_HAVE_STRTOL
    #define HAVE_STRTOL 1
#else
    #undef HAVE_STRTOL
#endif



#cmakedefine NDEBUG
#cmakedefine PACKAGE
#cmakedefine PACKAGE_BUGREPORT
#cmakedefine PACKAGE_NAME
#cmakedefine PACKAGE_STRING
#cmakedefine PACKAGE_TARNAME
#cmakedefine PACKAGE_URL
#cmakedefine PACKAGE_VERSION
#cmakedefine RETSIGTYPE
#cmakedefine STDC_HEADERS
#cmakedefine VERSION

#cmakedefine _UINT32_T
#cmakedefine _UINT8_T
#cmakedefine const
#cmakedefine int16_t
#cmakedefine int32_t
#cmakedefine int8_t
#cmakedefine off_t
#cmakedefine size_t
#cmakedefine uint16_t
#cmakedefine uint32_t
#cmakedefine uint8_t
