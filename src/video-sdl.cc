/*
 * video-sdl.cc - SDL graphics rendering
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

#include "src/video-sdl.h"

#include <cstdio>
#include <cstdlib>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <cstring>
#include <csignal>
#include <cassert>

#include <SDL.h>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/log.h"
  #include "src/data.h"
END_EXT_C
#include "src/gfx.h"
#include "src/version.h"

int video_sdl_t::bpp = 32;
Uint32 video_sdl_t::Rmask = 0xFF000000;
Uint32 video_sdl_t::Gmask = 0x00FF0000;
Uint32 video_sdl_t::Bmask = 0x0000FF00;
Uint32 video_sdl_t::Amask = 0x000000FF;
Uint32 video_sdl_t::pixel_format = SDL_PIXELFORMAT_RGBA8888;

video_sdl_t::video_sdl_t() {
  screen = NULL;

  /* Initialize defaults and Video subsystem */
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    LOGE("sdl-video", "Unable to initialize SDL: %s.", SDL_GetError());
    assert(false);
  }

  /* Display program name and version in caption */
  char caption[64];
  snprintf(caption, sizeof(caption), "freeserf %s", FREESERF_VERSION);

  /* Create window and renderer */
  window = SDL_CreateWindow(caption,
          SDL_WINDOWPOS_UNDEFINED,
          SDL_WINDOWPOS_UNDEFINED,
          800, 600, SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    LOGE("sdl-video", "Unable to create SDL window: %s.", SDL_GetError());
    assert(false);
  }

  /* Create renderer for window */
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (renderer == NULL) {
    LOGE("sdl-video", "Unable to create SDL renderer: %s.", SDL_GetError());
    assert(false);
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
  SDL_PixelFormatEnumToMasks(pixel_format, &bpp,
                             &Rmask, &Gmask, &Bmask, &Amask);

  /* Set scaling mode */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

  /* Exit on signals */
  signal(SIGINT, exit);
  signal(SIGTERM, exit);
}

video_sdl_t::~video_sdl_t() {
  set_cursor(NULL);
  SDL_Quit();
}

SDL_Surface *
video_sdl_t::create_surface(int width, int height) {
  SDL_Surface *surf = SDL_CreateRGBSurface(0, width, height, bpp,
             Rmask, Gmask, Bmask, Amask);
  if (surf == NULL) {
    LOGE("sdl-video", "Unable to create SDL surface: %s.", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  return surf;
}

bool
video_sdl_t::set_resolution(int width, int height, int fullscreen) {
  /* Set fullscreen mode */
  int r = SDL_SetWindowFullscreen(window,
                                  fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP :
                                               0);
  if (r < 0) {
    LOGE("sdl-video", "Unable to set window fullscreen: %s.", SDL_GetError());
    return false;
  }

  if (screen == NULL) {
    screen = new frame_t(this, 100, 100);
  }

  /* Allocate new screen surface and texture */
  if (screen->surf != NULL) {
    SDL_FreeSurface(screen->surf);
  }
  screen->surf = create_surface(width, height);

  if (screen_texture != NULL) SDL_DestroyTexture(screen_texture);
  screen_texture = SDL_CreateTexture(renderer, pixel_format,
             SDL_TEXTUREACCESS_STREAMING,
             width, height);
  if (screen_texture == NULL) {
    LOGE("sdl-video", "Unable to create SDL texture: %s.", SDL_GetError());
    return false;
  }

  /* Set logical size of screen */
  r = SDL_RenderSetLogicalSize(renderer, width, height);
  if (r < 0) {
    LOGE("sdl-video", "Unable to set logical size: %s.", SDL_GetError());
    return false;
  }

  this->fullscreen = fullscreen;

  return true;
}

void
video_sdl_t::get_resolution(int *width, int *height) {
  SDL_GL_GetDrawableSize(window, width, height);
}

bool
video_sdl_t::set_fullscreen(int enable) {
  int width = 0;
  int height = 0;
  SDL_GL_GetDrawableSize(window, &width, &height);
  return set_resolution(width, height, enable);
}

bool
video_sdl_t::is_fullscreen() {
  return fullscreen;
}

frame_t *
video_sdl_t::get_screen_frame() {
  return screen;
}

void
video_sdl_t::frame_init(frame_t *frame, int width, int height) {
  frame->surf = create_surface(width, height);
}

void
video_sdl_t::frame_deinit(frame_t *frame) {
  SDL_FreeSurface(frame->surf);
}

void
video_sdl_t::warp_mouse(int x, int y) {
  SDL_WarpMouseInWindow(NULL, x, y);
}


SDL_Surface *
video_sdl_t::create_surface_from_data(void *data, int width, int height) {
  /* Create sprite surface */
  SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(data, width, height, 32,
                                               4 * width,
                                               0xff, 0xff00, 0xff0000,
                                               0xff000000);
  if (surf == NULL) {
    LOGE("sdl-video", "Unable to create sprite surface: %s.", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  /* Covert to screen format */
  SDL_Surface *surf_screen = SDL_ConvertSurface(surf, screen->surf->format, 0);
  if (surf_screen == NULL) {
    LOGE("sdl-video", "Unable to convert sprite surface: %s.", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  SDL_FreeSurface(surf);

  return surf_screen;
}

SDL_Surface *
video_sdl_t::create_surface_from_sprite(const sprite_t *sprite) {
  const void *data = reinterpret_cast<const uint8_t*>(sprite) +
                     sizeof(sprite_t);

  uint width = sprite->width;
  uint height = sprite->height;

  return create_surface_from_data(const_cast<void*>(data), width, height);
}

void
video_sdl_t::draw_sprite(const sprite_t *sprite, int x, int y, int y_offset,
                         frame_t *dest) {
  SDL_Surface *surf = create_surface_from_sprite(sprite);

  SDL_Rect src_rect = { 0, y_offset, surf->w, surf->h - y_offset };
  SDL_Rect dest_rect = { x, y + y_offset, 0, 0 };

  /* Blit sprite */
  int r = SDL_BlitSurface(surf, &src_rect, dest->surf, &dest_rect);
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
video_sdl_t::draw_frame(int dx, int dy, frame_t *dest, int sx, int sy,
                        frame_t *src, int w, int h) {
  int x = dx;
  int y = dy;

  SDL_Rect dest_rect = { x, y, 0, 0 };
  SDL_Rect src_rect = { sx, sy, w, h };

  int r = SDL_BlitSurface(src->surf, &src_rect, dest->surf, &dest_rect);
  if (r < 0) {
    LOGE("sdl-video", "BlitSurface error: %s", SDL_GetError());
  }
}

void
video_sdl_t::draw_rect(int x, int y, int width, int height,
                       const color_t *color, frame_t *dest) {
  /* Draw rectangle. */
  fill_rect(x, y, width, 1, color, dest);
  fill_rect(x, y+height-1, width, 1, color, dest);
  fill_rect(x, y, 1, height, color, dest);
  fill_rect(x+width-1, y, 1, height, color, dest);
}

void
video_sdl_t::fill_rect(int x, int y, int width, int height,
                       const color_t *color, frame_t *dest) {
  SDL_Rect rect = { x, y, width, height };

  /* Fill rectangle */
  int r = SDL_FillRect(dest->surf, &rect, SDL_MapRGBA(dest->surf->format,
                                                      color->r, color->g,
                                                      color->b, 0xff));
  if (r < 0) {
    LOGE("sdl-video", "FillRect error: %s.", SDL_GetError());
  }
}

void
video_sdl_t::swap_buffers() {
  SDL_UpdateTexture(screen_texture, NULL, screen->surf->pixels,
                    screen->surf->pitch);

  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

void
video_sdl_t::set_palette(const uint8_t *palette) {
  /* Fill palette */
  for (int i = 0; i < 256; i++) {
    pal_colors[i].r = palette[i*3];
    pal_colors[i].g = palette[i*3+1];
    pal_colors[i].b = palette[i*3+2];
    pal_colors[i].a = SDL_ALPHA_OPAQUE;
  }
}

void
video_sdl_t::set_cursor(const sprite_t *sprite) {
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
