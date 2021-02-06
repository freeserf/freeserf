/*
 * text-input.h - Text input GUI component
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

#ifndef SRC_TEXT_INPUT_H_
#define SRC_TEXT_INPUT_H_

#include <string>

#include "src/gui.h"

class TextInput;

class TextInput : public GuiObject {
 public:
  typedef bool(*Filter)(const char key, TextInput *text_input);

 protected:
  std::string text;
  unsigned int max_length;
  Filter filter;
  Color color_focus;
  Color color_text;
  Color color_background;
  bool draw_focus;

 public:
  TextInput();

  void set_text(const std::string &text);
  std::string get_text();
  unsigned int get_max_length() { return max_length; }
  void set_max_length(unsigned int max_length);
  void set_filter(Filter _filter) { filter = _filter; }

 protected:
  virtual void internal_draw();

  virtual bool handle_click_left(int x, int y, int modifier);
  virtual bool handle_key_pressed(char key, int modifier);
  virtual bool handle_focus_loose();
};

#endif  // SRC_TEXT_INPUT_H_
