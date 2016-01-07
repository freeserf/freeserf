/*
 * video-sdl.cc - SDL graphics rendering
 *
 * Copyright (C) 2013-2015  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <strstream>

#include <SDL.h>

SDL_Exception::SDL_Exception(const std::string &description) throw()
  : Video_Exception(description) {
  sdl_error = SDL_GetError();
}

SDL_Exception::~SDL_Exception() throw() {
}

const char *
SDL_Exception::get_description() const {
  std::strstream str;
  str << Video_Exception::get_description();
  str << "(" << sdl_error << ")";
  return str.str();
}

const char *
SDL_Exception::get_platform() const {
  return "SDL";
}

int video_sdl_t::bpp = 32;
Uint32 video_sdl_t::Rmask = 0x0000FF00;
Uint32 video_sdl_t::Gmask = 0x00FF0000;
Uint32 video_sdl_t::Bmask = 0xFF000000;
Uint32 video_sdl_t::Amask = 0x000000FF;
Uint32 video_sdl_t::pixel_format = SDL_PIXELFORMAT_RGBA8888;

video_sdl_t::video_sdl_t() throw(Video_Exception) {
  screen = NULL;
  screen_texture = NULL;
  cursor = NULL;
  fullscreen = false;

  /* Initialize defaults and Video subsystem */
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    throw SDL_Exception("Unable to initialize SDL video");
  }

  /* Create window and renderer */
  window = SDL_CreateWindow("freeserf",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            800, 600, SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    throw SDL_Exception("Unable to create SDL window");
  }

  /* Create renderer for window */
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED |
                                            SDL_RENDERER_TARGETTEXTURE);
  if (renderer == NULL) {
    throw SDL_Exception("Unable to create SDL renderer");
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
}

video_sdl_t::~video_sdl_t() {
  if (screen != NULL) {
    delete screen;
    screen = NULL;
  }
  set_cursor(NULL, 0, 0);
  SDL_Quit();
}

video_t *
video_t::get_instance() {
  if (instance == NULL) {
    instance = new video_sdl_t();
  }
  return instance;
}

SDL_Surface *
video_sdl_t::create_surface(int width, int height) {
  SDL_Surface *surf = SDL_CreateRGBSurface(0, width, height, bpp,
                                           Rmask, Gmask, Bmask, Amask);
  if (surf == NULL) {
    throw SDL_Exception("Unable to create SDL surface");
  }

  return surf;
}

void
video_sdl_t::set_resolution(unsigned int width, unsigned int height,
                            bool fullscreen) throw(Video_Exception) {
  /* Set fullscreen mode */
  int r = SDL_SetWindowFullscreen(window,
                                  fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP :
                                               0);
  if (r < 0) {
    throw SDL_Exception("Unable to set window fullscreen");
  }

  if (screen == NULL) {
    screen = new video_frame_t();
  }

  /* Allocate new screen surface and texture */
  if (screen->texture != NULL) {
    SDL_DestroyTexture(screen->texture);
  }
  screen->texture = create_texture(width, height);

  if (screen_texture != NULL) {
    SDL_DestroyTexture(screen_texture);
  }
  screen_texture = SDL_CreateTexture(renderer, pixel_format,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     width, height);
  if (screen_texture == NULL) {
    throw SDL_Exception("Unable to create SDL texture");
  }

  /* Set logical size of screen */
  r = SDL_RenderSetLogicalSize(renderer, width, height);
  if (r < 0) {
    throw SDL_Exception("Unable to set logical size");
  }

  this->fullscreen = fullscreen;
}

void
video_sdl_t::get_resolution(unsigned int *width, unsigned int *height) {
  int w = 0;
  int h = 0;
  SDL_GetRendererOutputSize(renderer, &w, &h);
  if (width != NULL) {
    *width = w;
  }
  if (height != NULL) {
    *height = h;
  }
}

void
video_sdl_t::set_fullscreen(bool enable) throw(Video_Exception) {
  int width = 0;
  int height = 0;
  SDL_GetRendererOutputSize(renderer, &width, &height);
  set_resolution(width, height, enable);
}

bool
video_sdl_t::is_fullscreen() {
  return fullscreen;
}

video_frame_t *
video_sdl_t::get_screen_frame() {
  return screen;
}

video_frame_t *
video_sdl_t::create_frame(unsigned int width, unsigned int height) {
  video_frame_t *frame = new video_frame_t;
  frame->texture = create_texture(width, height);
  return frame;
}

void
video_sdl_t::destroy_frame(video_frame_t *frame) {
  SDL_DestroyTexture(frame->texture);
  delete frame;
}

video_image_t *
video_sdl_t::create_image(void *data, unsigned int width, unsigned int height) {
  video_image_t *image = new video_image_t();
  image->w = width;
  image->h = height;
  image->texture = create_texture_from_data(data, width, height);
  return image;
}

void
video_sdl_t::destroy_image(video_image_t *image) {
  SDL_DestroyTexture(image->texture);
  delete image;
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
                                               0x00FF0000, 0x0000FF00,
                                               0x000000FF, 0xFF000000);
  if (surf == NULL) {
    throw SDL_Exception("Unable to create sprite surface");
  }

  /* Covert to screen format */
  SDL_Surface *surf_screen = SDL_ConvertSurfaceFormat(surf, pixel_format, 0);
  if (surf_screen == NULL) {
    throw SDL_Exception("Unable to convert sprite surface");
  }

  SDL_FreeSurface(surf);

  return surf_screen;
}

SDL_Texture *
video_sdl_t::create_texture(int width, int height) {
  SDL_Texture *texture = SDL_CreateTexture(renderer, pixel_format,
                                           SDL_TEXTUREACCESS_TARGET,
                                           width, height);

  if (texture == NULL) {
    throw SDL_Exception("Unable to create SDL texture");
  }

  SDL_SetRenderTarget(renderer, texture);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
  SDL_RenderClear(renderer);

  return texture;
}

SDL_Texture *
video_sdl_t::create_texture_from_data(void *data, int width, int height) {
  SDL_Surface *surf = create_surface_from_data(data, width, height);
  if (surf == NULL) {
    return NULL;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
  if (texture == NULL) {
    throw SDL_Exception("Unable to create SDL texture from data");
  }

  SDL_FreeSurface(surf);

  return texture;
}

void
video_sdl_t::draw_image(const video_image_t *image, int x, int y, int y_offset,
                        video_frame_t *dest) {
  SDL_Rect dest_rect = { x, y + y_offset,
                         static_cast<int>(image->w),
                         static_cast<int>(image->h - y_offset) };
  SDL_Rect src_rect = { 0, y_offset,
                        static_cast<int>(image->w),
                        static_cast<int>(image->h - y_offset) };

  /* Blit sprite */
  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  int r = SDL_RenderCopy(renderer, image->texture, &src_rect, &dest_rect);
  if (r < 0) {
    throw SDL_Exception("RenderCopy error");
  }
}

void
video_sdl_t::draw_frame(int dx, int dy, video_frame_t *dest, int sx, int sy,
                        video_frame_t *src, int w, int h) {
  SDL_Rect dest_rect = { dx, dy, w, h };
  SDL_Rect src_rect = { sx, sy, w, h };

  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  int r = SDL_RenderCopy(renderer, src->texture, &src_rect, &dest_rect);
  if (r < 0) {
    throw SDL_Exception("RenderCopy error");
  }
}

void
video_sdl_t::draw_rect(int x, int y, unsigned int width, unsigned int height,
                       const video_color_t color, video_frame_t *dest) {
  /* Draw rectangle. */
  fill_rect(x, y, width, 1, color, dest);
  fill_rect(x, y+height-1, width, 1, color, dest);
  fill_rect(x, y, 1, height, color, dest);
  fill_rect(x+width-1, y, 1, height, color, dest);
}

void
video_sdl_t::fill_rect(int x, int y, unsigned int width, unsigned int height,
                       const video_color_t color, video_frame_t *dest) {
  SDL_Rect rect = { x, y, static_cast<int>(width), static_cast<int>(height) };

  /* Fill rectangle */
  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0xff);
  int r = SDL_RenderFillRect(renderer, &rect);
  if (r < 0) {
    throw SDL_Exception("RenderFillRect error");
  }
}

void
video_sdl_t::swap_buffers() {
  SDL_SetRenderTarget(renderer, NULL);
  SDL_RenderCopy(renderer, screen->texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

void
video_sdl_t::set_cursor(void *data, unsigned int width, unsigned int height) {
  if (cursor != NULL) {
    SDL_SetCursor(NULL);
    SDL_FreeCursor(cursor);
    cursor = NULL;
  }

  if (data == NULL) return;

  SDL_Surface *surface = create_surface_from_data(data, width, height);
  cursor = SDL_CreateColorCursor(surface, 8, 8);
  SDL_SetCursor(cursor);
}
