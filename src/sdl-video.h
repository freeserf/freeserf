/* sdl-video.h */

#ifndef _SDL_VIDEO_H
#define _SDL_VIDEO_H

#include <stdint.h>

#include "SDL.h"

#include "gfx.h"


typedef struct surface surface_t;

int sdl_init();
void sdl_deinit();
int sdl_set_resolution(int width, int height, int fullscreen);
frame_t *sdl_get_screen_frame();
void sdl_frame_init(frame_t *frame, int x, int y, int width, int height, frame_t *dest);
int sdl_frame_get_width(const frame_t *frame);
int sdl_frame_get_height(const frame_t *frame);

void sdl_draw_transp_sprite(const sprite_t *sprite, int x, int y, int use_off, int y_off, int color_off, frame_t *dest);
void sdl_draw_waves_sprite(const sprite_t *sprite, const sprite_t *mask, int x, int y, int mask_off, frame_t *dest);
void sdl_draw_sprite(const sprite_t *sprite, int x, int y, frame_t *dest);
void sdl_draw_overlay_sprite(const sprite_t *sprite, int x, int y, int y_off, frame_t *dest);
surface_t *sdl_draw_masked_sprite(const sprite_t *sprite, int x, int y, const sprite_t *mask, surface_t *surface, frame_t *dest);
void sdl_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h);
void sdl_draw_rect(int x, int y, int width, int height, int color, frame_t *dest);
void sdl_fill_rect(int x, int y, int width, int height, int color, frame_t *dest);
void sdl_set_palette(const uint8_t *palette);
void sdl_mark_dirty(int x, int y, int width, int height);
void sdl_swap_buffers();


#endif /* ! _SDL_VIDEO_H */
