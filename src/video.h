/*
 * video.h - Base for graphics rendering
 *
 * Copyright (C) 2015  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/debug.h"

typedef struct {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
} video_color_t;

class Video_Exception : public Freeserf_Exception {
 public:
  explicit Video_Exception(const std::string &description) throw();
  virtual ~Video_Exception() throw();

  virtual std::string get_description() const;
  virtual std::string get_platform() const { return "Abstract"; }
  virtual std::string get_system() const { return "video"; }
};

class video_frame_t;
class video_image_t;

class video_t {
 protected:
  static video_t *instance;

  video_t() throw(Video_Exception);

 public:
  virtual ~video_t();

  static video_t *get_instance();

  virtual void set_resolution(unsigned int width, unsigned int height,
                              bool fullscreen) throw(Video_Exception) = 0;
  virtual void get_resolution(unsigned int *width, unsigned int *height) = 0;
  virtual void set_fullscreen(bool enable) throw(Video_Exception) = 0;
  virtual bool is_fullscreen() = 0;

  virtual video_frame_t *get_screen_frame() = 0;
  virtual video_frame_t *create_frame(unsigned int width,
                                      unsigned int height) = 0;
  virtual void destroy_frame(video_frame_t *frame) = 0;

  virtual video_image_t *create_image(void *data, unsigned int width,
                                      unsigned int height) = 0;
  virtual void destroy_image(video_image_t *image) = 0;

  virtual void warp_mouse(int x, int y) = 0;

  virtual void draw_image(const video_image_t *image, int x, int y,
                          int y_offset, video_frame_t *dest) = 0;
  virtual void draw_frame(int dx, int dy, video_frame_t *dest, int sx, int sy,
                          video_frame_t *src, int w, int h) = 0;
  virtual void draw_rect(int x, int y, unsigned int width, unsigned int height,
                         const video_color_t color, video_frame_t *dest) = 0;
  virtual void fill_rect(int x, int y, unsigned int width, unsigned int height,
                         const video_color_t color, video_frame_t *dest) = 0;
  virtual void swap_buffers() = 0;

  virtual void set_cursor(void *data, unsigned int width,
                          unsigned int height) = 0;

  virtual float get_zoom_factor() = 0;
  virtual bool set_zoom_factor(float factor) = 0;
};

#endif  // SRC_VIDEO_H_
