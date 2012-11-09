/* debug.h */

#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>

#include "log.h"

#ifndef NDEBUG
# define NOT_REACHED()  do { LOGE("debug", "NOT_REACHED at line %i of %s.", __LINE__, __FILE__); abort(); } while (0)
#else
# define NOT_REACHED()  do { } while (0)
#endif

#endif /* ! _DEBUG_H */
