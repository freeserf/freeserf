/* gfx.h */

#ifndef _GFX_H
#define _GFX_H

#include <stdint.h>

#include "SDL.h"


#define MAP_TILE_WIDTH   32
#define MAP_TILE_HEIGHT  20


typedef struct {
	SDL_Surface *surf;
	SDL_Rect clip;
} frame_t;

typedef struct {
	int8_t b_x;
	int8_t b_y;
	uint16_t w;
	uint16_t h;
	int16_t x;
	int16_t y;
} sprite_t;


int gfx_load_file(const char *path);
void gfx_unload();
void *gfx_get_data_object(int index, size_t *size);
void gfx_draw_string(int x, int y, int color, int shadow, frame_t *dest, const char *str);
void gfx_draw_number(int x, int y, int color, int shadow, frame_t *dest, int n);
void gfx_draw_sprite(int x, int y, int sprite, frame_t *dest);
void gfx_draw_transp_sprite(int x, int y, int sprite, frame_t *dest);
void gfx_fill_rect(int x, int y, int width, int height, int color, frame_t *dest);
void gfx_data_fixup();
void gfx_set_palette(int palette);

void gfx_unpack_transparent_sprite(void *dest, const void *src, size_t destlen, int offset);
void gfx_unpack_overlay_sprite(void *dest, const void *src, size_t destlen);
void gfx_unpack_mask_sprite(void *dest, const void *src, size_t destlen);


#endif /* ! _GFX_H */
