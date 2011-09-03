/* misc.h */

#ifndef _MISC_H
#define _MISC_H

#define BIT(n)          (1 << (n))
#define BIT_TEST(x, n)  ((x) & BIT(n))

#define max(x,y)      (((x) > (y)) ? (x) : (y))
#define min(x,y)      (((x) < (y)) ? (x) : (y))
#define clamp(l,x,h)  (max((l),min((x),(h))))

typedef unsigned int uint;

#endif /* ! _MISC_H */
