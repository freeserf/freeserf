/*
 * sdl-video.c - SDL graphics rendering
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <string.h>
#include <signal.h>

#include "SDL.h"

#include "sdl-video.h"
#include "gfx.h"
#include "misc.h"
#include "version.h"
#include "log.h"
#include "data.h"


static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *screen_texture;
static int bpp = 32;
static Uint32 Rmask = 0xFF000000;
static Uint32 Gmask = 0x00FF0000;
static Uint32 Bmask = 0x0000FF00;
static Uint32 Amask = 0x000000FF;
static Uint32 pixel_format = SDL_PIXELFORMAT_RGBA8888;

static frame_t screen;
static int is_fullscreen;
static SDL_Color pal_colors[256];
static SDL_Cursor *cursor = NULL;


int
sdl_init()
{
	/* Initialize defaults and Video subsystem */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		LOGE("sdl-video", "Unable to initialize SDL: %s.", SDL_GetError());
		return -1;
	}

	/* Display program name and version in caption */
	char caption[64];
	snprintf(caption, 64, "freeserf %s", FREESERF_VERSION);

	/* Create window and renderer */
	window = SDL_CreateWindow(caption,
				  SDL_WINDOWPOS_UNDEFINED,
				  SDL_WINDOWPOS_UNDEFINED,
				  800, 600, SDL_WINDOW_RESIZABLE);
	if (window == NULL) {
		LOGE("sdl-video", "Unable to create SDL window: %s.", SDL_GetError());
		return -1;
	}

	/* Create renderer for window */
	renderer = SDL_CreateRenderer(window, -1, 0);
	if (renderer == NULL) {
		LOGE("sdl-video", "Unable to create SDL renderer: %s.", SDL_GetError());
		return -1;
	}

	/* Determine optimal pixel format for current window */
	SDL_RendererInfo render_info = {0};
	SDL_GetRendererInfo(renderer, &render_info);
	for (Uint32 i = 0; i < render_info.num_texture_formats; i++) {
		Uint32 format = render_info.texture_formats[i];
		int bpp = SDL_BITSPERPIXEL(format);
		if (32 == bpp) {
			pixel_format = format;
			break;
		}
	}
	SDL_PixelFormatEnumToMasks(pixel_format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);

	/* Set scaling mode */
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	/* Exit on signals */
	signal(SIGINT, exit);
	signal(SIGTERM, exit);

	return 0;
}

void
sdl_deinit()
{
	sdl_set_cursor(NULL);
	SDL_Quit();
}

static SDL_Surface *
sdl_create_surface(int width, int height)
{
	SDL_Surface *surf = SDL_CreateRGBSurface(0, width, height, bpp,
						 Rmask, Gmask, Bmask, Amask);
	if (surf == NULL) {
		LOGE("sdl-video", "Unable to create SDL surface: %s.", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	return surf;
}

void
video_actualize_clipping(frame_t *frame)
{
	SDL_Rect rect = {frame->clip.x, frame->clip.y, frame->clip.w, frame->clip.h};
	SDL_SetClipRect((SDL_Surface*)frame->surf, &rect);
}

int
sdl_set_resolution(int width, int height, int fullscreen)
{
	/* Set fullscreen mode */
	int r = SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (r < 0) {
		LOGE("sdl-video", "Unable to set window fullscreen: %s.", SDL_GetError());
		return -1;
	}

	/* Allocate new screen surface and texture */
	if (screen.surf != NULL) SDL_FreeSurface((SDL_Surface*)screen.surf);
	screen.surf = sdl_create_surface(width, height);

	if (screen_texture != NULL) SDL_DestroyTexture(screen_texture);
	screen_texture = SDL_CreateTexture(renderer, pixel_format,
					   SDL_TEXTUREACCESS_STREAMING,
					   width, height);
	if (screen_texture == NULL) {
		LOGE("sdl-video", "Unable to create SDL texture: %s.", SDL_GetError());
		return -1;
	}

	/* Set logical size of screen */
	r = SDL_RenderSetLogicalSize(renderer, width, height);
	if (r < 0) {
		LOGE("sdl-video", "Unable to set logical size: %s.", SDL_GetError());
		return -1;
	}

	/* Reset clipping */
	screen.clip.x = 0;
	screen.clip.y = 0;
	screen.clip.w = width;
	screen.clip.h = height;
	video_actualize_clipping(&screen);

	is_fullscreen = fullscreen;

	return 0;
}

void
sdl_get_resolution(int *width, int *height)
{
	SDL_GL_GetDrawableSize(window, width, height);
}

int
sdl_is_fullscreen_possible()
{
	return 1;
}

int
sdl_set_fullscreen(int enable)
{
	frame_t *screen = sdl_get_screen_frame();
	int width = sdl_frame_get_width(screen);
	int height = sdl_frame_get_height(screen);
	return sdl_set_resolution(width, height, enable);
}

int
sdl_is_fullscreen()
{
	return is_fullscreen;
}

frame_t *
sdl_get_screen_frame()
{
	return &screen;
}

void
sdl_frame_init(frame_t *frame, int x, int y, int width, int height, frame_t *dest)
{
	frame->surf = (dest != NULL) ? dest->surf : sdl_create_surface(width, height);
	frame->clip.x = x;
	frame->clip.y = y;
	frame->clip.w = width;
	frame->clip.h = height;
}

void
sdl_frame_deinit(frame_t *frame)
{
	SDL_FreeSurface((SDL_Surface*)frame->surf);
}

int
sdl_frame_get_width(const frame_t *frame)
{
	return frame->clip.w;
}

int
sdl_frame_get_height(const frame_t *frame)
{
	return frame->clip.h;
}

void
sdl_warp_mouse(int x, int y)
{
	SDL_WarpMouseInWindow(NULL, x, y);
}


static SDL_Surface *
create_surface_from_data(void *data, int width, int height)
{
	/* Create sprite surface */
	SDL_Surface *surf =
		SDL_CreateRGBSurfaceFrom(data, width, height,
					 32, 4 * width,
					 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if (surf == NULL) {
		LOGE("sdl-video", "Unable to create sprite surface: %s.",
		     SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/* Covert to screen format */
	SDL_Surface *surf_screen =
		SDL_ConvertSurface(surf, ((SDL_Surface*)screen.surf)->format, 0);
	if (surf_screen == NULL) {
		LOGE("sdl-video", "Unable to convert sprite surface: %s.",
		     SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_FreeSurface(surf);

	return surf_screen;
}

static SDL_Surface *
create_surface_from_sprite(const sprite_t *sprite)
{
	void *data = sprite->data;
	uint width = sprite->width;
	uint height = sprite->height;

	return create_surface_from_data(data, width, height);
}

void
sdl_draw_sprite(const sprite_t *sprite, int x, int y, int y_offset, frame_t *dest)
{
	x += dest->clip.x;
	y += dest->clip.y;

	SDL_Surface *surf = create_surface_from_sprite(sprite);

	SDL_Rect src_rect = { 0, y_offset, surf->w, surf->h - y_offset };
	SDL_Rect dest_rect = { x, y + y_offset, 0, 0 };

	video_actualize_clipping(dest);

	/* Blit sprite */
	int r = SDL_BlitSurface(surf, &src_rect, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s.", SDL_GetError());
	}

	/* Clean up */
	SDL_FreeSurface(surf);

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y + y_off, surf->w, surf->h - y_off, 72, dest);
#endif
}

void
sdl_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h)
{
	int x = dx + dest->clip.x;
	int y = dy + dest->clip.y;

	SDL_Rect dest_rect = { x, y, 0, 0 };
	SDL_Rect src_rect = { sx, sy, w, h };

	video_actualize_clipping(dest);

	int r = SDL_BlitSurface((SDL_Surface*)src->surf, &src_rect, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s", SDL_GetError());
	}
}

void
sdl_fill_rect(int x, int y, int width, int height, const color_t *color, frame_t *dest)
{
	SDL_Rect rect = {
		x + dest->clip.x,
		y + dest->clip.y,
		width, height
	};

	video_actualize_clipping(dest);

	/* Fill rectangle */
	int r = SDL_FillRect((SDL_Surface*)dest->surf, &rect, SDL_MapRGBA(((SDL_Surface*)dest->surf)->format,
									  color->r, color->g, color->b, 0xff));
	if (r < 0) {
		LOGE("sdl-video", "FillRect error: %s.", SDL_GetError());
	}
}

void
sdl_swap_buffers()
{
	SDL_UpdateTexture(screen_texture, NULL,
			  ((SDL_Surface*)screen.surf)->pixels,
			  ((SDL_Surface*)screen.surf)->pitch);

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void
sdl_set_palette(const uint8_t *palette)
{
	/* Fill palette */
	for (int i = 0; i < 256; i++) {
		pal_colors[i].r = palette[i*3];
		pal_colors[i].g = palette[i*3+1];
		pal_colors[i].b = palette[i*3+2];
		pal_colors[i].a = SDL_ALPHA_OPAQUE;
	}
}

void
sdl_set_cursor(const sprite_t *sprite)
{
	if (cursor != NULL) {
		SDL_SetCursor(NULL);
		SDL_FreeCursor(cursor);
		cursor = NULL;
	}

	if (sprite == NULL) return;

	SDL_Surface *surface = create_surface_from_sprite(sprite);
	cursor = SDL_CreateColorCursor(surface, 8, 8);
	SDL_SetCursor(cursor);
}
