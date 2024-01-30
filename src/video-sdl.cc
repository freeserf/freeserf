/*
 * video-sdl.cc - SDL graphics rendering
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <sstream>

#include <SDL3/SDL.h>

ExceptionSDL::ExceptionSDL(const std::string &description) throw()
  : ExceptionVideo(description) {
  sdl_error = SDL_GetError();
  this->description += " (" + sdl_error + ")";
}

int VideoSDL::bpp = 32;
Uint32 VideoSDL::Rmask = 0x0000FF00;
Uint32 VideoSDL::Gmask = 0x00FF0000;
Uint32 VideoSDL::Bmask = 0xFF000000;
Uint32 VideoSDL::Amask = 0x000000FF;
Uint32 VideoSDL::pixel_format = SDL_PIXELFORMAT_RGBA8888;

VideoSDL::VideoSDL() {
  screen = nullptr;
  cursor = nullptr;
  fullscreen = false;
  zoom_factor = 1.f;

  Log::Info["video"] << "Initializing \"sdl\".";
  Log::Info["video"] << "Available drivers:";
  int num_drivers = SDL_GetNumVideoDrivers();
  for (int i = 0; i < num_drivers; ++i) {
    Log::Info["video"] << "\t" << SDL_GetVideoDriver(i);
  }

  /* Initialize defaults and Video subsystem */
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
    throw ExceptionSDL("Unable to initialize SDL video");
  }

  SDL_version version;
  SDL_GetVersion(&version);
  Log::Info["video"] << "Initialized with SDL "
                     << static_cast<int>(version.major) << '.'
                     << static_cast<int>(version.minor) << '.'
                     << static_cast<int>(version.patch)
                     << " (driver: " << SDL_GetCurrentVideoDriver() << ")";

  /* Create window and renderer */
  window = SDL_CreateWindow("freeserf",
                            800, 600,
                            SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    throw ExceptionSDL("Unable to create SDL window");
  }

  /* Create renderer for window */
  renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL) {
    throw ExceptionSDL("Unable to create SDL renderer");
  }

  /* Determine optimal pixel format for current window */
  SDL_RendererInfo render_info = {0, 0, 0, {0}, 0, 0};
  SDL_GetRendererInfo(renderer, &render_info);
  for (Uint32 i = 0; i < render_info.num_texture_formats; i++) {
    Uint32 format = render_info.texture_formats[i];
    int bpp = SDL_BITSPERPIXEL(format);
    if (32 == bpp) {
      pixel_format = format;
      break;
    }
  }
  SDL_GetMasksForPixelFormatEnum(pixel_format, &bpp,
                             &Rmask, &Gmask, &Bmask, &Amask);

  /* Set scaling mode */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  set_resolution(w, h, fullscreen);
}

VideoSDL::~VideoSDL() {
  if (screen != nullptr) {
    delete screen;
    screen = nullptr;
  }
  set_cursor(nullptr, 0, 0);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

Video &
Video::get_instance() {
  static VideoSDL instance;
  return instance;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return SDL_CreateSurface(width, height,
            SDL_GetPixelFormatEnumForMasks(depth, Rmask, Gmask, Bmask, Amask));
}

SDL_Surface *
VideoSDL::create_surface(int width, int height) {
  SDL_Surface *surf = SDL_CreateRGBSurface(0, width, height, bpp,
                                           Rmask, Gmask, Bmask, Amask);
  if (surf == nullptr) {
    throw ExceptionSDL("Unable to create SDL surface");
  }

  return surf;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return SDL_CreateSurfaceFrom(pixels, width, height, pitch,
            SDL_GetPixelFormatEnumForMasks(depth, Rmask, Gmask, Bmask, Amask));
}

void
VideoSDL::set_resolution(unsigned int width, unsigned int height, bool fs) {
  /* Set fullscreen mode */
  int r = SDL_SetWindowFullscreen(window, fs);
  if (r < 0) {
    throw ExceptionSDL("Unable to set window fullscreen");
  }

  if (screen == nullptr) {
    screen = new Video::Frame();
  }

  /* Allocate new screen surface and texture */
  if (screen->texture != nullptr) {
    SDL_DestroyTexture(screen->texture);
  }
  screen->texture = create_texture(width, height);

  /* Set logical size of screen */
  //r = SDL_SetRenderLogicalPresentation(renderer, width, height);
  r = SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED, SDL_SCALEMODE_NEAREST);
  if (r < 0) {
    throw ExceptionSDL("Unable to set logical size");
  }

  fullscreen = fs;
}

void
VideoSDL::get_resolution(unsigned int *width, unsigned int *height) {
  int w = 0;
  int h = 0;
  SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
  if (width != nullptr) {
    *width = w;
  }
  if (height != nullptr) {
    *height = h;
  }
}

void
VideoSDL::set_fullscreen(bool enable) {
  int width = 0;
  int height = 0;
  SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
  set_resolution(width, height, enable);
}

bool
VideoSDL::is_fullscreen() {
  return fullscreen;
}

Video::Frame *
VideoSDL::get_screen_frame() {
  return screen;
}

Video::Frame *
VideoSDL::create_frame(unsigned int width, unsigned int height) {
  Video::Frame *frame = new Video::Frame;
  frame->texture = create_texture(width, height);
  return frame;
}

void
VideoSDL::destroy_frame(Video::Frame *frame) {
  SDL_DestroyTexture(frame->texture);
  delete frame;
}

Video::Image *
VideoSDL::create_image(void *data, unsigned int width, unsigned int height) {
  Video::Image *image = new Video::Image();
  image->w = width;
  image->h = height;
  image->texture = create_texture_from_data(data, width, height);
  return image;
}

void
VideoSDL::destroy_image(Video::Image *image) {
  SDL_DestroyTexture(image->texture);
  delete image;
}

void
VideoSDL::warp_mouse(int x, int y) {
  SDL_WarpMouseInWindow(nullptr, x, y);
}

SDL_Surface *
VideoSDL::create_surface_from_data(void *data, int width, int height) {
  /* Create sprite surface */
  SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(data, width, height, 32,
                                               4 * width,
                                               0x00FF0000, 0x0000FF00,
                                               0x000000FF, 0xFF000000);
  if (surf == nullptr) {
    throw ExceptionSDL("Unable to create sprite surface");
  }

  /* Covert to screen format */
  SDL_Surface *surf_screen = SDL_ConvertSurfaceFormat(surf, pixel_format);
  if (surf_screen == nullptr) {
    throw ExceptionSDL("Unable to convert sprite surface");
  }

  SDL_DestroySurface(surf);

  return surf_screen;
}

SDL_Texture *
VideoSDL::create_texture(int width, int height) {
  SDL_Texture *texture = SDL_CreateTexture(renderer, pixel_format,
                                           SDL_TEXTUREACCESS_TARGET,
                                           width, height);

  if (texture == nullptr) {
    throw ExceptionSDL("Unable to create SDL texture");
  }

  SDL_SetRenderTarget(renderer, texture);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
  SDL_RenderClear(renderer);

  return texture;
}

SDL_Texture *
VideoSDL::create_texture_from_data(void *data, int width, int height) {
  SDL_Surface *surf = create_surface_from_data(data, width, height);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
  if (texture == nullptr) {
    throw ExceptionSDL("Unable to create SDL texture from data");
  }

  SDL_DestroySurface(surf);

  return texture;
}

void
VideoSDL::draw_image(const Video::Image *image, int x, int y, int y_offset,
                        Video::Frame *dest) {
  SDL_FRect dest_rect = { x, y + y_offset,
                         static_cast<int>(image->w),
                         static_cast<int>(image->h - y_offset) };
  SDL_FRect src_rect = { 0, y_offset,
                        static_cast<int>(image->w),
                        static_cast<int>(image->h - y_offset) };

  /* Blit sprite */
  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  int r = SDL_RenderTexture(renderer, image->texture, &src_rect, &dest_rect);
  if (r < 0) {
    throw ExceptionSDL("RenderCopy error");
  }
}

void
VideoSDL::draw_frame(int dx, int dy, Video::Frame *dest, int sx, int sy,
                        Video::Frame *src, int w, int h) {
  SDL_FRect dest_rect = { dx, dy, w, h };
  SDL_FRect src_rect = { sx, sy, w, h };

  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  int r = SDL_RenderTexture(renderer, src->texture, &src_rect, &dest_rect);
  if (r < 0) {
    throw ExceptionSDL("RenderCopy error");
  }
}

void
VideoSDL::draw_rect(int x, int y, unsigned int width, unsigned int height,
                       const Video::Color color, Video::Frame *dest) {
  /* Draw rectangle. */
  fill_rect(x, y, width, 1, color, dest);
  fill_rect(x, y+height-1, width, 1, color, dest);
  fill_rect(x, y, 1, height, color, dest);
  fill_rect(x+width-1, y, 1, height, color, dest);
}

void
VideoSDL::fill_rect(int x, int y, unsigned int width, unsigned int height,
                       const Video::Color color, Video::Frame *dest) {
  SDL_FRect rect = { x, y, static_cast<int>(width), static_cast<int>(height) };

  /* Fill rectangle */
  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0xff);
  int r = SDL_RenderFillRect(renderer, &rect);
  if (r < 0) {
    throw ExceptionSDL("RenderFillRect error");
  }
}

void
VideoSDL::draw_line(int x, int y, int x1, int y1, const Video::Color color,
                    Video::Frame *dest) {
  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0xff);
  SDL_RenderLine(renderer, x, y, x1, y1);
}

void
VideoSDL::swap_buffers() {
  SDL_SetRenderTarget(renderer, nullptr);
  SDL_RenderTexture(renderer, screen->texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

void
VideoSDL::set_cursor(void *data, unsigned int width, unsigned int height) {
  if (cursor != nullptr) {
    SDL_SetCursor(nullptr);
    SDL_DestroyCursor(cursor);
    cursor = nullptr;
  }

  if (data == nullptr) {
    return;
  }

  SDL_Surface *surface = create_surface_from_data(data, width, height);
  cursor = SDL_CreateColorCursor(surface, 8, 8);
  SDL_SetCursor(cursor);
}

bool
VideoSDL::set_zoom_factor(float factor) {
  if ((factor < 0.2f) || (factor > 1.f)) {
    return false;
  }

  unsigned int width = 0;
  unsigned int height = 0;
  get_resolution(&width, &height);
  zoom_factor = factor;

  width = (unsigned int)(static_cast<float>(width) * zoom_factor);
  height = (unsigned int)(static_cast<float>(height) * zoom_factor);
  set_resolution(width, height, is_fullscreen());

  return true;
}

void
VideoSDL::get_screen_factor(float *fx, float *fy) {
  int w = 0;
  int h = 0;
  SDL_GetWindowSize(window, &w, &h);
  int rw = 0;
  int rh = 0;
  SDL_GetWindowSizeInPixels(window, &rw, &rh);
  if (fx != nullptr) {
    *fx = static_cast<float>(rw) / static_cast<float>(w);
  }
  if (fy != nullptr) {
    *fy = static_cast<float>(rh) / static_cast<float>(h);
  }
}
