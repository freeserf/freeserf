/*
 * data-source-amiga.cc - Amiga data loading
 *
 * Copyright (C) 2016-2017  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/data-source-amiga.h"

#include <vector>
#include <sstream>
#include <string>
#include <memory>
#include <cstddef>
#include <cstring>

#include "src/data.h"
#include "src/freeserf_endian.h"
#include "src/sfx2wav.h"
#include "src/mod2wav.h"
#include "src/log.h"

uint8_t palette[] = {
  0x00, 0x00, 0x00,   // 0
  0xFF, 0xAA, 0x00,   // 1
  0x00, 0x00, 0x00,   // 2
  0x00, 0xEE, 0xEE,   // 3  // 1st player normal
  0x00, 0x00, 0xBB,   // 4
  0x44, 0x44, 0xDD,   // 5
  0x88, 0x88, 0xFF,   // 6
  0x00, 0xCC, 0xBB,   // 7  // 1st player dark
  0x22, 0x44, 0x00,   // 8
  0x33, 0x55, 0x00,   // 9
  0x33, 0x66, 0x33,   // 10
  0xEE, 0x88, 0xFF,   // 11
  0x44, 0x88, 0x00,   // 12
  0x66, 0x99, 0x00,   // 13
  0x77, 0xBB, 0x44,   // 14
  0xCC, 0x77, 0xDD,   // 15
  0x44, 0x44, 0x44,   // 16
  0x99, 0x99, 0x99,   // 17
  0xFF, 0xFF, 0xFF,   // 18
  0xEE, 0x66, 0x66,   // 19
  0x22, 0x22, 0x22,   // 20
  0x66, 0x66, 0x66,   // 21
  0xCC, 0xCC, 0xCC,   // 22
  0xDD, 0x33, 0x33,   // 23
  0x55, 0x22, 0x00,   // 24
  0x77, 0x33, 0x00,   // 25
  0x99, 0x55, 0x22,   // 26
  0xFF, 0xFF, 0x99,   // 27
  0xBB, 0x88, 0x55,   // 28
  0xDD, 0xAA, 0x77,   // 29
  0xFF, 0xDD, 0xBB,   // 30
  0xDD, 0xDD, 0x00,   // 31
};

uint8_t palette_intro[] = {
  0x00, 0x00, 0x00,   // 0
  0x55, 0x22, 0x00,   // 1
  0x77, 0x33, 0x00,   // 2
  0x99, 0x55, 0x22,   // 3
  0xBB, 0x88, 0x55,   // 4
  0xDD, 0xAA, 0x77,   // 5
  0xFF, 0xDD, 0xBB,   // 6
  0x00, 0xCC, 0xBB,   // 7
  0x22, 0x44, 0x00,   // 8
  0x33, 0x55, 0x00,   // 9
  0x33, 0x66, 0x33,   // 10
  0xEE, 0x88, 0xFF,   // 11
  0x44, 0x88, 0x00,   // 12
  0x66, 0x99, 0x00,   // 13
  0x77, 0xBB, 0x44,   // 14
  0xCC, 0x77, 0xDD,   // 15
  0x44, 0x44, 0x44,   // 16
  0x99, 0x99, 0x99,   // 17
  0xFF, 0xFF, 0xFF,   // 18
  0xEE, 0x66, 0x66,   // 19
  0x22, 0x22, 0x22,   // 20
  0x66, 0x66, 0x66,   // 21
  0xCC, 0xCC, 0xCC,   // 22
  0xDD, 0x33, 0x33,   // 23
  0x55, 0x22, 0x00,   // 24
  0x77, 0x33, 0x00,   // 25
  0x99, 0x55, 0x22,   // 26
  0xFF, 0xFF, 0x99,   // 27
  0xBB, 0x88, 0x55,   // 28
  0xDD, 0xAA, 0x77,   // 29
  0xFF, 0xDD, 0xBB,   // 30
  0xDD, 0xDD, 0x00,   // 31
};

uint8_t palette_logo[] = {
  0x00, 0x00, 0x00,   // 0
  0x44, 0x66, 0xDD,   // 1
  0x00, 0x00, 0x00,   // 2
  0x00, 0xEE, 0xEE,   // 3
  0x00, 0x00, 0xBB,   // 4
  0x44, 0x44, 0xDD,   // 5
  0x88, 0x88, 0xFF,   // 6
  0x00, 0xCC, 0xBB,   // 7
  0x22, 0x44, 0x00,   // 8
  0x33, 0x55, 0x00,   // 9
  0x33, 0x66, 0x33,   // 10
  0x66, 0x88, 0xEE,   // 11
  0x44, 0x88, 0x00,   // 12
  0x66, 0x99, 0x00,   // 13
  0x77, 0xBB, 0x44,   // 14
  0x00, 0x00, 0xDD,   // 15
  0x44, 0x44, 0x44,   // 16
  0x99, 0x99, 0x99,   // 17
  0xFF, 0xFF, 0xFF,   // 18
  0xEE, 0x66, 0x66,   // 19
  0x22, 0x22, 0x22,   // 20
  0x66, 0x66, 0x66,   // 21
  0xCC, 0xCC, 0xCC,   // 22
  0xDD, 0x33, 0x33,   // 23
  0x55, 0x22, 0x00,   // 24
  0x77, 0x33, 0x00,   // 25
  0x99, 0x55, 0x22,   // 26
  0x55, 0x77, 0xDD,   // 27
  0xBB, 0x88, 0x55,   // 28
  0xDD, 0xAA, 0x77,   // 29
  0xFF, 0xDD, 0xBB,   // 30
  0xDD, 0xDD, 0x00,   // 31
};

uint8_t palette_symbols[] = {
  0x00, 0x00, 0x00,   // 0
  0xFF, 0xAA, 0x00,   // 1
  0x00, 0x00, 0x00,   // 2
  0x00, 0xEE, 0xEE,   // 3
  0x00, 0x00, 0xBB,   // 4
  0x44, 0x44, 0xDD,   // 5
  0x88, 0x88, 0xFF,   // 6
  0x00, 0xCC, 0xBB,   // 7
  0x22, 0x44, 0x00,   // 8
  0x33, 0x55, 0x00,   // 9
  0x33, 0x66, 0x33,   // 10
  0xEE, 0x88, 0xFF,   // 11
  0x44, 0x88, 0x00,   // 12
  0x66, 0x99, 0x00,   // 13
  0x77, 0xBB, 0x44,   // 14
  0xCC, 0x77, 0xDD,   // 15
  0x44, 0x44, 0x44,   // 16
  0x99, 0x99, 0x99,   // 17
  0xFF, 0xFF, 0xFF,   // 18
  0xEE, 0x66, 0x66,   // 19
  0x22, 0x22, 0x22,   // 20
  0x66, 0x66, 0x66,   // 21
  0xCC, 0xCC, 0xCC,   // 22
  0xDD, 0x33, 0x33,   // 23
  0x55, 0x22, 0x00,   // 24
  0x77, 0x33, 0x00,   // 25
  0x99, 0x55, 0x22,   // 26
  0xFF, 0xFF, 0x99,   // 27
  0xBB, 0x88, 0x55,   // 28
  0xDD, 0xAA, 0x77,   // 29
  0xFF, 0xDD, 0xBB,   // 30
  0xDD, 0xDD, 0x00,   // 31
};

uint8_t palette2[] = {
  0x00, 0x00, 0x00,  // 0
  0x44, 0x44, 0xEE,  // 1
  0x55, 0xCC, 0x00,  // 2
  0x88, 0xFF, 0x00,  // 3
  0xCC, 0x33, 0x33,  // 4
  0x44, 0x44, 0x44,  // 5
  0x99, 0x99, 0x99,  // 6
  0xFF, 0xFF, 0xFF,  // 7
  0x33, 0x33, 0x33,  // 8
  0xCC, 0xCC, 0xCC,  // 9
  0x55, 0x22, 0x00,  // 10
  0x77, 0x33, 0x11,  // 11
  0x99, 0x55, 0x22,  // 12
  0xBB, 0x88, 0x55,  // 13
  0xDD, 0xAA, 0x77,  // 14
  0xFF, 0xDD, 0xBB,  // 15
  0x00, 0x00, 0x00,  // 16 ???
  0x00, 0x00, 0x00,  // 17 ???
  0x00, 0x00, 0x00,  // 18 ???
  0x00, 0x00, 0x00,  // 19 ???
  0x00, 0x00, 0x00,  // 20 ???
  0xFF, 0xFF, 0x00,  // 21
  0xCC, 0xAA, 0x11,  // 22
  0xAA, 0x77, 0x11,  // 23
  0x00, 0x00, 0x00,  // 24 ???
  0xFF, 0xFF, 0x00,  // 25
  0xCC, 0xAA, 0x11,  // 26
  0xAA, 0x77, 0x11,  // 27
  0x00, 0x00, 0x00,  // 28 ???
  0x00, 0x00, 0x00,  // 29 ???
  0x00, 0x00, 0x00,  // 30 ???
  0x00, 0x00, 0x00,  // 31 ???
};

uint8_t palette3[] = {
  0x00, 0x00, 0x00,   // 0
  0xFF, 0xAA, 0x00,   // 1
  0x00, 0x00, 0x00,   // 2
  0xFF, 0x00, 0xEE,   // 3  // 1st player normal
  0x00, 0x00, 0xBB,   // 4
  0x44, 0x44, 0xDD,   // 5
  0x88, 0x88, 0xFF,   // 6
  0xFF, 0x00, 0xBB,   // 7  // 1st player dark
  0x22, 0x44, 0x00,   // 8
  0x33, 0x55, 0x00,   // 9
  0x33, 0x66, 0x33,   // 10
  0xEE, 0x88, 0xFF,   // 11
  0x44, 0x88, 0x00,   // 12
  0x66, 0x99, 0x00,   // 13
  0x77, 0xBB, 0x44,   // 14
  0xCC, 0x77, 0xDD,   // 15
  0x44, 0x44, 0x44,   // 16
  0x99, 0x99, 0x99,   // 17
  0xFF, 0xFF, 0xFF,   // 18
  0xEE, 0x66, 0x66,   // 19
  0x22, 0x22, 0x22,   // 20
  0x66, 0x66, 0x66,   // 21
  0xCC, 0xCC, 0xCC,   // 22
  0xDD, 0x33, 0x33,   // 23
  0x55, 0x22, 0x00,   // 24
  0x77, 0x33, 0x00,   // 25
  0x99, 0x55, 0x22,   // 26
  0xFF, 0xFF, 0x99,   // 27
  0xBB, 0x88, 0x55,   // 28
  0xDD, 0xAA, 0x77,   // 29
  0xFF, 0xDD, 0xBB,   // 30
  0xDD, 0xDD, 0x00,   // 31
};

DataSourceAmiga::DataSourceAmiga(const std::string &_path)
  : DataSourceLegacy(_path) {
}

DataSourceAmiga::~DataSourceAmiga() {
}

bool
DataSourceAmiga::check() {
  std::vector<std::string> data_files = {
    "gfxheader",  // catalog file
    "gfxfast",    // fast graphics file
    "gfxchip"     // chip graphics file
  };

  for (std::string file_name : data_files) {
    std::string cp = path + '/' + file_name;
    Log::Info["data"] << "Looking for game data in '" << cp << "'...";
    if (!check_file(cp)) {
      return false;
    }
  }

  return true;
}

bool
DataSourceAmiga::load() {
  try {
    gfxfast = std::make_shared<Buffer>(path + "/gfxfast", Buffer::EndianessBig);
    gfxfast = decode(gfxfast);
    gfxfast = unpack(gfxfast);
    Log::Debug["data"] << "Data file 'gfxfast' loaded (size = "
                       << gfxfast->get_size() << ")";
  } catch (...) {
    Log::Error["data"] << "Failed to load 'gfxfast'";
    return false;
  }

  try {
    gfxchip = std::make_shared<Buffer>(path + "/gfxchip", Buffer::EndianessBig);
    gfxchip = decode(gfxchip);
    gfxchip = unpack(gfxchip);
    Log::Debug["data"] << "Data file 'gfxchip' loaded (size = "
                       << gfxchip->get_size() << ")";
  } catch (...) {
    Log::Error["data"] << "Failed to load 'gfxchip'";
    return false;
  }

  PBuffer gfxheader;
  try {
    gfxheader = std::make_shared<Buffer>(path + "/gfxheader",
                                         Buffer::EndianessBig);
  } catch (...) {
    Log::Error["data"] << "Failed to load 'gfxheader'";
    return false;
  }

  // Prepare icons catalog
  size_t icon_catalog_offset = gfxheader->pop<uint16_t>();
  size_t icon_catalog_size = gfxheader->pop<uint16_t>();
  PBuffer icon_catalog_tmp = gfxfast->get_subbuffer(icon_catalog_offset * 4,
                                                    icon_catalog_size * 4);
  for (unsigned int i = 0; i < icon_catalog_size; i++) {
    size_t offset = icon_catalog_tmp->pop<uint32_t>();
    icon_catalog.push_back(offset);
  }

  // Prepare data pointer bases
  data_pointers[1] = gfxfast;   // Animations
  data_pointers[2] = gfxfast;   // Ground masks catalog (4 * 81)
  data_pointers[3] = gfxfast;   // Path sprites catalog (4 * 27)
  data_pointers[4] = gfxfast;   // Ground sprites catalog (4 * 32)
  data_pointers[5] = gfxchip;   // ?
  data_pointers[6] = gfxfast;   // Map objects catalog (4 * 194)
  data_pointers[7] = gfxfast;   // Hud multiplayer
  data_pointers[8] = gfxchip;   // Borders
  data_pointers[9] = gfxchip;   // Waves
  data_pointers[10] = gfxchip;  // Popup frame horizontal
  data_pointers[11] = gfxfast;  // Popup frame vertical
  data_pointers[12] = gfxfast;  // Cursor
  data_pointers[13] = gfxfast;  // Icons catalog
  data_pointers[14] = gfxfast;  // Font data (8 * 44)
  data_pointers[15] = gfxfast;  // Game objects catalog
  data_pointers[16] = gfxfast;  // Panel buttons (17 images)
  data_pointers[17] = gfxfast;  // Rotated star catalog
  data_pointers[18] = gfxfast;  // Hud
  data_pointers[19] = gfxfast;  // Serf torse+arms catalog
  data_pointers[20] = gfxfast;  // Serf heads
  data_pointers[21] = gfxchip;  // Screen top
  data_pointers[22] = gfxchip;  // Screen sides (2 * 1864)
  data_pointers[23] = gfxfast;  // Title (1 * 43200)

  for (unsigned int i = 1; i < gfxheader->get_size()/4; i++) {
    size_t black_offset = gfxheader->pop<uint32_t>();
//    Log::Warn["data"] << "Block " << i << " : " << black_offset;
    data_pointers[i] = data_pointers[i]->get_tail(black_offset);
  }

  try {
    sound = std::make_shared<Buffer>(path + "/sounds");
    sound = decode(sound);
  } catch (...) {
    Log::Warn["data"] << "Failed to load 'sounds'";
    sound = nullptr;
  }

  try {
    PBuffer gfxpics = std::make_shared<Buffer>(path + "/gfxpics",
                                               Buffer::EndianessBig);
    for (size_t i = 0; i < 14; i++) {
      uint32_t offset = gfxpics->pop<uint32_t>();
      uint32_t size = gfxpics->pop<uint32_t>();
      pics[i] = gfxpics->get_subbuffer(28*4 + offset, size);
      pics[i] = decode(pics[i]);
      pics[i] = unpack(pics[i]);
    }
    Log::Debug["data"] << "Data file 'gfxpics' loaded (size = "
                       << gfxpics->get_size() << ")";
  } catch (...) {
    Log::Warn["data"] << "Failed to load 'gfxpics'";
  }

  return load_animation_table(data_pointers[1]->get_subbuffer(0, 30528));
}

DataSource::MaskImage
DataSourceAmiga::get_sprite_parts(Data::Resource res, size_t index) {
  PSprite sprite;

  switch (res) {
    case Data::AssetArtLandscape:
      break;
    case Data::AssetSerfShadow: {
      uint16_t shadow[6] = {betoh<uint16_t>(0x01C0), betoh<uint16_t>(0x07C0),
                            betoh<uint16_t>(0x1F80), betoh<uint16_t>(0x7F00),
                            betoh<uint16_t>(0xFE00), betoh<uint16_t>(0xFC00)};
      PBuffer data = std::make_shared<Buffer>(shadow, 12);
      PSpriteAmiga m = decode_mask_sprite(data, 16, 6);
      m->fill_masked({0x00, 0x00, 0x00, 0x80});
      m->set_delta(2, 0);
      m->set_offset(-2, -7);
      sprite = m;
      break;
    }
    case Data::AssetDottedLines:
      break;
    case Data::AssetArtFlag:
      break;
    case Data::AssetArtBox:
      if ((index < pics.size()) && pics[index]) {
        sprite = decode_interlased_sprite(pics[index], 16, 144, 0, 0, palette);
      }
      break;
    case Data::AssetCreditsBg:
      sprite = decode_interlased_sprite(data_pointers[23], 40, 200, 0, 0,
                                        palette_intro);
      break;
    case Data::AssetLogo: {
      PBuffer data = gfxfast->get_tail(188356);
      sprite = decode_interlased_sprite(data, 64/8, 96, 0, 0, palette_logo);
      break;
    }
    case Data::AssetSymbol: {
      PBuffer data = gfxfast->get_tail(178116 + (640*index));
      sprite = decode_interlased_sprite(data, 32/8, 32, 0, 0, palette_symbols);
      break;
    }
    case Data::AssetMapMaskUp:
      sprite = get_ground_mask_sprite(index);
      break;
    case Data::AssetMapMaskDown: {
      PSprite mask = get_ground_mask_sprite(index);
      if (mask) {
        PSpriteAmiga s = get_mirrored_horizontaly_sprite(mask);
        s->set_offset(0, -(static_cast<int>(s->get_height()) - 1));
        sprite = s;
      }
      break;
    }
    case Data::AssetPathMask:
      sprite = get_path_mask_sprite(index);
      break;
    case Data::AssetMapGround: {
      PSpriteAmiga s;
      if (index == 32) {
        s = std::make_shared<SpriteAmiga>(32, 21);
        Sprite::Color c = {0xBB, 0x00, 0x00, 0xFF};
        s->fill(c);
      } else {
        s = get_ground_sprite(index);
      }
      sprite = s;
      break;
    }
    case Data::AssetPathGround:
      sprite = get_ground_sprite(index);
      break;
    case Data::AssetGameObject:
      sprite = get_game_object_sprite(15, index+1);
      break;
    case Data::AssetFrameTop:
      if (index < 2) {
        sprite = decode_amiga_sprite(data_pointers[22]->get_tail(1864*index),
                                     2, 233, palette);
      } else if (index == 2) {
        sprite = decode_planned_sprite(data_pointers[21], 39, 8, 24, 24,
                                       palette);
      } else if (index == 3) {
        PSpriteAmiga left = decode_interlased_sprite(data_pointers[7], 2, 216,
                                                     0, 0, palette);
        PSpriteAmiga right = decode_interlased_sprite(
                                               data_pointers[7]->get_tail(2160),
                                                       2, 216, 0, 0, palette);
        sprite = left->merge_horizontaly(right);
      }
      break;
    case Data::AssetMapBorder: {
      PBuffer data = data_pointers[8]->get_tail(index * 120);
      PSpriteAmiga s = decode_interlased_sprite(data, 2, 6, 0, 0, palette);
      data = data->get_tail(60);
      PSpriteAmiga m = decode_interlased_sprite(data, 2, 6, 0, 0, palette);
      m->make_transparent();
      sprite = s->get_masked(m);
      break;
    }
    case Data::AssetMapWaves: {
      PBuffer data = data_pointers[9]->get_tail(index * 228);
      PSpriteAmiga s = decode_interlased_sprite(data, 6, 19, 28, 5, palette);
      s->make_transparent(0x0000BB);
      s->set_delta(1, 0);
      sprite = s;
      break;
    }
    case Data::AssetFramePopup:
      if (index == 0) {
        sprite = decode_interlased_sprite(data_pointers[10], 18, 9, 0, 0,
                                          palette, 1);
      } else if (index == 1) {
        sprite = decode_interlased_sprite(data_pointers[10]->get_tail(972),
                                          18, 7, 0, 0, palette, 1);
      } else {
        PSpriteAmiga s = decode_interlased_sprite(data_pointers[11], 2, 144,
                                                  0, 0, palette);
        sprite = s->split_horizontaly(index == 3);
      }
      break;
    case Data::AssetIndicator:
//      sprite = decode_planned_sprite(reinterpret_cast<uint8_t*>(gfxfast) +
//                                     43076, 1, 7, 0, 0);
//      if (index > 3) {
//        index -= 4;
//      }
//    sprite = decode_amiga_sprite(reinterpret_cast<uint8_t*>(gfxfast) + 43076,
//                                 5, 7, palette);  // 5 indicators WTF?
//    10 indicators WTF?
      sprite = decode_amiga_sprite(gfxfast->get_tail(44676), 10, 7, palette2);
      break;
    case Data::AssetFont: {
      PBuffer data = data_pointers[14]->get_subbuffer(index * 8, 8);
      PSpriteAmiga s = decode_mask_sprite(data, 8, 8);
      return std::make_tuple(s, nullptr);
    }
    case Data::AssetFontShadow: {
      PBuffer data = data_pointers[14]->get_subbuffer(index * 8, 8);
      PSpriteAmiga s = decode_mask_sprite(data, 8, 8);
      return std::make_tuple(make_shadow_from_symbol(s), nullptr);
    }
    case Data::AssetIcon:
      sprite = get_icon_sprite(index);
      break;
    case Data::AssetMapObject:
      if ((index >= 128) && (index <= 143)) {  // Flag sprites
        int flag_frame = (index - 128) % 4;
        PSprite s1 = get_map_object_sprite(128 + flag_frame);
        PSprite s2 = get_map_object_sprite(128 + flag_frame + 4);
        return separate_sprites(s1, s2);
      }
      sprite = get_map_object_sprite(index);
      break;
    case Data::AssetMapShadow:
      sprite = get_map_object_shadow(index);
      break;
    case Data::AssetPanelButton:
      if (index < 17) {
        sprite = get_menu_sprite(index, data_pointers[16], 32, 32, 16, 0);
      } else {
        index -= 17;
        unsigned int backs[] = {3, 4, 10, 12, 14, 16, 2, 8};
        PSpriteAmiga s = get_menu_sprite(backs[index], data_pointers[16],
                                         32, 32, 16, 0);
        PBuffer l_data = get_data_from_catalog(17, 0, gfxchip);
        PBuffer r_data = get_data_from_catalog(17, 16, gfxchip);
        PSpriteAmiga left = decode_interlased_sprite(l_data, 2, 29, 28, 20,
                                                     palette2);
        left->make_transparent();
        PSpriteAmiga right = decode_interlased_sprite(r_data, 2, 29, 28, 20,
                                                      palette2);
        right->make_transparent();
        PSpriteAmiga star = left->merge_horizontaly(right);
        s->stick(star, 0, 1);
        sprite = s;
      }
      break;
    case Data::AssetFrameBottom:
      if (index == 0) {
        PSpriteAmiga s = get_hud_sprite(6);
        sprite = s->split_horizontaly(true);
      } else if (index == 1) {
        PSpriteAmiga s = get_hud_sprite(6);
        sprite = s->split_horizontaly(false);
      } else if (index == 2) {
        sprite = get_hud_sprite(18);
      } else if (index == 4) {
        sprite = get_hud_sprite(19);
      } else if (index == 6) {
        sprite = get_hud_sprite(17);
      } else if ((index > 6) && (index < 17)) {
        sprite = get_hud_sprite(7 + (index - 7));
      } else if (index > 19) {
        sprite = get_hud_sprite(index - 20);
      }
      break;
    case Data::AssetSerfTorso: {
      PSprite s1 = get_torso_sprite(index, palette);
      PSprite s2 = get_torso_sprite(index, palette3);
      return separate_sprites(s1, s2);
    }
    case Data::AssetSerfHead:
      sprite = get_game_object_sprite(20, index);
      break;
    case Data::AssetFrameSplit:
      break;
    case Data::AssetCursor: {
      PBuffer data = data_pointers[12];
      PSpriteAmiga s = decode_interlased_sprite(data, 2, 16, 28, 16, palette);
      s->make_transparent(0x00444444);
      sprite = s;
      break;
    }
    default:
      break;
  }

  return std::make_tuple(nullptr, sprite);
}

typedef struct SoundStruct {
  ptrdiff_t offset;
  size_t size;
} SoundStruct;

std::vector<SoundStruct> sound_info = { {
  {    0,     0},
  {    0,  2704},
  { 2704,  1134},
  {    0,     0},
  { 3838,  2420},
  {    0,     0},
  { 6258,  1014},
  {    0,     0},
  { 7272,    78},
  {    0,     0},
  { 7350,  2114},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  { 9464,  2644},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {12108,  2258},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {14366,  1426},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {15792,  4108},
  {    0,     0},
  {19900,   894},
  {    0,     0},
  {20794,  1524},
  {    0,     0},
  {22318,  1088},
  {    0,     0},
  {23406,  1774},
  {    0,     0},
  {25180,  1094},
  {    0,     0},
  {26274,   780},
  {    0,     0},
  {27054,   484},
  {    0,     0},
  {27538,  1944},
  {27538,  1944},
  {29482,   916},
  {    0,     0},
  {30398,   470},
  {    0,     0},
  {30868,   608},
  {    0,     0},
  {31476,  1894},
  {    0,     0},
  {33370,  1174},
  {    0,     0},
  {34544,   300},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {34844,   682},
  {    0,     0},
  {35526,  1170},
  {    0,     0},
  {36696,  2294},
  {    0,     0},
  {38990,  3388},
  {    0,     0},
  {42378,  2670},
  {    0,     0},
  {    0,     0},
  {45048,  3340},
  {48388,   954},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {49342,  3540},
  {    0,     0},
  {52882,  5868},
  {    0,     0},
  {58750,  4436},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {63186,  7334},
  {    0,     0},
  {70520, 11304},
  {    0,     0},
  {81824, 14874},
  {    0,     0},
  {96698, 15312},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {    0,     0},
  {    0,     0}
} };

PBuffer
DataSourceAmiga::get_sound_data(size_t index) {
  if (sound) {
    if (index < sound_info.size()) {
      if (sound_info[index].size != 0) {
        return sound->get_subbuffer(sound_info[index].offset,
                                    sound_info[index].size);
      }
    }
  }

  return nullptr;
}

PBuffer
DataSourceAmiga::get_sound(size_t index) {
  PBuffer data = get_sound_data(index);
  if (!data) {
    Log::Error["data"] << "Sound sample with index" << index << " not present.";
    return nullptr;
  }

  try {
    ConvertorSFX2WAV convertor(data);
    return convertor.convert();
  } catch (...) {
    Log::Error["data"] << "Could not convert SFX clip to WAV: #" << index;
    return nullptr;
  }
}

PBuffer
DataSourceAmiga::get_music(size_t /*index*/) {
  if (music) {
    return music;
  }

  PBuffer data;
  try {
    data = std::make_shared<Buffer>(path + "/music");
    data = decode(data);
    data = unpack(data);
  } catch (...) {
    Log::Warn["data"] << "Failed to load 'music'";
    return nullptr;
  }

  // Amiga music file starts with music-player code, we must drop it
  char *str = reinterpret_cast<char*>(data->get_data());
  std::string string(str, str + data->get_size());
  size_t pos = string.find("M!K!");
  if (pos == std::string::npos) {
    return nullptr;
  }
  pos -= 1080;

  PBuffer mod = data->get_tail(pos);

  try {
    ConvertorMOD2WAV converter(mod);
    music = converter.convert();
  } catch (ExceptionFreeserf &e) {
    Log::Error["data"] << e.get_description();
    music = nullptr;
  }

  return music;
}

PBuffer
DataSourceAmiga::decode(PBuffer data) {
  PMutableBuffer result = std::make_shared<MutableBuffer>(data->get_size(),
                                                          Buffer::EndianessBig);
  for (size_t i = 0; i < data->get_size(); i++) {
    result->push<uint8_t>(data->pop<uint8_t>() ^ static_cast<uint8_t>(i));
  }
  return result;
}

PBuffer
DataSourceAmiga::unpack(PBuffer data) {
  PMutableBuffer result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);
  uint8_t flag = data->pop<uint8_t>();

  while (data->readable()) {
    uint8_t val = data->pop<uint8_t>();
    size_t count = 0;

    if (val == flag) {
      count = data->pop<uint8_t>();
      val = data->pop<uint8_t>();
    }

    for (unsigned int i = 0; i < count + 1; i++) {
      result->push<uint8_t>(val);
    }
  }

  return result;
}

unsigned int
DataSourceAmiga::bitplane_count_from_compression(unsigned char compression) {
  unsigned int bpp = 5;

  for (int i = 0 ; i < 5 ; i++) {
    if (compression & 0x01) {
      bpp--;
    }
    compression = compression >> 1;
  }

  return bpp;
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::get_menu_sprite(size_t index, PBuffer block,
                                 size_t width, size_t height,
                                 unsigned char compression,
                                 unsigned char filling) {
  unsigned int bpp = bitplane_count_from_compression(compression);
  size_t sprite_size = (width * height) / 8 * bpp;
  PBuffer data = block->get_tail(sprite_size * index);
  return decode_interlased_sprite(data, width/8, height, compression, filling,
                                  palette2);
}

PSprite
DataSourceAmiga::get_icon_sprite(size_t index) {
  PBuffer data = gfxfast->get_tail(icon_catalog[index]);

  uint16_t width = data->pop<uint16_t>();
  uint16_t height = data->pop<uint16_t>();
  uint8_t filling = data->pop<uint8_t>();
  uint8_t compression = data->pop<uint8_t>();

  return decode_planned_sprite(data->pop_tail(), width, height, compression,
                               filling, palette);
}

static uint8_t
invert5bit(uint8_t src) {
  uint8_t res = 0;

  for (int i = 0; i < 5; i++) {
    res <<= 1;
    res |= (src & 0x01);
    src >>= 1;
  }

  return res;
}

PBuffer
DataSourceAmiga::get_data_from_catalog(size_t catalog_index, size_t index,
                                       PBuffer base) {
  uint32_t *catalog = reinterpret_cast<uint32_t*>(
                                      data_pointers[catalog_index]->get_data());
  uint32_t offset = betoh(catalog[index]);
  if (offset == 0) {
    return nullptr;
  }

  return base->get_tail(offset);
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::get_ground_sprite(size_t index) {
  PBuffer data = get_data_from_catalog(4, index, gfxchip);
  if (!data) {
    return nullptr;
  }

  uint8_t filled = data->pop<uint8_t>();
  uint8_t compressed = data->pop<uint8_t>();

  PSpriteAmiga sprite = decode_planned_sprite(data->pop_tail(), 4, 21,
                                              compressed, filled, palette);

  if (sprite) {
    sprite->set_delta(1, 0);
    sprite->set_offset(0, 0);
  }

  return sprite;
}

PSprite
DataSourceAmiga::get_ground_mask_sprite(size_t index) {
  PBuffer data;
  if (index == 0) {
    data = gfxchip->get_tail(0);
  } else {
    data = get_data_from_catalog(2, index, gfxchip);
  }

  if (!data) {
    return nullptr;
  }
  uint16_t height = data->pop<uint16_t>();

  PSpriteAmiga sprite = std::make_shared<SpriteAmiga>(32, height);
  sprite->set_delta(2, 0);
  sprite->set_offset(0, 0);

  uint32_t *pixel = reinterpret_cast<uint32_t*>(sprite->get_data());
  for (int i = 0 ; i < 32*height/8 ; i++) {
    uint8_t byte = data->pop<uint8_t>();
    for (unsigned int j = 0 ; j < 8; j++) {
      if (((byte >> (7-j)) & 0x01) == 0x01) {
        *pixel = 0xFFFFFFFF;
      } else {
        *pixel = 0x00000000;
      }
      pixel++;
    }
  }

  return sprite;
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::get_mirrored_horizontaly_sprite(PSprite sprite) {
  if (!sprite) {
    return nullptr;
  }
  PSpriteAmiga result = std::make_shared<SpriteAmiga>(sprite->get_width(),
                                                      sprite->get_height());
  result->set_delta(sprite->get_delta_x(), sprite->get_delta_y());
  result->set_offset(sprite->get_offset_x(), sprite->get_offset_y());

  uint8_t *src_pixels = reinterpret_cast<uint8_t*>(sprite->get_data());
  uint8_t *dst_pixels = reinterpret_cast<uint8_t*>(result->get_data()) +
                        result->get_width() * (result->get_height() - 1) * 4;
  for (unsigned int i = 0 ; i < sprite->get_height() ; i++) {
    memcpy(dst_pixels, src_pixels, sprite->get_width() * 4);
    src_pixels += sprite->get_width() * 4;
    dst_pixels -= sprite->get_width() * 4;
  }

  return result;
}

PSprite
DataSourceAmiga::get_path_mask_sprite(size_t index) {
  PBuffer data = get_data_from_catalog(3, index, gfxchip);
  if (!data) {
    return nullptr;
  }

  uint8_t n = data->pop<uint8_t>();
  uint8_t k = data->pop<uint8_t>();

  int width = (k == 66) ? 32 : 16;

  int heghts[] = { 0, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41 };

  PSpriteAmiga sprite = decode_mask_sprite(data->pop_tail(), width, heghts[n]);
  sprite->set_delta(2, 0);
  sprite->set_offset(0, 0);

  return sprite;
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::decode_mask_sprite(PBuffer data, size_t width, size_t height) {
  PSpriteAmiga sprite = std::make_shared<SpriteAmiga>(width, height);

  size_t size = width/8 * height;

  uint32_t *pixel = reinterpret_cast<uint32_t*>(sprite->get_data());
  for (unsigned int i = 0 ; i < size ; i++) {
    uint8_t byte = data->pop<uint8_t>();
    for (unsigned int j = 0 ; j < 8; j++) {
      if (((byte >> (7-j)) & 0x01) == 0x01) {
        *pixel = 0xFFFFFFFF;
      } else {
        *pixel = 0x00000000;
      }
      pixel++;
    }
  }

  return sprite;
}

PSprite
DataSourceAmiga::get_game_object_sprite(size_t catalog, size_t index) {
  PBuffer data = get_data_from_catalog(catalog, index, gfxchip);
  if (!data) {
    return nullptr;
  }

  uint8_t h = data->pop<uint8_t>();
  uint8_t offset_y = data->pop<uint8_t>();
  uint8_t w = data->pop<uint8_t>();
  uint8_t offset_x = data->pop<uint8_t>();
  uint8_t filling = data->pop<uint8_t>();
  uint8_t compression = data->pop<uint8_t>();

  if (w < 16) w = 16;

  int width = (w+7)/8;

  PSpriteAmiga mask = decode_mask_sprite(data, width*8, h);
  PSpriteAmiga pixels = decode_planned_sprite(data->pop_tail(), width, h,
                                              compression, filling, palette);

  PSpriteAmiga sprite = pixels->get_amiga_masked(mask);
  if (!sprite) {
    return nullptr;
  }
  sprite->set_delta(1, 0);
  sprite->set_offset(-offset_x, -offset_y);

  return sprite;
}

PSprite
DataSourceAmiga::get_torso_sprite(size_t index, uint8_t *palette) {
  PBuffer data = get_data_from_catalog(19, index, gfxchip);
  if (!data) {
    return nullptr;
  }

  uint8_t offset_x = data->pop<uint8_t>();
  uint8_t w = data->pop<uint8_t>();
  uint8_t offset_y = data->pop<uint8_t>();
  uint8_t h = data->pop<uint8_t>();
  int8_t delta_y = data->pop<int8_t>();
  int8_t delta_x = data->pop<int8_t>();

  if (w < 16) {
    w = 16;
  }
  w = (w+7)/8;

  size_t bps = w*h;

  PSpriteAmiga mask = decode_mask_sprite(data, w*8, h);

  PMutableBuffer buff = std::make_shared<MutableBuffer>(Buffer::EndianessBig);
  buff->push(data->pop(bps*4));
  data->pop(bps);
  buff->push(data->pop(bps));

  PSpriteAmiga pixels = decode_planned_sprite(buff, w, h, 0, 0, palette);

  PSpriteAmiga res = pixels->get_amiga_masked(mask);

  res->set_delta(delta_y, delta_x);
  res->set_offset(-offset_x, -offset_y);

  return res;
}

PSprite
DataSourceAmiga::get_map_object_sprite(size_t index) {
  PBuffer data = get_data_from_catalog(6, index, gfxchip);
  if (!data) {
    return nullptr;
  }

  data->pop<uint16_t>();  // drop shadow_offset
  uint16_t bitplane_size = data->pop<uint16_t>();
  uint8_t width = data->pop<uint8_t>();
  uint8_t height = data->pop<uint8_t>();
  uint8_t offset_y = data->pop<uint8_t>();
  uint8_t compression = data->pop<uint8_t>();

  if (height == 0) {
    return nullptr;
  }
  if (width == 0) {
    width = bitplane_size / height;
  }

  uint8_t compressed = 0;
  uint8_t filled = 0;
  for (int i = 0 ; i < 2 ; i++) {
    compressed  = compressed >> 1;
    filled = filled >> 1;

    if (compression & 0x80) {
      compressed = compressed | 0x10;
      if (compression & 0x40) {
        filled = filled | 0x10;
      }
      compression = compression << 1;
    }
    compression = compression << 1;
  }

  PSpriteAmiga mask = decode_mask_sprite(data, width * 8, height);
  PSpriteAmiga pixels = decode_planned_sprite(data->pop_tail(), width, height,
                                              compressed, filled, palette);

  PSpriteAmiga sprite = pixels->get_amiga_masked(mask);
  sprite->set_delta(1, 0);
  if ((index >= 128) && (index <= 143)) {
    sprite->set_offset(0, -offset_y);
  } else {
    sprite->set_offset(-width * 4, -offset_y);
  }

  return sprite;
}

PSprite
DataSourceAmiga::get_map_object_shadow(size_t index) {
  PBuffer data = get_data_from_catalog(6, index, gfxchip);
  if (!data) {
    return nullptr;
  }

  uint16_t shadow_offset = data->pop<uint16_t>();
  uint16_t bitplane_size = data->pop<uint16_t>();
  uint8_t width = data->pop<uint8_t>();
  uint8_t height = data->pop<uint8_t>();
  data->pop<uint8_t>();  // drop offset_y
  data->pop<uint8_t>();  // drop compression

  if (height == 0) {
    return nullptr;
  }
  if (width == 0) {
    width = bitplane_size / height;
  }

  PBuffer shadow = data->get_tail(shadow_offset);
  int shadow_offset_y = shadow->pop<int8_t>();
  size_t shadow_height = shadow->pop<uint8_t>();
  int shadow_offset_x = shadow->pop<int8_t>();
  size_t shadow_width = shadow->pop<uint8_t>() * 8;

  PSpriteAmiga sprite = std::make_shared<SpriteAmiga>(shadow_width,
                                                      shadow_height);
  decode_mask_sprite(shadow, shadow_width, shadow_height);
  sprite->clear();
  sprite->set_delta(0, 0);
  sprite->set_offset(shadow_offset_x * 8, -shadow_offset_y);

  return sprite;
}

std::vector<size_t> hud_offsets = {
  0,    320, 2, 40,
  320,  320, 2, 40,
  640,  320, 2, 40,
  960,  320, 2, 40,
  1280, 320, 2, 40,
  1600, 320, 2, 40,
  1920, 320, 2, 40,
  2240,  64, 4,  4,
  2304,  64, 4,  4,
  2368,  64, 4,  4,
  2432,  64, 4,  4,
  2496,  64, 4,  4,
  2560,  64, 4,  4,
  2624,  64, 4,  4,
  2688,  64, 4,  4,
  2752,  64, 4,  4,
  2816,  64, 4,  4,
  2880, 800, 5, 40,
  3680,  48, 1, 12,
  3728,  40, 1, 10
};

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::get_hud_sprite(size_t index) {
  PBuffer data = data_pointers[18]->get_tail(hud_offsets[index*4]);

  return decode_interlased_sprite(data, hud_offsets[index * 4 + 2],
                                  hud_offsets[index * 4 + 3], 16, 0, palette2);
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::decode_planned_sprite(PBuffer data, size_t width,
                                       size_t height,
                                       uint8_t compression, uint8_t filling,
                                       uint8_t *palette, bool invert) {
  PSpriteAmiga sprite = std::make_shared<SpriteAmiga>(width*8, height);

  uint8_t *src = reinterpret_cast<uint8_t*>(data->get_data());
  Sprite::Color *res = sprite->get_writable_data();

  size_t bps = width * height;  // bitplane size in bytes

  for (size_t i = 0; i < bps; i++) {
    for (int k = 7; k >= 0; k--) {
      uint8_t color = 0;
      int n = 0;
      for (size_t b = 0; b < 5; b++) {
        color = color << 1;
        if ((compression >> b) & 0x01) {
          if ((filling >> b) & 0x01) {
            color |= 0x01;
          }
        } else {
          color |= ((*(src+(n*bps)) >> k) & 0x01);
          n++;
        }
      }
      if (invert) {
        color = invert5bit(color);
      }
      res->red = palette[color*3+0];    // R
      res->green = palette[color*3+1];  // G
      res->blue = palette[color*3+2];   // B
      res->alpha = 0xFF;                // A
      res++;
    }
    src++;
  }

  return sprite;
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::decode_interlased_sprite(PBuffer data, size_t width,
                                          size_t height,
                                          uint8_t compression,
                                          uint8_t filling,
                                          uint8_t *palette,
                                          size_t skip_lines) {
  PSpriteAmiga sprite = std::make_shared<SpriteAmiga>(width*8, height);

  uint8_t *src = reinterpret_cast<uint8_t*>(data->get_data());
  Sprite::Color *res = sprite->get_writable_data();

  size_t bpp = bitplane_count_from_compression(compression);

  for (size_t y = 0; y < height; y++) {
    for (size_t i = 0; i < width; i++) {
      for (int k = 7; k >= 0; k--) {
        uint8_t color = 0;
        int n = 0;
        for (size_t b = 0; b < 5; b++) {
          color = color << 1;
          if ((compression >> b) & 0x01) {
            if ((filling >> b) & 0x01) {
              color |= 0x01;
            }
          } else {
            color |= ((*(src+(n*width)+((skip_lines*width*y))) >> k) & 0x01);
            n++;
          }
        }
        color = invert5bit(color);
        res->red = palette[color*3+0];    // R
        res->green = palette[color*3+1];  // G
        res->blue = palette[color*3+2];   // B
        res->alpha = 0xFF;                // A
        res++;
      }
      src++;
    }
    src += (bpp-1) * width;
  }

  return sprite;
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::decode_amiga_sprite(PBuffer data, size_t width, size_t height,
                                     uint8_t *palette) {
  PSpriteAmiga sprite = std::make_shared<SpriteAmiga>(width*8, height);

  uint8_t *src_1 = reinterpret_cast<uint8_t*>(data->get_data());
  size_t bp2s = width * 2 * height;
  uint8_t *src_2 = src_1 + bp2s;
  Sprite::Color *res = sprite->get_writable_data();

  for (size_t y = 0; y < height; y++) {
    for (size_t i = 0; i < width; i++) {
      for (int k = 7; k >= 0; k--) {
        uint8_t color = 0;
        color |= (((*src_1) >> k) & 0x01) << 0;
        color |= (((*(src_1 + width)) >> k) & 0x01) << 1;
        color |= (((*src_2) >> k) & 0x01) << 2;
        color |= (((*(src_2 + width)) >> k) & 0x01) << 3;
        color |= 0x10;
        res->red = palette[color*3+0];    // R
        res->green = palette[color*3+1];  // G
        res->blue = palette[color*3+2];   // B
        res->alpha = 0xFF;                // A
        res++;
      }
      src_1++;
      src_2++;
    }
    src_1 += width;
    src_2 += width;
  }

  return sprite;
}

DataSourceAmiga::SpriteAmiga::SpriteAmiga(size_t _width, size_t _height) {
  width = _width;
  height = _height;
  delta_x = 0;
  delta_y = 0;
  offset_x = 0;
  offset_y = 0;
  size_t size = width * height * 4;
  data = new uint8_t[size];
  memset(data, 0, size);
}

DataSourceAmiga::SpriteAmiga::~SpriteAmiga() {
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::SpriteAmiga::get_amiga_masked(PSprite mask) {
  if (mask->get_width() > width) {
    throw ExceptionFreeserf("Failed to apply mask");
  }

  PSpriteAmiga masked = std::make_shared<SpriteAmiga>(mask->get_width(),
                                                      mask->get_height());

  uint32_t *pos = reinterpret_cast<uint32_t*>(masked->get_data());

  uint32_t *s_beg = reinterpret_cast<uint32_t*>(data);
  uint32_t *s_pos = s_beg;
  uint32_t *s_end = s_beg + (width * height);
  size_t s_delta = width - masked->get_width();

  uint32_t *m_pos = reinterpret_cast<uint32_t*>(mask->get_data());

  for (size_t y = 0; y < masked->get_height(); y++) {
    for (size_t x = 0; x < masked->get_width(); x++) {
      if (s_pos >= s_end) {
        s_pos = s_beg;
      }
      *pos++ = *s_pos++ & *m_pos++;
    }
    s_pos += s_delta;
  }

  return masked;
}

void
DataSourceAmiga::SpriteAmiga::make_transparent(uint32_t rc) {
  uint32_t *p = reinterpret_cast<uint32_t*>(data);
  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++) {
      if ((*p & 0x00FFFFFF) == rc) {
        *p = 0x00000000;
      }
      p++;
    }
  }
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::SpriteAmiga::merge_horizontaly(PSpriteAmiga right) {
  if (right->height != height) {
    return nullptr;
  }

  PSpriteAmiga result = std::make_shared<SpriteAmiga>(width + right->width,
                                                      height);

  uint32_t *src_l = reinterpret_cast<uint32_t*>(data);
  uint32_t *src_r = reinterpret_cast<uint32_t*>(right->data);
  uint32_t *res = reinterpret_cast<uint32_t*>(result->data);
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      *res++ = *src_l++;
    }
    for (size_t x = 0; x < right->width; x++) {
      *res++ = *src_r++;
    }
  }

  return result;
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::SpriteAmiga::split_horizontaly(bool return_right) {
  size_t res_width = width/2;
  PSpriteAmiga s = std::make_shared<SpriteAmiga>(res_width, height);
  uint32_t *src = reinterpret_cast<uint32_t*>(data);
  uint32_t *res = reinterpret_cast<uint32_t*>(s->data);
  if (return_right) {
    src += res_width;
  }
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < res_width; x++) {
      *res++ = *src++;
    }
    src += res_width;
  }

  return s;
}

DataSourceAmiga::PSpriteAmiga
DataSourceAmiga::make_shadow_from_symbol(PSpriteAmiga symbol) {
  PSpriteAmiga res = std::make_shared<SpriteAmiga>(10, 10);
  res->stick(symbol, 1, 0);
  res->stick(symbol, 0, 1);
  res->stick(symbol, 2, 1);
  res->stick(symbol, 1, 2);
  res->fill_masked({0xFF, 0xFF, 0xFF, 0xFF});
  return res;
}
