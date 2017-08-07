/*
 * video-sdl.h - SDL graphics rendering
 *
 * Copyright (C) 2012-2015  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_VIDEO_SDL_H_
#define SRC_VIDEO_SDL_H_

#include <exception>
#include <string>

#include <SDL.h>

#include "src/video.h"

class Video::Frame {
 public:
  SDL_Texture *texture;

  Frame() : texture(NULL) {}
};

class Video::Image {
 public:
  unsigned int w;
  unsigned int h;
  SDL_Texture *texture;

  Image() : w(0), h(0), texture(NULL) {}
};

class ExceptionSDL : public ExceptionVideo {
 protected:
  std::string sdl_error;

 public:
  explicit ExceptionSDL(const std::string &description) throw();
  virtual ~ExceptionSDL() throw() {}

  virtual std::string get_platform() const { return "SDL"; }
};

class VideoSDL : public Video {
 protected:
  static int bpp;
  static Uint32 Rmask;
  static Uint32 Gmask;
  static Uint32 Bmask;
  static Uint32 Amask;
  static Uint32 pixel_format;

  SDL_Window *window;
  SDL_Renderer *renderer;
  Video::Frame *screen;
  bool fullscreen;
  SDL_Cursor *cursor;
  float zoom_factor;

 public:
  VideoSDL() throw(ExceptionVideo);
  virtual ~VideoSDL();

  virtual void set_resolution(unsigned int width, unsigned int height,
                              bool fullscreen) throw(ExceptionVideo);
  virtual void get_resolution(unsigned int *width, unsigned int *height);
  virtual void set_fullscreen(bool enable) throw(ExceptionVideo);
  virtual bool is_fullscreen();

  virtual Video::Frame *get_screen_frame();
  virtual Video::Frame *create_frame(unsigned int width, unsigned int height);
  virtual void destroy_frame(Video::Frame *frame);

  virtual Video::Image *create_image(void *data, unsigned int width,
                                     unsigned int height);
  virtual void destroy_image(Video::Image *image);

  virtual void warp_mouse(int x, int y);

  virtual void draw_image(const Video::Image *image, int x, int y,
                           int y_offset, Video::Frame *dest);
  virtual void draw_frame(int dx, int dy, Video::Frame *dest, int sx, int sy,
                          Video::Frame *src, int w, int h);
  virtual void draw_rect(int x, int y, unsigned int width, unsigned int height,
                         const Video::Color color, Video::Frame *dest);
  virtual void fill_rect(int x, int y, unsigned int width, unsigned int height,
                         const Video::Color color, Video::Frame *dest);
  virtual void draw_line(int x, int y, int x1, int y1,
                         const Video::Color color);

  virtual void swap_buffers();

  virtual void set_cursor(void *data, unsigned int width, unsigned int height);

  virtual float get_zoom_factor() { return zoom_factor; }
  virtual bool set_zoom_factor(float factor);

 protected:
  SDL_Surface *create_surface(int width, int height);
  SDL_Surface *create_surface_from_data(void *data, int width, int height);
  SDL_Texture *create_texture(int width, int height);
  SDL_Texture *create_texture_from_data(void *data, int width, int height);
};

#endif  // SRC_VIDEO_SDL_H_
