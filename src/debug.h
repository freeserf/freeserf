/* debug.h */

#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef NDEBUG
# define NOT_REACHED()  do { fprintf(stderr, "NOT_REACHED at line %i of %s\n", __LINE__, __FILE__); abort(); } while (0)
#else
# define NOT_REACHED()  do { } while (0)
#endif

#endif /* ! _DEBUG_H */
