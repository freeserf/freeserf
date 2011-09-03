/* sdl-video.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include "SDL.h"

#include "freeserf_endian.h"
#include "list.h"
#include "sdl-video.h"
#include "gfx.h"
#include "misc.h"
#include "version.h"


#define RSHIFT   0
#define GSHIFT   8
#define BSHIFT  16
#define ASHIFT  24

#define RMASK  (0xff << RSHIFT)
#define GMASK  (0xff << GSHIFT)
#define BMASK  (0xff << BSHIFT)
#define AMASK  (0xff << ASHIFT)

#define MAX_DIRTY_RECTS  128

static frame_t screen;
static SDL_Color pal_colors[256];

static SDL_Rect dirty_rects[MAX_DIRTY_RECTS];
static int dirty_rect_counter = 0;


struct surface {
	SDL_Surface *surf;
};

typedef struct {
	list_elm_t elm;
	const sprite_t *sprite;
	const sprite_t *mask;
	int color_off;
	surface_t *surface;
} cached_surface_t;

static list_t transp_sprite_cache = LIST_INIT(transp_sprite_cache);
static list_t overlay_sprite_cache = LIST_INIT(overlay_sprite_cache);
static list_t masked_sprite_cache = LIST_INIT(masked_sprite_cache);


int
sdl_init()
{
	/* Initialize defaults and Video subsystem */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { 
		fprintf(stderr, "Unable to initialize SDL: %s.\n",
			SDL_GetError());
		return -1;
	}

	/* Display program name and version in caption */
	char caption[64];
	snprintf(caption, 64, "freeserf %s", VCS_VERSION);
	SDL_WM_SetCaption(caption, caption);

	/* Exit on signals */
	signal(SIGINT, exit);
	signal(SIGTERM, exit);

	/* Don't show cursor */
	SDL_ShowCursor(SDL_DISABLE);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	return 0;
}

void
sdl_deinit()
{
	SDL_Quit();
}

int
sdl_set_resolution(int width, int height, int fullscreen)
{
	int flags = SDL_SWSURFACE | SDL_DOUBLEBUF;
	if (fullscreen) flags |= SDL_FULLSCREEN;

	screen.surf = SDL_SetVideoMode(width, height, 32, flags);
	if (screen.surf == NULL) {
		fprintf(stderr, "Unable to set video mode: %s.\n", SDL_GetError());
		return -1;
	}

	SDL_GetClipRect(screen.surf, &screen.clip);

	return 0;
}

static SDL_Surface *
sdl_create_surface(int width, int height)
{
	SDL_Surface *surf = SDL_CreateRGBSurface(SDL_SRCALPHA,
						 width, height, 32, RMASK, GMASK, BMASK, AMASK);
	if (surf == NULL) {
		fprintf(stderr, "Unable to create SDL surface: %s.\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	return surf;
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

static surface_t *
get_cached_surface(list_t *cache, const sprite_t *sprite, const sprite_t *mask, int color_off)
{
	surface_t *cached_surface = NULL;

	list_elm_t *elm;
	list_foreach(cache, elm) {
		cached_surface_t *cs = (cached_surface_t *)elm;
		if (cs->sprite == sprite && cs->mask == mask && cs->color_off == color_off) {
			cached_surface = cs->surface;
			list_elm_remove((list_elm_t *)cs);
			list_prepend(cache, (list_elm_t *)cs);
			break;
		}
	}

	return cached_surface;
}

static void
add_cached_surface(list_t *cache, const sprite_t *sprite, const sprite_t *mask, int color_off, surface_t *surface)
{
	cached_surface_t *cs = malloc(sizeof(cached_surface_t));
	if (cs == NULL) abort();

	cs->sprite = sprite;
	cs->mask = mask;
	cs->surface = surface;
	cs->color_off = color_off;

	list_prepend(cache, (list_elm_t *)cs);
}


static SDL_Surface *
create_transp_surface(const sprite_t *sprite, int offset)
{
	int r;

	void *data = (uint8_t *)sprite + sizeof(sprite_t);

	size_t width = le16toh(sprite->w);
	size_t height = le16toh(sprite->h);

	/* Unpack */
	size_t unpack_size = width * height;
	uint8_t *unpack = calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	gfx_unpack_transparent_sprite(unpack, data, unpack_size, offset);
	
	/* Create sprite surface */
	SDL_Surface *surf8 =
		SDL_CreateRGBSurfaceFrom(unpack, width, height, 8,
					 width*sizeof(uint8_t), 0, 0, 0, 0);
	if (surf8 == NULL) {
		fprintf(stderr, "Unable to create sprite surface: %s.\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/* Set sprite palette */
	r = SDL_SetPalette(surf8, SDL_LOGPAL | SDL_PHYSPAL, pal_colors, 0, 256);
	if (r == 0) {
		fprintf(stderr, "Unable to set palette for sprite.\n");
		exit(EXIT_FAILURE);
	}

	/* Set color key */
	r = SDL_SetColorKey(surf8, SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
	if (r < 0) {
		fprintf(stderr, "Unable to set color key for sprite.\n");
		exit(EXIT_FAILURE);
	}

	/* Covert to screen format */
	SDL_Surface *surf = SDL_DisplayFormatAlpha(surf8);
	if (surf == NULL) {
		fprintf(stderr, "Unable to convert sprite surface: %s.\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_FreeSurface(surf8);

	return surf;
}

void
sdl_draw_transp_sprite(const sprite_t *sprite, int x, int y, int use_off, int y_off, int color_off, frame_t *dest)
{
	int r;

	if (use_off) {
		x += le16toh(sprite->x);
		y += le16toh(sprite->y);
	}

	surface_t *surface = get_cached_surface(&transp_sprite_cache, sprite, NULL, color_off);
	if (surface == NULL) {
		surface = malloc(sizeof(surface_t));
		if (surface == NULL) abort();

		surface->surf = create_transp_surface(sprite, color_off);
		add_cached_surface(&transp_sprite_cache, sprite, NULL, color_off, surface);
	}

	SDL_Surface *surf = surface->surf;

	SDL_Rect src_rect = { 0, y_off, surf->w, surf->h - y_off };
	SDL_Rect dest_rect = { x, y + y_off, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, &src_rect, dest->surf, &dest_rect);
	if (r < 0) {
		fprintf(stderr, "BlitSurface error: %s\n", SDL_GetError());
	}

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y + y_off, surf->w, surf->h - y_off, 72, dest);
#endif
}

void
sdl_draw_waves_sprite(const sprite_t *sprite, int x, int y, frame_t *dest)
{
	int r;
	
	x += le16toh(sprite->x);
	y += le16toh(sprite->y);

	surface_t *surface = get_cached_surface(&transp_sprite_cache, sprite, NULL, 0);
	if (surface == NULL) {
		surface = malloc(sizeof(surface_t));
		if (surface == NULL) abort();

		surface->surf = create_transp_surface(sprite, 0);
		add_cached_surface(&transp_sprite_cache, sprite, NULL, 0, surface);
	}

	SDL_Surface *surf = surface->surf;
	SDL_Rect dest_rect = { x, y, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* TODO mask by the same mask sprite as used to mask the water in the first place */

	/* Blit sprite */
	r = SDL_BlitSurface(surf, NULL, dest->surf, &dest_rect);
	if (r < 0) {
		fprintf(stderr, "BlitSurface error: %s.\n", SDL_GetError());
	}

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y, surf->w, surf->h, 41, dest);
#endif
}

static SDL_Surface *
create_sprite_surface(const sprite_t *sprite)
{
	int r;

	void *data = (uint8_t *)sprite + sizeof(sprite_t);

	size_t width = le16toh(sprite->w);
	size_t height = le16toh(sprite->h);

	/* Create sprite surface from data */
	SDL_Surface *surf8 =
		SDL_CreateRGBSurfaceFrom(data, width, height, 8,
					 width*sizeof(uint8_t), 0, 0, 0, 0);
	if (surf8 == NULL) {
		fprintf(stderr, "Unable to create sprite surface: %s.\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/* Set sprite palette */
	r = SDL_SetPalette(surf8, SDL_LOGPAL | SDL_PHYSPAL, pal_colors, 0, 256);
	if (r == 0) {
		fprintf(stderr, "Unable to set palette for sprite.\n");
		exit(EXIT_FAILURE);
	}

	/* Convert sprite to screen format */
	SDL_Surface *surf = SDL_DisplayFormat(surf8);
	if (surf == NULL) {
		fprintf(stderr, "Unable to convert sprite surface: %s.\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_FreeSurface(surf8);

	return surf;
}

void
sdl_draw_sprite(const sprite_t *sprite, int x, int y, frame_t *dest)
{
	int r;

	x += le16toh(sprite->x);
	y += le16toh(sprite->y);

	SDL_Surface *surf = create_sprite_surface(sprite); /* Not cached */

	SDL_Rect dest_rect = { x, y, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, NULL, dest->surf, &dest_rect);
	if (r < 0) {
		fprintf(stderr, "BlitSurface error: %s.\n", SDL_GetError());
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
	uint8_t *unpack = calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	gfx_unpack_overlay_sprite(unpack, data, unpack_size);

	/* Create sprite surface */
	SDL_Surface *surf = sdl_create_surface(width, height);
	r = SDL_LockSurface(surf);
	if (r < 0) {
		fprintf(stderr, "Unable to lock sprite.\n");
		exit(EXIT_FAILURE);
	}

	/* Fill alpha value from overlay data */
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
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

	x += le16toh(sprite->x);
	y += le16toh(sprite->y);

	surface_t *surface = get_cached_surface(&overlay_sprite_cache, sprite, NULL, 0);
	if (surface == NULL) {
		surface = malloc(sizeof(surface_t));
		if (surface == NULL) abort();

		surface->surf = create_overlay_surface(sprite);
		add_cached_surface(&overlay_sprite_cache, sprite, NULL, 0, surface);
	}

	SDL_Surface *surf = surface->surf;
	SDL_Rect src_rect = { 0, y_off, surf->w, surf->h - y_off };
	SDL_Rect dest_rect = { x, y + y_off, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, &src_rect, dest->surf, &dest_rect);
	if (r < 0) {
		fprintf(stderr, "BlitSurface error: %s.\n", SDL_GetError());
	}

#if 0
	/* Bounding box */
	sdl_draw_rect(x, y + y_off, surf->w, surf->h - y_off, 1, dest);
#endif
}

static SDL_Surface *
create_masked_surface(const sprite_t *sprite, const sprite_t *mask)
{
	int r;

	size_t m_width = le16toh(mask->w);
	size_t m_height = le16toh(mask->h);

	size_t s_width = le16toh(sprite->w);
	size_t s_height = le16toh(sprite->h);

	void *s_data = (uint8_t *)sprite + sizeof(sprite_t);

	uint8_t *s_copy = malloc(m_width * m_height * sizeof(uint8_t));
	if (s_copy == NULL) abort();

	size_t to_copy = m_width * m_height;
	uint8_t *copy_dest = s_copy;
	while (to_copy) {
		size_t s = min(to_copy, s_width * s_height);
		memcpy(copy_dest, s_data, s * sizeof(uint8_t));
		to_copy -= s;
		copy_dest += s;
	}

	/* Create sprite surface */
	SDL_Surface *surf8 =
		SDL_CreateRGBSurfaceFrom(s_copy, m_width, m_height, 8,
				m_width*sizeof(uint8_t), 0, 0, 0, 0);
	if (surf8 == NULL) {
		fprintf(stderr, "Unable to create sprite surface: %s.\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/* Set sprite palette */
	r = SDL_SetPalette(surf8, SDL_LOGPAL | SDL_PHYSPAL, pal_colors, 0, 256);
	if (r == 0) {
		fprintf(stderr, "Unable to set palette for sprite.\n");
		exit(EXIT_FAILURE);
	}

	/* Set color key */
	r = SDL_SetColorKey(surf8, SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
	if (r < 0) {
		fprintf(stderr, "Unable to set color key for sprite.\n");
		exit(EXIT_FAILURE);
	}

	/* Convert sprite to screen format */
	SDL_Surface *surf = SDL_DisplayFormatAlpha(surf8);
	if (surf == NULL) {
		fprintf(stderr, "Unable to convert sprite surface: %s.\n",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_FreeSurface(surf8);

	/* Mask */
	void *m_data = (uint8_t *)mask + sizeof(sprite_t);

	/* Unpack mask */
	size_t unpack_size = m_width * m_height;
	uint8_t *m_unpack = calloc(unpack_size, sizeof(uint8_t));
	if (m_unpack == NULL) abort();

	gfx_unpack_mask_sprite(m_unpack, m_data, unpack_size);

	r = SDL_LockSurface(surf);
	if (r < 0) {
		fprintf(stderr, "Unable to lock sprite.\n");
		exit(EXIT_FAILURE);
	}

	/* Fill alpha value from mask data */
	for (int y = 0; y < m_height; y++) {
		for (int x = 0; x < m_width; x++) {
			uint32_t *p = (uint32_t *)((uint8_t *)surf->pixels + y * surf->pitch);
			p[x] = (~AMASK & p[x]) | (m_unpack[y*m_width+x] << ASHIFT);
		}
	}

	SDL_UnlockSurface(surf);
	free(m_unpack);

	return surf;
}

surface_t *
sdl_draw_masked_sprite(const sprite_t *sprite, int x, int y, const sprite_t *mask, surface_t *surface, frame_t *dest)
{
	int r;

	x += le16toh(mask->x);
	y += le16toh(mask->y);

	if (surface == NULL) {
		surface = get_cached_surface(&masked_sprite_cache, sprite, mask, 0);
		if (surface == NULL) {
			surface = malloc(sizeof(surface_t));
			if (surface == NULL) abort();

			surface->surf = create_masked_surface(sprite, mask);
			add_cached_surface(&masked_sprite_cache, sprite, mask, 0, surface);
		}
	}

	SDL_Surface *surf = surface->surf;

	SDL_Rect src_rect = { 0, 0, surf->w, surf->h };
	SDL_Rect dest_rect = { x, y, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit to dest */
	r = SDL_BlitSurface(surf, &src_rect, dest->surf, &dest_rect);
	if (r < 0) {
		fprintf(stderr, "BlitSurface error: %s\n", SDL_GetError());
	}

	return surface;
}

void
sdl_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h)
{
	int r;

	SDL_Rect dest_rect = { dx, dy, 0, 0 };
	SDL_Rect src_rect = { sx, sy, w, h };

	SDL_SetClipRect(dest->surf, &dest->clip);

	r = SDL_BlitSurface(src->surf, &src_rect, dest->surf, &dest_rect);
	if (r < 0) {
		fprintf(stderr, "BlitSurface error: %s\n", SDL_GetError());
	}
}

void
sdl_draw_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Draw rectangle. */
	sdl_fill_rect(x, y, width, 1, color, dest);
	sdl_fill_rect(x, y+height-1, width, 1, color, dest);
	sdl_fill_rect(x, y, 1, height, color, dest);
	sdl_fill_rect(x+width-1, y, 1, height, color, dest);
}

void
sdl_fill_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	SDL_Rect rect = { x, y, width, height };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Fill rectangle */
	int r = SDL_FillRect(dest->surf, &rect, SDL_MapRGBA(dest->surf->format,
			pal_colors[color].r, pal_colors[color].g, pal_colors[color].b, 0xff));
	if (r < 0) {
		fprintf(stderr, "FillRect error: %s.\n", SDL_GetError());
	}
}

void
sdl_mark_dirty(int x, int y, int width, int height)
{
	if (dirty_rect_counter < MAX_DIRTY_RECTS) {
		dirty_rects[dirty_rect_counter].x = x;
		dirty_rects[dirty_rect_counter].y = y;
		dirty_rects[dirty_rect_counter].w = width;
		dirty_rects[dirty_rect_counter].h = height;
	}
	dirty_rect_counter += 1;
}

void
sdl_swap_buffers()
{
	if (dirty_rect_counter > MAX_DIRTY_RECTS) {
		SDL_UpdateRect(screen.surf, 0, 0, 0, 0);
	} else if (dirty_rect_counter > 0) {
		SDL_UpdateRects(screen.surf, dirty_rect_counter, dirty_rects);
	}
	dirty_rect_counter = 0;
}

void
sdl_set_palette(const uint8_t *palette)
{
	/* Entry 0 is always black */
	pal_colors[0].r = 0;
	pal_colors[0].g = 0;
	pal_colors[0].b = 0;

	/* Fill palette */
	for (int i = 0; i < 256; i++) {
		pal_colors[i].r = palette[i*3];
		pal_colors[i].g = palette[i*3+1];
		pal_colors[i].b = palette[i*3+2];
	}
}

