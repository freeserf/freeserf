/*
 * font.h - TrueType font of the texts declaration
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


#ifndef SRC_FONT_H_
#define SRC_FONT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

/* The font of the texts, a TrueType font of the game's resources
   (resources/settlers.ttf). Its glyphs are pixels of the original font
   doubled (16 pixels per em, the steps smoothed with Scale2x): rasterized
   at 16 pixels and reduced by 2x2 blocks they are the pixels of the
   original 8x8 font again. */
class Font {
 public:
  /* Pixels of a glyph in the size of the original font. */
  class Glyph {
   public:
    /* Position of the pixels: from the pen position, and from the top of
       the letters (the 8 rows of the original glyphs). */
    int left;
    int top;
    unsigned int width;
    unsigned int height;
    int advance;  /* To the pen position of the next glyph. */
    std::vector<bool> pixels;

    bool get(unsigned int x, unsigned int y) const {
      return pixels[y * width + x];
    }
  };

 protected:
  void *font;
  std::map<uint32_t, std::unique_ptr<Glyph>> glyphs;

 public:
  Font();
  virtual ~Font();

  /* Open a font of the game's resources (the resources folder next to the
     program, in the bundle resources on macOS). */
  bool load_resource(const std::string &name);
  bool is_loaded() const { return font != nullptr; }

  /* The glyph of a Unicode character, nullptr when the font has none. */
  const Glyph *get_glyph(uint32_t ch);

 protected:
  bool load(const std::string &path);
  std::unique_ptr<Glyph> render(uint32_t ch);
};

#endif  // SRC_FONT_H_
