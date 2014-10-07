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

#include "freeserf_endian.h"
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


typedef struct {
	SDL_Surface *surf;
} surface_t;


/* Unique identifier for a surface. */
typedef struct {
	const sprite_t *sprite;
	const sprite_t *mask;
	uint offset;
} surface_id_t;

typedef struct surface_ht_entry surface_ht_entry_t;

/* Entry in the hashtable of surfaces. */
struct surface_ht_entry {
	surface_ht_entry_t *next;
	surface_id_t id;
	surface_t *value;
};

/* Hashtable of surfaces, used as sprite cache. */
typedef struct {
	size_t size;
	uint entry_count;
	surface_ht_entry_t **entries;
} surface_ht_t;


/* The sprite cache is divided in three areas for
   different sprite types. */
static surface_ht_t transp_sprite_cache;
static surface_ht_t overlay_sprite_cache;
static surface_ht_t masked_sprite_cache;


/* Calculate hash of surface identifier. */
static uint32_t
surface_id_hash(const surface_id_t *id)
{
	const uint8_t *s = (uint8_t *)id;

	/* FNV-1 */
	uint32_t hash = 2166136261;
	for (int i = 0; i < sizeof(surface_id_t); i++) {
		hash *= 16777619;
		hash ^= s[i];
	}

	return hash;
}

/* Intialize surface hashtable. */
static int
surface_ht_init(surface_ht_t *ht, size_t size)
{
	ht->size = size;
	ht->entries = (surface_ht_entry_t**)calloc(size, sizeof(surface_ht_entry_t *));
	if (ht->entries == NULL) return -1;

	ht->entry_count = 0;

	return 0;
}

/* Return a pointer to the surface pointer associated with
   id. If it does not exist in the table it a new entry is
   created. */
static surface_t **
surface_ht_store(surface_ht_t *ht, const surface_id_t *id)
{
	uint32_t hash = surface_id_hash(id);
	surface_ht_entry_t *entry =
		(surface_ht_entry_t *)&ht->entries[hash % ht->size];

	/* The first entry pointed to is not really an
	   entry but a sentinel. */
	while (entry->next != NULL) {
		entry = entry->next;

		if (memcmp(id, &entry->id, sizeof(surface_id_t)) == 0) {
			return &entry->value;
		}
	}

	surface_ht_entry_t *new_entry = (surface_ht_entry_t*)calloc(1, sizeof(surface_ht_entry_t));
	if (new_entry == NULL) return NULL;

	ht->entry_count += 1;

	entry->next = new_entry;
	memcpy(&new_entry->id, id, sizeof(surface_id_t));

	return &new_entry->value;
}


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

	/* Init sprite cache */
	surface_ht_init(&transp_sprite_cache, 4096);
	surface_ht_init(&overlay_sprite_cache, 512);
	surface_ht_init(&masked_sprite_cache, 1024);

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
	SDL_SetClipRect(screen.surf, &screen.clip);

	is_fullscreen = fullscreen;

	return 0;
}

void
sdl_get_resolution(int *width, int *height)
{
	SDL_GetWindowSize(window, width, height);
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
create_surface_from_data(void *data, int width, int height, int transparent) {
	int r;

	/* Create sprite surface */
	SDL_Surface *surf8 =
	SDL_CreateRGBSurfaceFrom(data, (int)width, (int)height, 8,
				 (int)(width*sizeof(uint8_t)), 0, 0, 0, 0);
	if (surf8 == NULL) {
		LOGE("sdl-video", "Unable to create sprite surface: %s.",
		     SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/* Set sprite palette */
	r = SDL_SetPaletteColors(surf8->format->palette, pal_colors, 0, 256);
	if (r < 0) {
		LOGE("sdl-video", "Unable to set palette for sprite.");
		exit(EXIT_FAILURE);
	}

	/* Covert to screen format */
	SDL_Surface *surf = NULL;

	if (transparent) {
		/* Set color key */
		r = SDL_SetColorKey(surf8, SDL_TRUE, 0);
		if (r < 0) {
			LOGE("sdl-video", "Unable to set color key for sprite.");
			exit(EXIT_FAILURE);
		}
	}

	surf = SDL_ConvertSurface(surf8, ((SDL_Surface*)screen.surf)->format, 0);
	if (surf == NULL) {
		LOGE("sdl-video", "Unable to convert sprite surface: %s.",
		     SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_FreeSurface(surf8);

	return surf;
}

static SDL_Surface *
create_transp_surface(const sprite_t *sprite, int offset)
{
	void *data = (uint8_t *)sprite + sizeof(sprite_t);

	int width = le16toh(sprite->w);
	int height = le16toh(sprite->h);

	/* Unpack */
	size_t unpack_size = width * height;
	uint8_t *unpack = (uint8_t*)calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	data_unpack_transparent_sprite(unpack, data, unpack_size, offset);

	SDL_Surface *surf = create_surface_from_data(unpack, width, height, 1);

	free(unpack);

	return surf;
}

/* Create a masked surface from the given transparent sprite and mask.
   The sprite must be at least as wide as the mask plus mask offset. */
static SDL_Surface *
create_masked_transp_surface(const sprite_t *sprite, const sprite_t *mask, int mask_off)
{
	void *s_data = (uint8_t *)sprite + sizeof(sprite_t);

	size_t s_width = le16toh(sprite->w);
	size_t s_height = le16toh(sprite->h);

	/* Unpack */
	size_t unpack_size = s_width * s_height;
	uint8_t *unpack = (uint8_t*)calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	data_unpack_transparent_sprite(unpack, s_data, unpack_size, 0);

	size_t m_width = le16toh(mask->w);
	size_t m_height = le16toh(mask->h);

	uint8_t *s_copy = (uint8_t*)calloc(m_width * m_height, sizeof(uint8_t));
	if (s_copy == NULL) abort();

	size_t to_copy = m_width * min(m_height, s_height);
	uint8_t *copy_dest = s_copy;
	uint8_t *copy_src = unpack + mask_off;
	while (to_copy) {
		memcpy(copy_dest, copy_src, m_width * sizeof(uint8_t));
		to_copy -= m_width;
		copy_dest += m_width;
		copy_src += s_width;
	}

	free(unpack);

	/* Mask */
	void *m_data = (uint8_t *)mask + sizeof(sprite_t);

	/* Unpack mask */
	size_t m_unpack_size = m_width * m_height;
	uint8_t *m_unpack = (uint8_t*)calloc(m_unpack_size, sizeof(uint8_t));
	if (m_unpack == NULL) abort();

	data_unpack_mask_sprite(m_unpack, m_data, m_unpack_size);

	/* Fill alpha value from mask data */
	for (size_t y = 0; y < m_height; y++) {
		for (size_t x = 0; x < m_width; x++) {
			if (!m_unpack[y*m_width+x]) {
				*(s_copy + y * m_width + x) = 0;
			}
		}
	}

	free(m_unpack);

	SDL_Surface *surf = create_surface_from_data(s_copy, (int)m_width, (int)m_height, 1);

	free(s_copy);

	return surf;
}

void
sdl_draw_transp_sprite(const sprite_t *sprite, int x, int y, int use_off, int y_off, int color_off, frame_t *dest)
{
	int r;

	x += dest->clip.x;
	y += dest->clip.y;

	if (use_off) {
		x += le16toh(sprite->x);
		y += le16toh(sprite->y);
	}

	surface_id_t id;
	id.sprite = sprite;
	id.mask = NULL;
	id.offset = color_off;
	surface_t **surface = surface_ht_store(&transp_sprite_cache, &id);
	if (*surface == NULL) {
		*surface = (surface_t*)malloc(sizeof(surface_t));
		if (*surface == NULL) abort();

		(*surface)->surf = create_transp_surface(sprite, color_off);
	}

	SDL_Surface *surf = (*surface)->surf;

	SDL_Rect src_rect = { 0, y_off, surf->w, surf->h - y_off };
	SDL_Rect dest_rect = { x, y + y_off, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, &src_rect, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s.", SDL_GetError());
	}

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y + y_off, surf->w, surf->h - y_off, 72, dest);
#endif
}

void
sdl_draw_waves_sprite(const sprite_t *sprite, const sprite_t *mask,
		      int x, int y, int mask_off, frame_t *dest)
{
	x += le16toh(sprite->x) + dest->clip.x;
	y += le16toh(sprite->y) + dest->clip.y;

	surface_id_t id;
	id.sprite = sprite;
	id.mask = mask;
	id.offset = 0;
	surface_t **surface = surface_ht_store(&transp_sprite_cache, &id);
	if (*surface == NULL) {
		*surface = (surface_t*)malloc(sizeof(surface_t));
		if (*surface == NULL) abort();

		if (mask != NULL) {
			(*surface)->surf = create_masked_transp_surface(sprite, mask, mask_off);
		} else {
			(*surface)->surf = create_transp_surface(sprite, 0);
		}
	}

	SDL_Surface *surf = (*surface)->surf;
	SDL_Rect dest_rect = { x, y, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	int r = SDL_BlitSurface(surf, NULL, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s.", SDL_GetError());
	}

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y, surf->w, surf->h, 41, dest);
#endif
}

static SDL_Surface *
create_sprite_surface(const sprite_t *sprite)
{
	void *data = (uint8_t *)sprite + sizeof(sprite_t);

	int width = le16toh(sprite->w);
	int height = le16toh(sprite->h);
	
	return create_surface_from_data(data, width, height, 0);
}

void
sdl_draw_sprite(const sprite_t *sprite, int x, int y, frame_t *dest)
{
	int r;

	x += le16toh(sprite->x) + dest->clip.x;
	y += le16toh(sprite->y) + dest->clip.y;

	SDL_Surface *surf = create_sprite_surface(sprite); /* Not cached */

	SDL_Rect dest_rect = { x, y, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, NULL, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s.", SDL_GetError());
	}

	/* Clean up */
	SDL_FreeSurface(surf);

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y, surf->w, surf->h, 68, dest);
#endif
}

static SDL_Surface *
create_overlay_surface(const sprite_t *sprite)
{
	int r;

	void *data = (uint8_t *)sprite + sizeof(sprite_t);

	size_t width = le16toh(sprite->w);
	size_t height = le16toh(sprite->h);

	/* Unpack */
	size_t unpack_size = width * height;
	uint8_t *unpack = (uint8_t*)calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	data_unpack_overlay_sprite(unpack, data, unpack_size);

	/* Create sprite surface */
	SDL_Surface *surf = sdl_create_surface((int)width, (int)height);
	r = SDL_LockSurface(surf);
	if (r < 0) {
		LOGE("sdl-video", "Unable to lock sprite.");
		exit(EXIT_FAILURE);
	}

	/* Fill alpha value from overlay data */
	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			uint32_t *p = (uint32_t *)((uint8_t *)surf->pixels + y * surf->pitch);
			p[x] = SDL_MapRGBA(surf->format, 0, 0, 0, unpack[y*width+x]);
		}
	}

	SDL_UnlockSurface(surf);
	free(unpack);

	return surf;
}

void
sdl_draw_overlay_sprite(const sprite_t *sprite, int x, int y, int y_off, frame_t *dest)
{
	int r;

	x += le16toh(sprite->x) + dest->clip.x;
	y += le16toh(sprite->y) + dest->clip.y;

	surface_id_t id;
	id.sprite = sprite;
	id.mask = NULL;
	id.offset = 0;
	surface_t **surface = surface_ht_store(&overlay_sprite_cache, &id);
	if (*surface == NULL) {
		*surface = (surface_t*)malloc(sizeof(surface_t));
		if (*surface == NULL) abort();

		(*surface)->surf = create_overlay_surface(sprite);
	}

	SDL_Surface *surf = (*surface)->surf;
	SDL_Rect src_rect = { 0, y_off, surf->w, surf->h - y_off };
	SDL_Rect dest_rect = { x, y + y_off, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, &src_rect, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s.", SDL_GetError());
	}

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y + y_off, surf->w, surf->h - y_off, 1, dest);
#endif
}

static SDL_Surface *
create_masked_surface(const sprite_t *sprite, const sprite_t *mask)
{
	size_t m_width = le16toh(mask->w);
	size_t m_height = le16toh(mask->h);

	size_t s_width = le16toh(sprite->w);
	size_t s_height = le16toh(sprite->h);

	void *s_data = (uint8_t *)sprite + sizeof(sprite_t);

	uint8_t *s_copy = (uint8_t*)malloc(m_width * m_height * sizeof(uint8_t));
	if (s_copy == NULL) abort();

	size_t to_copy = m_width * m_height;
	uint8_t *copy_dest = s_copy;
	while (to_copy) {
		size_t s = min(to_copy, s_width * s_height);
		memcpy(copy_dest, s_data, s * sizeof(uint8_t));
		to_copy -= s;
		copy_dest += s;
	}

	/* Mask */
	void *m_data = (uint8_t *)mask + sizeof(sprite_t);

	/* Unpack mask */
	size_t unpack_size = m_width * m_height;
	uint8_t *m_unpack = (uint8_t*)calloc(unpack_size, sizeof(uint8_t));
	if (m_unpack == NULL) abort();

	data_unpack_mask_sprite(m_unpack, m_data, unpack_size);

	/* Fill alpha value from mask data */
	for (size_t y = 0; y < m_height; y++) {
		for (size_t x = 0; x < m_width; x++) {
			if (!m_unpack[y*m_width+x]) {
				*(s_copy + y * m_width + x) = 0;
			}
		}
	}

	free(m_unpack);

	SDL_Surface *surf = create_surface_from_data(s_copy, (int)m_width, (int)m_height, 1);

	free(s_copy);

	return surf;
}

void
sdl_draw_masked_sprite(const sprite_t *sprite, int x, int y, const sprite_t *mask, frame_t *dest)
{
	int r;

	x += le16toh(mask->x) + dest->clip.x;
	y += le16toh(mask->y) + dest->clip.y;

	surface_id_t id;
	id.sprite = sprite;
	id.mask = mask;
	id.offset = 0;
	surface_t **surface = surface_ht_store(&masked_sprite_cache, &id);
	if (*surface == NULL) {
		*surface = (surface_t*)malloc(sizeof(surface_t));
		if (*surface == NULL) abort();

		(*surface)->surf = create_masked_surface(sprite, mask);
	}

	SDL_Surface *surf = (*surface)->surf;
	SDL_Rect src_rect = { 0, 0, surf->w, surf->h };
	SDL_Rect dest_rect = { x, y, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit to dest */
	r = SDL_BlitSurface(surf, &src_rect, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s", SDL_GetError());
	}
}

void
sdl_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h)
{
	int x = dx + dest->clip.x;
	int y = dy + dest->clip.y;

	SDL_Rect dest_rect = { x, y, 0, 0 };
	SDL_Rect src_rect = { sx, sy, w, h };

	SDL_SetClipRect(dest->surf, &dest->clip);

	int r = SDL_BlitSurface((SDL_Surface*)src->surf, &src_rect, (SDL_Surface*)dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s", SDL_GetError());
	}
}

void
sdl_draw_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	SDL_SetClipRect(dest->surf, &dest->clip);

	x += dest->clip.x;
	y += dest->clip.y;

	/* Draw rectangle. */
	sdl_fill_rect(x, y, width, 1, color, dest);
	sdl_fill_rect(x, y+height-1, width, 1, color, dest);
	sdl_fill_rect(x, y, 1, height, color, dest);
	sdl_fill_rect(x+width-1, y, 1, height, color, dest);
}

void
sdl_fill_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	SDL_Rect rect = {
		x + dest->clip.x,
		y + dest->clip.y,
		width, height
	};

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Fill rectangle */
	int r = SDL_FillRect((SDL_Surface*)dest->surf, &rect, SDL_MapRGBA(((SDL_Surface*)dest->surf)->format,
			pal_colors[color].r, pal_colors[color].g, pal_colors[color].b, 0xff));
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

	SDL_Surface *surface = create_transp_surface(sprite, 0);
	cursor = SDL_CreateColorCursor(surface, 8, 8);
	SDL_SetCursor(cursor);
}
