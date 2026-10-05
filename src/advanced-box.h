/*
 * advanced-box.h - Box of the advanced options
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_ADVANCED_BOX_H_
#define SRC_ADVANCED_BOX_H_

#include "src/gui.h"

class Interface;

/* Large box of the settings that the original game does not have, opened
   from the options box. */
class AdvancedBox : public GuiObject {
 public:
  static const int kWidth = 360;
  static const int kHeight = 256;

 protected:
  Interface *interface;

 public:
  explicit AdvancedBox(Interface *interface);
  virtual ~AdvancedBox() {}

 protected:
  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // SRC_ADVANCED_BOX_H_
