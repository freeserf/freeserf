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

#include <SDL.h>

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
  zoom_type = -1;

  Log::Info["video"] << "Initializing \"sdl\".";
  Log::Info["video"] << "Available drivers:";
  int num_drivers = SDL_GetNumVideoDrivers();
  for (int i = 0; i < num_drivers; ++i) {
    Log::Info["video"] << "\t" << SDL_GetVideoDriver(i);
  }

  /* Initialize defaults and Video subsystem */
  if (SDL_VideoInit(NULL) != 0) {
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
  window = SDL_CreateWindow("Forkserf",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            800, 600,
                            SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    throw ExceptionSDL("Unable to create SDL window");
  }

  /* Create renderer for window */
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED |
                                            SDL_RENDERER_TARGETTEXTURE);
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
  SDL_PixelFormatEnumToMasks(pixel_format, &bpp,
                             &Rmask, &Gmask, &Bmask, &Amask);

  /* Set scaling mode */  // i.e. zoom interpolation type
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");   // this is no aliasing/pixelart style
  //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // interpolated/blurred edges of pixels.  This is what Freeserf uses
  //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");  // this looks same as linear to me

  int w = 0;
  int h = 0;
  SDL_GL_GetDrawableSize(window, &w, &h);
  set_resolution(w, h, fullscreen);
}

VideoSDL::~VideoSDL() {
  if (screen != nullptr) {
    delete screen;
    screen = nullptr;
  }
  set_cursor(nullptr, 0, 0);
  SDL_VideoQuit();
}

Video &
Video::get_instance() {
  static VideoSDL instance;
  return instance;
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

void
VideoSDL::set_resolution(unsigned int width, unsigned int height, bool fs) {
  Log::Debug["video-sdl.cc"] << "inside VideoSDL::set_resolution, width " << width << ", height " << height << " fullscreen bool is " << fs;
  /* Set fullscreen mode */
  int r = SDL_SetWindowFullscreen(window,
                                  fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
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
  r = SDL_RenderSetLogicalSize(renderer, width, height);
  if (r < 0) {
    throw ExceptionSDL("Unable to set logical size");
  }

  fullscreen = fs;
}

void
VideoSDL::get_resolution(unsigned int *width, unsigned int *height) {
  int w = 0;
  int h = 0;
  SDL_GetRendererOutputSize(renderer, &w, &h);
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
  SDL_GetRendererOutputSize(renderer, &width, &height);
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
  SDL_Surface *surf_screen = SDL_ConvertSurfaceFormat(surf, pixel_format, 0);
  if (surf_screen == nullptr) {
    throw ExceptionSDL("Unable to convert sprite surface");
  }

  SDL_FreeSurface(surf);

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

  SDL_FreeSurface(surf);

  return texture;
}

void
VideoSDL::draw_image(const Video::Image *image, int x, int y, int y_offset,
                        Video::Frame *dest) {
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
    throw ExceptionSDL("RenderCopy error");
  }
}

void
VideoSDL::draw_frame(int dx, int dy, Video::Frame *dest, int sx, int sy,
                        Video::Frame *src, int w, int h) {
  SDL_Rect dest_rect = { dx, dy, w, h };
  SDL_Rect src_rect = { sx, sy, w, h };

  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  int r = SDL_RenderCopy(renderer, src->texture, &src_rect, &dest_rect);
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
  SDL_Rect rect = { x, y, static_cast<int>(width), static_cast<int>(height) };

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
  SDL_RenderDrawLine(renderer, x, y, x1, y1);
}

// draw a triple-thick line by drawing three lines
//  side-by-side, with the parallel line's axis
//  determined by the relationship between the lines
// otherwise it would be like a calligraphy pen where
//  one direction is thin and one thick
void
VideoSDL::draw_thick_line(int x, int y, int x1, int y1, const Video::Color color,
                    Video::Frame *dest) {
  SDL_SetRenderTarget(renderer, dest->texture);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0xff);

/*
  // find the slope of a straight line 
  float fx = x;
  float fx1 = x1;
  float fy = y;
  float fy1 = y1;
  float slope = (fy1 - fy) / (fx1 - fx); 
  Log::Info["video-sdl"] << "slope of x " << fx << ", x1 " << fx1 << " to y " << fy << ", y1 " << fy1 << " = " << std::to_string(slope);

*/

  // draw the original thin line...
  SDL_RenderDrawLine(renderer, x, y, x1, y1);
  
  /*
  if (slope >= 2.5 || slope <= -2.5){
    // up-down line, widen x axis by 2
    SDL_RenderDrawLine(renderer, x-1, y, x1-1, y1);
    //SDL_RenderDrawLine(renderer, x+1, y, x1+1, y1);
    //SDL_RenderDrawLine(renderer, x, y-1, x1, y1-1);
    //SDL_RenderDrawLine(renderer, x, y+1, x1, y1+1);
  }
  else if (slope <= 1 && slope >= -1){
    // left-right line, widen y axis by 1
    SDL_RenderDrawLine(renderer, x, y-1, x1, y1-1);
    //SDL_RenderDrawLine(renderer, x, y+1, x1, y1+1);
    //SDL_RenderDrawLine(renderer, x-1, y, x1-1, y1);
    //SDL_RenderDrawLine(renderer, x+1, y, x1+1, y1);
  }
  else{  //(slope >= 1 && slope < 2 || slope <= -1 && slope >= -2){
    //// diagonal line, widen x and y axis
    //SDL_RenderDrawLine(renderer, x-1, y-1, x1-1, y1-1);
    //SDL_RenderDrawLine(renderer, x+1, y+1, x1+1, y1+1);
    // diagonal line, widen x by 1 and y by 1, in caret shape
    SDL_RenderDrawLine(renderer, x-1, y, x1-1, y1);
    SDL_RenderDrawLine(renderer, x, y-1, x1, y1-1);
    */
    // draw four lines, in an + shape around the original line
    SDL_RenderDrawLine(renderer, x-1, y, x1-1, y1);
    SDL_RenderDrawLine(renderer, x+1, y, x1+1, y1);
    SDL_RenderDrawLine(renderer, x, y+1, x1, y1+1);
    SDL_RenderDrawLine(renderer, x, y-1, x1, y1-1);
  //}

  /*
  if (x >= x1 && y >= y1){
    // up-right quadrant
  }
  if (x <= x1 && y >= y1){
    // up-left quadrant
  }
  if (x <= x1 && y <= y1){
    // down-left quadrant
  }
  if (x >= x1 && y <= y1){
    // down-right quadrant
  }
  */
}


void
VideoSDL::swap_buffers() {
  SDL_SetRenderTarget(renderer, nullptr);
  SDL_RenderCopy(renderer, screen->texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

// this sets the mouse pointer cursor, NOT the game/map cursor
void
VideoSDL::set_cursor(void *data, unsigned int width, unsigned int height) {
  if (cursor != nullptr) {
    SDL_SetCursor(nullptr);
    SDL_FreeCursor(cursor);
    cursor = nullptr;
  }

  if (data == nullptr) {
    return;
  }

  SDL_Surface *surface = create_surface_from_data(data, width, height);
  cursor = SDL_CreateColorCursor(surface, 8, 8);
  SDL_SetCursor(cursor);
}

void
VideoSDL::get_mouse_cursor_coord(int *x, int *y){
  // it seems like this gives inverted x coord?
  SDL_GetMouseState(x, y);  // "set to the mouse cursor position relative to the focus window for the currently selected mouse"
  //SDL_GetRelativeMouseState(x, y);  // "set to the mouse deltas since the last call to SDL_GetRelativeMouseState() or since event initialization"
}

//
//  NOTES about attempt to scale Viewport separately from UI 
//    https://github.com/forkserf/forkserf/issues/282
//    https://stackoverflow.com/questions/328500/proper-way-to-scale-an-sdl-surface-without-clipping
//
// simpler explanation of below:
//   "have a bool that determines if UI or no-UI sprite being drawn, if not UI scale each sprite when drawn according to zoom factor.  The Resolution never changes"
//   "... and somehow avoid drawing anything that won't be seen"
//
//  try:
//   - change the existing zoom logic to NOT change the resolution?  but somehow still only draw the Viewport elements
//        that will actually be seen while zoomed (i.e don't waste time drawing anything that will end up outside the view when zoom applied)
//        but instead of drawing to a small rectangle and using resolution scaling(?), change RenderCopy to RenderCopyEx and use dsrect
//        to scale each drawn sprite according to the zoom factor, and draw onto the full window size, keeping the full resolution the entire time
//  *****************************************************************************************************************************************************************
//  TRY THIS FIRST BEFORE EVEN MESSING WITH UI DRAWING CHANGES.  SEE IF POSSIBLE TO GET SCALING WORKING ON ENTIRE GAME BY RenderCopyEx INSTEAD OF RESOLUTION SCALING!
//  *****************************************************************************************************************************************************************
//   - create a separate draw_image function for UI elements, have it use regular RenderCopy or just let dsrect be unscaled value
//   - inside the normal draw_image function, somehow check to see if a UI element is being drawn? 
//        a quick hack would be to set a global bool that is flipped on/off in Viewport::internal_draw() 
//        and Viewport::draw_game_objects() when the UI elements are drawn
//
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
VideoSDL::set_zoom_type(int type) {
  zoom_type = type;
}

void
VideoSDL::get_screen_factor(float *fx, float *fy) {
  int w = 0;
  int h = 0;
  SDL_GetWindowSize(window, &w, &h);
  int rw = 0;
  int rh = 0;
  SDL_GL_GetDrawableSize(window, &rw, &rh);
  if (fx != nullptr) {
    *fx = static_cast<float>(rw) / static_cast<float>(w);
  }
  if (fy != nullptr) {
    *fy = static_cast<float>(rh) / static_cast<float>(h);
  }
}

void
VideoSDL::get_screen_size(int *x, int *y) {
  SDL_GetWindowSize(window, x, y);
}
