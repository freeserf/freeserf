#ifndef _CONFIG_H
#define _CONFIG_H

#	if !defined(_STDINT_H_) && (!defined(HAVE_STDINT_H) || !_HAVE_STDINT_H)
#	if defined(__GNUC__) || defined(__DMC__) || defined(__WATCOMC__)
#		define HAVE_STDINT_H   1
#	elif defined(_MSC_VER)
typedef signed __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef signed __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef signed __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#	ifndef _UINTPTR_T_DEFINED
#		ifdef  _WIN64
typedef unsigned __int64 uintptr_t;
#		else
typedef unsigned int uintptr_t;
#	endif
#		define _UINTPTR_T_DEFINED
#	endif

#	include <stdio.h>
#	include <stdarg.h>

#	ifndef snprintf
#		define snprintf c99_snprintf

inline int c99_vsnprintf(char* str, size_t size, const char* format, va_list ap)
{
	int count = -1;

	if (size != 0)
		count = _vsnprintf_s(str, size, _TRUNCATE, format, ap);
	if (count == -1)
		count = _vscprintf(format, ap);

	return count;
}

inline int c99_snprintf(char* str, size_t size, const char* format, ...)
{
	int count;
	va_list ap;

	va_start(ap, format);
	count = c99_vsnprintf(str, size, format, ap);
	va_end(ap);

	return count;
}

#		endif
#	endif
#endif

#endif /* !_CONFIG_H */
