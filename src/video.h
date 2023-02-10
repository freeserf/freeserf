/*
 * video.h - Base for graphics rendering
 *
 * Copyright (C) 2015-2018  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_VIDEO_H_
#define SRC_VIDEO_H_

#include <exception>
#include <string>

#include <SDL.h>   // for SDL_Texture hack

#include "src/debug.h"

class ExceptionVideo : public ExceptionFreeserf {
 public:
  explicit ExceptionVideo(const std::string &description);
  virtual ~ExceptionVideo();

  virtual std::string get_description() const;
  virtual std::string get_platform() const { return "Abstract"; }
  virtual std::string get_system() const { return "video"; }
};

class Video {
 public:
  typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
  } Color;

  class Frame;
  class Image;

 protected:
  Video() {}

 public:
  virtual ~Video() {}

  static Video &get_instance();

  virtual void set_resolution(unsigned int width, unsigned int height,
                              bool fullscreen) = 0;
  virtual void get_resolution(unsigned int *width, unsigned int *height) = 0;
  virtual void set_fullscreen(bool enable) = 0;
  virtual bool is_fullscreen() = 0;

  virtual Frame *get_screen_frame() = 0;
  virtual Frame *get_unscaled_screen_frame() = 0; // NULL STUB
  virtual SDL_Texture *get_unscaled_screen_frame_texture() = 0; // NULL STUB
  virtual Frame *create_frame(unsigned int width,
                                      unsigned int height) = 0;
  virtual void destroy_frame(Frame *frame) = 0;

  virtual Image *create_image(void *data, unsigned int width,
                                      unsigned int height) = 0;
  virtual void destroy_image(Image *image) = 0;

  virtual void warp_mouse(int x, int y) = 0;

  virtual void draw_image(const Image *image, int x, int y,
                          int y_offset, Frame *dest) = 0;
  virtual void draw_frame(int dx, int dy, Frame *dest, int sx, int sy,
                          Frame *src, int w, int h) = 0;
  virtual void draw_rect(int x, int y, unsigned int width, unsigned int height,
                         const Video::Color color, Frame *dest) = 0;
  virtual void fill_rect(int x, int y, unsigned int width, unsigned int height,
                         const Video::Color color, Frame *dest) = 0;
  virtual void draw_line(int x, int y, int x1, int y1,
                         const Video::Color color, Frame *dest) = 0;
  virtual void draw_thick_line(int x, int y, int x1, int y1,
                         const Video::Color color, Frame *dest) = 0;

  //virtual void swap_buffers() = 0;
  virtual void render_viewport() = 0; // NULL STUB
  virtual void render_ui() = 0; // NULL STUB
  virtual void change_to_unscaled_render_target() = 0; // NULL STUB

  virtual void set_cursor(void *data, unsigned int width, unsigned int height) = 0;
  virtual void get_mouse_cursor_coord(int *x, int *y) = 0; // NULL STUB - pure virtual overloaded in video-sdl // trying to center zoom around MOUSE POINTER cursor pos (not selected tile on map cursor)

  virtual float get_zoom_factor() = 0;
  virtual bool set_zoom_factor(float factor) = 0;
  virtual int get_zoom_type() = 0;  // NULL STUB - pure virtual overloaded in video-sdl // trying to center zoom around MOUSE POINTER cursor pos (not selected tile on map cursor)
  virtual void set_zoom_type(int type) = 0;  // NULL STUB - pure virtual overloaded in video-sdl // trying to center zoom around MOUSE POINTER cursor pos (not selected tile on map cursor)
  virtual void get_screen_factor(float *fx, float *fy) = 0;
  virtual void get_screen_size(int *x, int *y) = 0; // NULL STUB - pure virtual overloaded in video-sdl // trying to center zoom around MOUSE POINTER cursor pos (not selected tile on map cursor)
  
};

#endif  // SRC_VIDEO_H_
