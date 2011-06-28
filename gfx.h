/* gfx.h */

#ifndef _GFX_H
#define _GFX_H

#include <stdlib.h>

void gfx_data_fixup();
void gfx_unpack_transparent_sprite(void *dest, const void *src, size_t destlen, int offset);
void gfx_unpack_overlay_sprite(void *dest, const void *src, size_t destlen);
void gfx_unpack_mask_sprite(void *dest, const void *src, size_t destlen);


#endif /* ! _GFX_H */
