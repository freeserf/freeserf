/* sdl-video.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include "SDL.h"

#include "freeserf_endian.h"
#include "sdl-video.h"
#include "gfx.h"
#include "misc.h"
#include "version.h"
#include "log.h"


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
	ht->entries = calloc(size, sizeof(surface_ht_entry_t *));
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

	surface_ht_entry_t *new_entry = calloc(1, sizeof(surface_ht_entry_t));
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
	SDL_WM_SetCaption(caption, caption);

	/* Exit on signals */
	signal(SIGINT, exit);
	signal(SIGTERM, exit);

	/* Don't show cursor */
	SDL_ShowCursor(SDL_DISABLE);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	/* Init sprite cache */
	surface_ht_init(&transp_sprite_cache, 4096);
	surface_ht_init(&overlay_sprite_cache, 512);
	surface_ht_init(&masked_sprite_cache, 1024);

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
		LOGE("sdl-video", "Unable to set video mode: %s.", SDL_GetError());
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
		LOGE("sdl-video", "Unable to create SDL surface: %s.", SDL_GetError());
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
	r = SDL_SetPalette(surf8, SDL_LOGPAL | SDL_PHYSPAL, pal_colors, 0, 256);
	if (r == 0) {
		LOGE("sdl-video", "Unable to set palette for sprite.");
		exit(EXIT_FAILURE);
	}

	/* Covert to screen format */
	SDL_Surface *surf = NULL;

	if (transparent) {
		/* Set color key */
		r = SDL_SetColorKey(surf8, SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
		if (r < 0) {
			LOGE("sdl-video", "Unable to set color key for sprite.");
			exit(EXIT_FAILURE);
		}

		surf = SDL_DisplayFormatAlpha(surf8);
	} else {
		surf = SDL_DisplayFormat(surf8);
	}

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
	uint8_t *unpack = calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	gfx_unpack_transparent_sprite(unpack, data, unpack_size, offset);

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
	uint8_t *unpack = calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	gfx_unpack_transparent_sprite(unpack, s_data, unpack_size, 0);

	size_t m_width = le16toh(mask->w);
	size_t m_height = le16toh(mask->h);

	uint8_t *s_copy = calloc(m_width * m_height, sizeof(uint8_t));
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
	uint8_t *m_unpack = calloc(m_unpack_size, sizeof(uint8_t));
	if (m_unpack == NULL) abort();

	gfx_unpack_mask_sprite(m_unpack, m_data, m_unpack_size);

	/* Fill alpha value from mask data */
	for (int y = 0; y < m_height; y++) {
		for (int x = 0; x < m_width; x++) {
			if (!m_unpack[y*m_width+x]) {
				*(s_copy + y * m_width + x) = 0;
			}
		}
	}

	free(m_unpack);

	SDL_Surface *surf = create_surface_from_data(s_copy, m_width, m_height, 1);

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

	const surface_id_t id = { .sprite = sprite, .mask = NULL, .offset = color_off };
	surface_t **surface = surface_ht_store(&transp_sprite_cache, &id);
	if (*surface == NULL) {
		*surface = malloc(sizeof(surface_t));
		if (*surface == NULL) abort();

		(*surface)->surf = create_transp_surface(sprite, color_off);
	}

	SDL_Surface *surf = (*surface)->surf;

	SDL_Rect src_rect = { 0, y_off, surf->w, surf->h - y_off };
	SDL_Rect dest_rect = { x, y + y_off, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, &src_rect, dest->surf, &dest_rect);
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

	const surface_id_t id = { .sprite = sprite, .mask = mask, .offset = 0 };
	surface_t **surface = surface_ht_store(&transp_sprite_cache, &id);
	if (*surface == NULL) {
		*surface = malloc(sizeof(surface_t));
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
	int r = SDL_BlitSurface(surf, NULL, dest->surf, &dest_rect);
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
	r = SDL_BlitSurface(surf, NULL, dest->surf, &dest_rect);
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
	uint8_t *unpack = calloc(unpack_size, sizeof(uint8_t));
	if (unpack == NULL) abort();

	gfx_unpack_overlay_sprite(unpack, data, unpack_size);

	/* Create sprite surface */
	SDL_Surface *surf = sdl_create_surface((int)width, (int)height);
	r = SDL_LockSurface(surf);
	if (r < 0) {
		LOGE("sdl-video", "Unable to lock sprite.");
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

	x += le16toh(sprite->x) + dest->clip.x;
	y += le16toh(sprite->y) + dest->clip.y;

	const surface_id_t id = { .sprite = sprite, .mask = NULL, .offset = 0 };
	surface_t **surface = surface_ht_store(&overlay_sprite_cache, &id);
	if (*surface == NULL) {
		*surface = malloc(sizeof(surface_t));
		if (*surface == NULL) abort();

		(*surface)->surf = create_overlay_surface(sprite);
	}

	SDL_Surface *surf = (*surface)->surf;
	SDL_Rect src_rect = { 0, y_off, surf->w, surf->h - y_off };
	SDL_Rect dest_rect = { x, y + y_off, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit sprite */
	r = SDL_BlitSurface(surf, &src_rect, dest->surf, &dest_rect);
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

	/* Mask */
	void *m_data = (uint8_t *)mask + sizeof(sprite_t);

	/* Unpack mask */
	size_t unpack_size = m_width * m_height;
	uint8_t *m_unpack = calloc(unpack_size, sizeof(uint8_t));
	if (m_unpack == NULL) abort();

	gfx_unpack_mask_sprite(m_unpack, m_data, unpack_size);

	/* Fill alpha value from mask data */
	for (int y = 0; y < m_height; y++) {
		for (int x = 0; x < m_width; x++) {
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

surface_t *
sdl_draw_masked_sprite(const sprite_t *sprite, int x, int y, const sprite_t *mask, surface_t *surface, frame_t *dest)
{
	int r;

	x += le16toh(mask->x) + dest->clip.x;
	y += le16toh(mask->y) + dest->clip.y;

	if (surface == NULL) {
		const surface_id_t id = { .sprite = sprite, .mask = mask, .offset = 0 };
		surface_t **s = surface_ht_store(&masked_sprite_cache, &id);
		if (*s == NULL) {
			*s = malloc(sizeof(surface_t));
			if (*s == NULL) abort();

			(*s)->surf = create_masked_surface(sprite, mask);
		}
		surface = *s;
	}

	SDL_Surface *surf = surface->surf;

	SDL_Rect src_rect = { 0, 0, surf->w, surf->h };
	SDL_Rect dest_rect = { x, y, 0, 0 };

	SDL_SetClipRect(dest->surf, &dest->clip);

	/* Blit to dest */
	r = SDL_BlitSurface(surf, &src_rect, dest->surf, &dest_rect);
	if (r < 0) {
		LOGE("sdl-video", "BlitSurface error: %s", SDL_GetError());
	}

	return surface;
}

void
sdl_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h)
{
	int x = dx + dest->clip.x;
	int y = dy + dest->clip.y;

	SDL_Rect dest_rect = { x, y, 0, 0 };
	SDL_Rect src_rect = { sx, sy, w, h };

	SDL_SetClipRect(dest->surf, &dest->clip);

	int r = SDL_BlitSurface(src->surf, &src_rect, dest->surf, &dest_rect);
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
	int r = SDL_FillRect(dest->surf, &rect, SDL_MapRGBA(dest->surf->format,
			pal_colors[color].r, pal_colors[color].g, pal_colors[color].b, 0xff));
	if (r < 0) {
		LOGE("sdl-video", "FillRect error: %s.", SDL_GetError());
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

