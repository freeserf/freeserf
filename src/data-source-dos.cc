/*
 * data-source-dos.cc - DOS game resources file functions
 *
 * Copyright (C) 2014-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/data-source-dos.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>
#include <bitset>   // TEMP DEBUG, only for printing binary representation of chars
//#include <sstream>  // TEMP DEBUG for printing ascii art
//#include <string>  // TEMP DEBUG for printing ascii art
//#include <iostream>  // TEMP DEBUG for printing ascii art


#include "src/freeserf_endian.h"
#include "src/log.h"
#include "src/tpwm.h"
#include "src/data.h"
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"
#include "src/game-options.h" // for seasons

#define DATA_SERF_ANIMATION_TABLE  2
#define DATA_SERF_ARMS             1850  // 3, dos_sprite_type_transparent
#define DATA_SFX_BASE              3900  // SFX sounds (index 0 is undefined)
#define DATA_MUSIC_GAME            3990  // XMI music
#define DATA_MUSIC_ENDING          3992  // XMI music

// There are different types of sprites:
// - Non-packed, rectangular sprites: These are simple called sprites here.
// - Transparent sprites, "transp": These are e.g. buldings/serfs.
// The transparent regions are RLE encoded.
// - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
// This is used to either modify the alpha level of another sprite (shadows)
// or mask parts of other sprites completely (mask sprites).

// NOTE that this is an array of structs and the
//  the array index maps directly to Data::Resource typedef enum in data.h
DataSourceDOS::Resource dos_resources[] = {
//index | dos_palette | DataSourceDOS::Resource::Sprite type | Data::Resource::Asset type
  {   0, 0,    DataSourceDOS::SpriteTypeUnknown     },  // none
  {   1, 3997, DataSourceDOS::SpriteTypeSolid       },  // art_landscape
  {   2, 0,    DataSourceDOS::SpriteTypeUnknown     },  // animation
  {   4, 3,    DataSourceDOS::SpriteTypeOverlay     },  // serf_shadow
  {   5, 3,    DataSourceDOS::SpriteTypeSolid       },  // dotted_lines
  {  15, 3997, DataSourceDOS::SpriteTypeSolid       },  // art_flag
  {  25, 3,    DataSourceDOS::SpriteTypeSolid       },  // art_box
  {  40, 3998, DataSourceDOS::SpriteTypeSolid       },  // credits_bg
  {  41, 3998, DataSourceDOS::SpriteTypeSolid       },  // logo
  {  42, 3,    DataSourceDOS::SpriteTypeSolid       },  // symbol
  {  60, 3,    DataSourceDOS::SpriteTypeMask        },  // map_mask_up
  { 141, 3,    DataSourceDOS::SpriteTypeMask        },  // map_mask_down
  { 230, 3,    DataSourceDOS::SpriteTypeMask        },  // path_mask
  { 260, 3,    DataSourceDOS::SpriteTypeSolid       },  // map_ground
  { 300, 3,    DataSourceDOS::SpriteTypeSolid       },  // path_ground
  { 321, 3,    DataSourceDOS::SpriteTypeTransparent },  // game_object
  { 600, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_top
  { 610, 3,    DataSourceDOS::SpriteTypeTransparent },  // map_border
  { 630, 3,    DataSourceDOS::SpriteTypeTransparent },  // map_waves
  { 660, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_popup
  { 670, 3,    DataSourceDOS::SpriteTypeSolid       },  // indicator
  { 750, 3,    DataSourceDOS::SpriteTypeTransparent },  // font
  { 810, 3,    DataSourceDOS::SpriteTypeTransparent },  // font_shadow
  { 870, 3,    DataSourceDOS::SpriteTypeSolid       },  // icon
  {1250, 3,    DataSourceDOS::SpriteTypeTransparent },  // map_object
  {1500, 3,    DataSourceDOS::SpriteTypeOverlay     },  // map_shadow
  {1750, 3,    DataSourceDOS::SpriteTypeSolid       },  // panel_button
  {1780, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_bottom
  {2500, 3,    DataSourceDOS::SpriteTypeTransparent },  // serf_torso
  {3150, 3,    DataSourceDOS::SpriteTypeTransparent },  // serf_head
  {3880, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_split
  {3900, 0,    DataSourceDOS::SpriteTypeUnknown     },  // sound
  {3990, 0,    DataSourceDOS::SpriteTypeUnknown     },  // music
  {3999, 3,    DataSourceDOS::SpriteTypeTransparent },  // cursor
  {   3, 0,    DataSourceDOS::SpriteTypeUnknown     }   // palette
};

//int last_original_DOS_sprite_index[] = [-1,0,2,4,5,15,25,40,41,42,60,141,230,260,300,321,600,610,630,660,670,750,810,870,1250,1500,1750,1780,2500,3150,3880,3900,3990,3999];

DataSourceDOS::DataSourceDOS(const std::string &_path)
  : DataSourceLegacy(_path) {
}

DataSourceDOS::~DataSourceDOS() {
}

bool
DataSourceDOS::check() {
  std::vector<std::string> default_file_names = {
    "SPAE.PA",  // English
    "SPAF.PA",  // French
    "SPAD.PA",  // German
    "SPAU.PA"   // Engish (US)
  };

  if (check_file(path)) {
    return true;
  }

  for (std::string file_name : default_file_names) {
    std::string file_path = path + '/' + file_name;
    Log::Info["data"] << "Looking for game data in '" << file_path << "'...";
    if (check_file(file_path)) {
      path = std::move(file_path);
      return true;
    }
  }

  return false;
}

bool
DataSourceDOS::load() {
  if (!check()) {
    return false;
  }

  try {
    spae = std::make_shared<Buffer>(path);
  } catch (...) {
    return false;
  }

  // Check that data file is decompressed
  try {
    UnpackerTPWM unpacker(spae);
    spae = unpacker.convert();
    Log::Verbose["data"] << "Data file is compressed";
  }  catch (...) {
    Log::Verbose["data"] << "Data file is not compressed";
  }

  // Read the number of entries in the index table.
  // Some entries are undefined (size and offset are zero).
  size_t entry_count = spae->pop<uint32_t>();
  entries.push_back({0, 0});  // first entry is whole file itself, drop it
  for (size_t i = 0; i < entry_count; i++) {
    DataEntry entry;
    entry.size = spae->pop<uint32_t>();
    entry.offset = spae->pop<uint32_t>();
    entries.push_back(entry);
  }

  fixup();

  // The first uint32 is the byte length of the rest
  // of the table in big endian order.
  PBuffer anim = get_object(DATA_SERF_ANIMATION_TABLE);
  anim->set_endianess(Buffer::EndianessBig);
  size_t size = anim->get_size();
  if (size != anim->pop<uint32_t>()) {
    Log::Error["data"] << "Could not extract animation table.";
    return false;
  }
  anim = anim->pop_tail();

  return load_animation_table(anim);
}

// Return buffer with object at index
PBuffer
DataSourceDOS::get_object(size_t index) {
  if (index >= entries.size()) {
    return nullptr;
  }

  if (entries[index].offset == 0) {
    return nullptr;
  }
  /*
  #define DATA_SFX_BASE              3900  // SFX sounds (index 0 is undefined)
  #define DATA_MUSIC_GAME            3990  // XMI music
  */
  /* it seems the DOS sound 62 is indeed messed up by Freeserf reader, try using Amiga sound instead
  if (index >= DATA_SFX_BASE && index < DATA_MUSIC_GAME){
    Log::Info["data-source-dos.cc"] << "inside get_object, index " << index << ", offset " << entries[index].offset << ", size " << entries[index].size;
  }
  if (index == 3962){
    Log::Info["data-source-dos.cc"] << "inside get_object, hack smelter sound test";
    //return spae->get_subbuffer(entries[index].offset - 800, 1); // no static
    //return spae->get_subbuffer(entries[index].offset - 799, 1); // faint static
    //return spae->get_subbuffer(entries[index].offset - 800, 2); // faint static
    //return spae->get_subbuffer(entries[index].offset + 7680, 1); // no static
    //return spae->get_subbuffer(entries[index].offset + 7679, 1); // static
    //return spae->get_subbuffer(entries[index].offset + 7680, 3000); // no static -  wrong sound though
    // default
    return spae->get_subbuffer(entries[index].offset, entries[index].size);
  }
  */
  return spae->get_subbuffer(entries[index].offset, entries[index].size);
}

// Perform various fixups of the data file entries
void
DataSourceDOS::fixup() {
  // Fill out some undefined spaces in the index from other
  // places in the data file index.

  for (int i = 0; i < 48; i++) {
    for (int j = 1; j < 6; j++) {
      entries[3450+6*i+j] = entries[3450+6*i];
    }
  }

  for (int i = 0; i < 3; i++) {
    entries[3765+i] = entries[3762+i];
  }

  for (int i = 0; i < 6; i++) {
    entries[1363+i] = entries[1352];
    entries[1613+i] = entries[1602];
  }
}

// Create sprite object
// NOTE that 'index' is the relative index within the Data::Resource type
//  and the base resource type starting index is added in get_sprite_parts
//  when get_object is called to get the "from zero" data object id
Data::MaskImage
//DataSourceDOS::get_sprite_parts(Data::Resource res, size_t index) {
DataSourceDOS::get_sprite_parts(Data::Resource res, size_t index, bool darken) {
  Log::Debug["data-source-dos.cc"] << "inside DataSourceDOS::get_sprite_parts, res type is " << res << ", sprite index is " << index << ", darken bool is " << darken;
  if (index >= Data::get_resource_count(res)) {
    return std::make_tuple(nullptr, nullptr);
  }

  Resource &dos_res = dos_resources[res];

  ColorDOS *palette = get_dos_palette(dos_res.dos_palette);
  
  if (palette == nullptr) {
    return std::make_tuple(nullptr, nullptr);  // return null mask/sprite
  }

  if (res == Data::AssetSerfTorso) {
    PBuffer data = get_object(dos_res.index + index);
    if (!data) {
      return std::make_tuple(nullptr, nullptr);
    }
    Data::PSprite torso = std::make_shared<SpriteDosTransparent>(data, palette, res, dos_res.index + index,
                                                                 64);

    data = get_object(dos_res.index + index);
    if (!data) {
      return std::make_tuple(nullptr, nullptr);
    }
    Data::PSprite torso2 = std::make_shared<SpriteDosTransparent>(data, palette, res, dos_res.index + index,
                                                                  72);

    Data::MaskImage mi = separate_sprites(torso, torso2);

    data = get_object(DATA_SERF_ARMS + index);
    Data::PSprite arms = std::make_shared<SpriteDosTransparent>(data, palette, res, DATA_SERF_ARMS + index);
    torso->stick(arms, 0, 0);

    return mi;  // this returns a mask + sprite
  } else if (res == Data::AssetMapObject) {
      if ((index >= 128) && (index <= 143)) {  // Flag sprites
      int flag_frame = (index - 128) % 4;
      PBuffer data = get_object(dos_res.index + 128 + flag_frame);
      if (!data) {
        return std::make_tuple(nullptr, nullptr);
      }
      Data::PSprite s1 = std::make_shared<SpriteDosTransparent>(data, palette, res, dos_res.index + 128 + flag_frame);
      data = get_object(dos_res.index + 128 + 4 + flag_frame);
      if (!data) {
        return std::make_tuple(nullptr, nullptr);
      }
      Data::PSprite s2 = std::make_shared<SpriteDosTransparent>(data, palette, res, dos_res.index + 128 + 4 + flag_frame);

      return separate_sprites(s1, s2);  // this returns a mask + sprite
    }
  } else if (res == Data::AssetFont || res == Data::AssetFontShadow) {
    PBuffer data = get_object(dos_res.index + index);
    if (!data) {
      return std::make_tuple(nullptr, nullptr);
    }
    return std::make_tuple(std::make_shared<SpriteDosTransparent>(data,
                                                                  palette, res, dos_res.index + index),
                           nullptr);  // this returns a mask, but no sprite??
  }
  //
  // anything below this point is a "non-masked sprite" as far as this function cares
  //   (even if it is a map_mask_up/down sprite that is itself used as a mask when drawing terrain)
  //
  PBuffer data = get_object(dos_res.index + index);
  if (!data) {
    return std::make_tuple(nullptr, nullptr);
  }

  Data::PSprite sprite;
  switch (dos_res.sprite_type) {
    case SpriteTypeSolid: {   
      // terrain sprites and popups, borders
      //   NOTE that terrain sprites are eventually masked into triangles to match terrain slope & shape
      //    by the upstream draw_triangle_XX functions, they are NOT returned in a single tuple like the other
      //    sprites above. 
      //   get_sprite_parts() for a SpriteTypeSolid returns *only* the 32x20px rectangular, un-modified
      //    terrain sprite and a separate call gets the mask sprite and applies it (and that type is SpriteTypeMask, used below)
      //sprite = std::make_shared<SpriteDosSolid>(data, palette);
      //sprite = std::make_shared<SpriteDosSolid>(data, palette, res);  // FourSeasons needs the resource type
      //sprite = std::make_shared<SpriteDosSolid>(data, palette, res, dos_res.index + index);  // FogOfWar needs the resource type AND the sprite index (or some kind of bool I guess)
      Log::Debug["data-source-dos.cc"] << "inside DataSourceDOS::get_sprite_parts, res type is " << res << ", sprite index is " << index << ", darken bool is " << darken << ", about to call SpriteDosSolid constructor";
      sprite = std::make_shared<SpriteDosSolid>(data, palette, res, dos_res.index + index, darken);  // using a bool for FogOfWar, previous way wouldn't work)
      break;
    }
    case SpriteTypeTransparent: {
      // map objects such as buildings, flags, rocks that have transparent *backgrounds*
      //  The foreground is entirely solid (alpha is always 100%)
      //sprite = std::make_shared<SpriteDosTransparent>(data, palette);
      sprite = std::make_shared<SpriteDosTransparent>(data, palette, res, dos_res.index + index);
      break;
    }
    case SpriteTypeOverlay: {
      // *shadows* for map objects and serfs
      sprite = std::make_shared<SpriteDosOverlay>(data, palette, 0x80);
      break;
    }
    case SpriteTypeMask: {
      // mask sprites for terrain triangles (and paths, it seems?)
      // this returns the masks that are used by draw_triangle_up/down functions to
      //  change the rectangular 32x20 terrain sprites into triangles conforming to terrain lines
      //  When drawing terrain, get_sprite_parts is called individually for the mask 
      //  sprite and for the terrain sprite
      sprite = std::make_shared<SpriteDosMask>(data);
      break;
    }
    default:
      // return null mask/sprite
      return std::make_tuple(nullptr, nullptr);
  }

  // this returns a solid sprite, no mask, though the sprite itself might *be a mask* i.e. map_terrain_up/down/paths
  // I am not sure why the masking for terrain tiles is done differently, but it is
  return std::make_tuple(nullptr, sprite);
}

//
// this function appears to control map terrain tiles (not MapObjects such as trees, stones)
//   and game menus, icons, popup backgrounds, etc.
// GameObjects have transparent backgrounds and so use SpriteDosTransparent
// and shadows have partial alpha and so use SpriteDosOverlay
//
//DataSourceDOS::SpriteDosSolid::SpriteDosSolid(PBuffer _data, ColorDOS *palette)
// added passing of resource type to assist with weather/seasons/palette messing
//DataSourceDOS::SpriteDosSolid::SpriteDosSolid(PBuffer _data, ColorDOS *palette, Data::Resource res)
//DataSourceDOS::SpriteDosSolid::SpriteDosSolid(PBuffer _data, ColorDOS *palette, Data::Resource res, size_t sprite_index)
DataSourceDOS::SpriteDosSolid::SpriteDosSolid(PBuffer _data, ColorDOS *palette, Data::Resource res, size_t index, bool darken)
     : SpriteBaseDOS(_data) {
  Log::Debug["data-source-dos.cc"] << "inside DataSourceDOS::SpriteDosSolid::SpriteDosSolid, res type " << res << ", index " << index << ", darken bool is " << darken;
  size_t size = _data->get_size();
  if (size != (width * height + 10)) {
    throw ExceptionFreeserf("Failed to extract DOS solid sprite");
  }

  PMutableBuffer result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);

  // find the average brightness so the brightest/highlight pixels
  //  can be identified
  // don't really need to check the whole sprite, a small sample
  // should be representative for map tiles
  // NOTE - this avg_brightness has NOTHING TO DO WITH FogOfWar.
  //  It is used for identifying high/lowlights within a given sprite
  //  to enhance contrast/manipulate colors
  int avg_brightness = 0;
  if (option_FourSeasons && res == Data::AssetMapGround){
    //while (_data->readable()) {
    for (int i=0; i < 30; i++){

      ColorDOS color = palette[_data->pop<uint8_t>()];
      avg_brightness += color.r + color.g + color.b;
      //Log::Info["data-source-dos"] << "this pixel has brightness " << (color.r + color.g + color.b) / 3;
    }

    // because I do not know how to make a copy of this data, and
    //  the pop function increments this read counter to track its
    //  place in the buffer, I added this reset_read function to try
    //  resetting to zero to hope that it will allow normal iteration
    //  over it again from the usual pixel loading function below
    _data->reset_read();
    // now advance past the metadata at the beginning
    //  which is a total of ten 8-bit segments (8,8,16,16,16,16)
    /*delta_x = _data->pop<int8_t>();
    delta_y = _data->pop<int8_t>();
    width = _data->pop<uint16_t>();
    height = _data->pop<uint16_t>();
    offset_x = _data->pop<int16_t>();
    offset_y = _data->pop<int16_t>();*/
    for (int i=0; i < 10; i++){
      _data->pop<int8_t>();
    }

    avg_brightness = avg_brightness / 90; // 30 samples x3 colors each
    //Log::Info["data-source-dos"] << "the avg_brightness of this map tile is " << avg_brightness;
  }
  
  while (_data->readable()) {
    ColorDOS color = palette[_data->pop<uint8_t>()];

    if (option_FourSeasons && res == Data::AssetMapGround){
      //Log::Info["data-source-dos"] << "ColorDOS old color.b " << std::to_string(color.b) << ", color.g " << std::to_string(color.g) << ", color.r " << std::to_string(color.r);
      if (season == 0){
        // SPRING reverse-fade from WINTER to normal
        // REDUCE IMPACT OF WINTER's slightly reduce reds saturation
        if (color.r > color.g && color.r > color.b    // is red
              && color.r > 50){
          if (subseason == 1){  // fade from winter to spring
            color.r -=  2;
            color.g +=  3;
            color.b +=  3;
          }else if (subseason == 0){  // fade from winter to spring
            color.r -=  5;
            color.g +=  6;
            color.b +=  6;
          }
        }
        // REDUCE IMPACT OF WINTER's reduce greens saturation and shift blue slightly
        else if (color.g > color.r && color.g > color.b) {   // is green
          if ((color.r + color.g + color.b) / 3 > avg_brightness + 2) {    // is bright
            if (subseason == 1){  // fade from winter to spring
              color.r +=  3;
              color.g -= 10;
              color.b +=  8;
            }else if (subseason == 0){  // fade from winter to spring
              color.r +=  6;
              color.g -= 20;
              color.b += 16;
            }
          }else{
            // is not bright
            if (subseason == 1){  // fade from winter to spring
              color.r += 10;
              color.g -=  7;
              color.b += 12;
            }else if (subseason == 0){  // fade from winter to spring
              color.r += 20;
              color.g -= 14;
              color.b += 24;
            }
          }
        }
        // REDUCE IMPACT OF WINTER's slightly reduce blues saturation
        else if (color.b > color.r && color.b > color.g    // is blue
              && color.b > 60){
          if (subseason == 1){  // fade from winter to spring
            color.r +=  3;
            color.g +=  3;
            color.b -= 13;
          }else if (subseason == 0){  // fade from winter to spring
            color.r +=  6;
            color.g +=  6;
            color.b -= 27;
          }
        }
      }

      if (season == 1){
        // SUMMER do nothing
      }

      if (season == 2){
        // FALL reduce saturation of greens and shift highlights yellow to look like long grass
        if (color.g > color.r && color.g > color.b  // is green
            && ((color.r + color.g + color.b) / 3 > avg_brightness + 2)) {    // is bright
          if (subseason == 0){  // fade from summer to fall
            color.g -=  0;
            color.r += 28;
            color.b +=  5;
          } else if (subseason == 1){ // fade from summer to fall
            color.g -=  0;
            color.r += 56;
            color.b += 10;
          } else {
            color.g -=  0;
            color.r += 85;
            color.b += 15;
          }
        }
      }

      if (season == 3){
        // WINTER

        // during fade in only, reverse impact of FALL changes
        // REDUCE IMPACT OF FALL's reduce saturation of greens and shift highlights yellow to look like long grass
        if (color.g > color.r && color.g > color.b  // is green
            && ((color.r + color.g + color.b) / 3 > avg_brightness + 2)) {    // is bright
          if (subseason == 1){  // fade from summer to fall
            color.g -=  0;
            color.r += 28;
            color.b +=  5;
          } else if (subseason == 0){ // fade from summer to fall
            color.g -=  0;
            color.r += 56;
            color.b += 10;
          }
        }

        // slightly reduce reds saturation
        if (color.r > color.g && color.r > color.b    // is red
              && color.r > 50){
          if (subseason == 0){  // fade from fall to winter
            color.r -=  2;
            color.g +=  3;
            color.b +=  3;
          }else if (subseason == 1){  // fade from fall to winter
            color.r -=  5;
            color.g +=  6;
            color.b +=  6;
          }else{
            color.r -=  5;
            color.g += 10;
            color.b += 10;
          }
        }
        // reduce greens saturation and shift blue slightly
        else if (color.g > color.r && color.g > color.b) {   // is green
          if ((color.r + color.g + color.b) / 3 > avg_brightness + 2) {    // is bright
            if (subseason == 0){  // fade from fall to winter
              color.r +=  3;
              color.g -= 10;
              color.b +=  8;
            }else if (subseason == 1){  // fade from fall to winter
              color.r +=  6;
              color.g -= 20;
              color.b += 16;
            }else{
              color.r += 10;
              color.g -= 30;
              color.b += 25;
            }
          }else{
            // is not bright
            if (subseason == 0){  // fade from fall to winter
              color.r += 10;
              color.g -=  7;
              color.b += 12;
            }else if (subseason == 1){  // fade from fall to winter
              color.r += 20;
              color.g -= 14;
              color.b += 24;
            }else{
              color.r += 30;
              color.g -= 20;
              color.b += 35;
            }
          }
        }
        // slightly reduce blues saturation
        else if (color.b > color.r && color.b > color.g    // is blue
              && color.b > 60){
          if (subseason == 0){  // fade from fall to winter
            color.r +=  3;
            color.g +=  3;
            color.b -= 13;
          }else if (subseason == 1){  // fade from fall to winter
            color.r +=  6;
            color.g +=  6;
            color.b -= 27;
          }else{
            color.r += 10;
            color.g += 10;
            color.b -= 40;
          }
        }
      }
     
    } // if option_FourSeasons && AssetMapGround

    // handle FogOfWar
    //  note that FoW dark-ify must run after
    //  FourSeasons so it can modify the FourSeasons-adjusted files
    //   if both enabled
    //if (option_FogOfWar && res == Data::AssetMapGround){
    if (darken){
      //if (sprite_index == 200){
      //  // the not-revealed shrouded state is indicated by sprite index 200, no need for more than one
      //  color.r = 0;  // black
      //  color.g = 0;  // black
      //  color.b = 0;  // black
      //} else if (sprite_index >= 100){
        // the revealed-but-not-currently-visible state is indicated by sprite index 1xx
        color.r = color.r *0.6;  // darker by xx%
        color.g = color.g *0.6;  // darker by xx%
        color.b = color.b *0.6;  // darker by xx%
      //}
      // otherwise, this is a currently-visible sprite, so draw it normally
    }    


    //Log::Info["data-source-dos"] << "ColorDOS new color.b " << std::to_string(color.b) << ", color.g " << std::to_string(color.g) << ", color.r " << std::to_string(color.r);

    result->push<uint8_t>(color.b);  // Blue
    result->push<uint8_t>(color.g);  // Green
    result->push<uint8_t>(color.r);  // Red
    result->push<uint8_t>(0xff);     // Alpha

    /*
    // TEST TEST TEST
    if (option_FourSeasons){
      // making terrain sprites partially-transparent to make them darker sort of works, but where two tiles overlay slightly it is
      //  much lighter and looks bad
      //result->push<uint8_t>(0x80);     // Alpha
      // instead reducing overall brightness via rgb
      /// YES this works very well and is easier than swapping sprites with pre-rendered ones
      result->push<uint8_t>(color.b *0.5);  // Blue
      result->push<uint8_t>(color.g *0.5);  // Green
      result->push<uint8_t>(color.r *0.5);  // Red
      result->push<uint8_t>(0xff);     // Alpha
    }else{
      // normal coloration & solid
      result->push<uint8_t>(color.b);  // Blue
      result->push<uint8_t>(color.g);  // Green
      result->push<uint8_t>(color.r);  // Red
      result->push<uint8_t>(0xff);     // Alpha
    }
  */

  }  // while data readable

  data = reinterpret_cast<uint8_t*>(result->unfix());

} // SpriteDosSolid

//
// this function controls Map Objects such as trees, stones, serfs?, buildings
//  plus the mouse cursor and seemingly any other object that
//  has a transparent aspect
//
/*
DataSourceDOS::SpriteDosTransparent::SpriteDosTransparent(PBuffer _data,
                                                          ColorDOS *palette,
                                                          uint8_t color)
  : SpriteBaseDOS(_data) {
*/
// added passing of resource type to assist with weather/seasons/palette messing
/*
DataSourceDOS::SpriteDosTransparent::SpriteDosTransparent(PBuffer _data,
                                                          ColorDOS *palette,
                                                          Data::Resource res,
                                                          uint8_t color)
  : SpriteBaseDOS(_data) {
*/
// added passing of resource type AND sprite index to also allow manipulating specific sprites
DataSourceDOS::SpriteDosTransparent::SpriteDosTransparent(PBuffer _data,
                                                          ColorDOS *palette,
                                                          Data::Resource res,
                                                          size_t index,
                                                          uint8_t color)
  : SpriteBaseDOS(_data) {

  PMutableBuffer result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);

  //Log::Info["data-source-dos"] << "this transparant sprite has size " << _data->get_size();

  while (_data->readable()) {
    size_t drop = _data->pop<uint8_t>();
    result->push<uint32_t>(0x00000000, drop);

    size_t fill = _data->pop<uint8_t>();
    for (size_t i = 0; i < fill; i++) {
      unsigned int p_index = _data->pop<uint8_t>() + color;  // color_off;

      ColorDOS color = palette[p_index];

      /*
      // FALL
      if (res == Data::AssetMapObject){
        // size 500-510 should only be deciduous trees
        //  CHANGE THIS TO PASS THE object type and add
        //  match for falling/felled trees also
        //  maybe just switch to custom sprites instead of palette shifting??  vv
        if (_data->get_size() > 500 && _data->get_size() < 510){
          if (color.g > color.r && color.g > color.b){  // is green
            if (color.g + color.r + color.b > 158){ // is bright
              color.r += color.g - 20;  // make yellow
            }else{
              color.r += 20;
              color.g += 10;
            }         
          }
        }
      }
      */

      // testing FourSeasons manipulating Seeds / Field sprites
      //  THESE ARE BEING CACHED EVEN IF option_FourSeasons IS TURNED OFF!!!
      /// NEED TO FIGURE OUT HOW TO FLUSH THE CACHE!
      if (option_FourSeasons && season == 3){
        if (res == Data::AssetMapObject){
          // I haven't bothered to check how the sprite indexes are numbered, there must be some offset
          //  for each data type, instead I found through trial and error, only need one as a reference point
          int base = 1241;
          //if (index >= base + 105 && index <= base + 111){      // ObjectSeeds0 - ObjectFieldExpired 
          if ((index >= base + 105 && index <= base + 111)    // ObjectSeeds0 - ObjectFieldExpired 
           || (index >= base + 121 && index <= base + 121)){  // ObjectField0 - ObjectField0
            if (color.r < 200){  // is *NOT* red (i.e., is yellow or green, or blue I guess)
            //if (color.g > color.r && color.g > color.b){  // is green
              //if (color.g + color.r + color.b > 158){ // is bright
                // make more gray
                int tmpr = color.r + 50; if (tmpr < 1){ tmpr = 0;} if (tmpr >255){ tmpr = 255;} color.r=tmpr;
                int tmpg = color.g - 45; if (tmpg < 1){ tmpg = 0;} if (tmpg >255){ tmpg = 255;} color.g=tmpg;
                int tmpb = color.b +  0; if (tmpb < 1){ tmpb = 0;} if (tmpb >255){ tmpb = 255;} color.b=tmpb;
                // make black
                //color.r = 0;
                //color.g = 0;
                //color.b = 0;
            //  }         
            }
          }
        }
      }


      result->push<uint8_t>(color.b);  // Blue
      result->push<uint8_t>(color.g);  // Green
      result->push<uint8_t>(color.r);  // Red
      result->push<uint8_t>(0xFF);     // Alpha
    }
  }

  data = reinterpret_cast<uint8_t*>(result->unfix());
}

// note that the image metadata such as height and width are set
//  as part of SpriteBaseDOS(_data)
DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay(PBuffer _data,
                                                  ColorDOS *palette,
                                                  unsigned char value)
  : SpriteBaseDOS(_data) {  // metadata loaded here
  
  //Log::Debug["data-source-dos.cc"] << "start of DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay with dimensions h" << get_height() << "w" << get_width();
  //int bytes=0;
  //std::vector<const char*> asciiart = {};

  PMutableBuffer result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);

  while (_data->readable()) {
    size_t drop = _data->pop<uint8_t>();
    //bytes++;
    //Log::Debug["data-source-dos.cc"] << "inside DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay, this alpha sprite has drop " << drop;
    result->push<uint32_t>(0x00000000, drop);
    //for (size_t i = 0; i < drop; i++){
    //  ColorDOS color = palette[value];
    //  result->push<uint8_t>(color.b);  // Blue
    //  result->push<uint8_t>(color.g);  // Green
    //  result->push<uint8_t>(color.r);  // Red
    //  result->push<uint8_t>(value);    // Alpha  // ALL ALPHA (this works)
    //  asciiart.push_back("#");
    //}
    size_t fill = _data->pop<uint8_t>();
    for (size_t i = 0; i < fill; i++) {
      //Log::Debug["data-source-dos.cc"] << "inside DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay, this alpha sprite has fill " << fill;
      ColorDOS color = palette[value];
      result->push<uint8_t>(color.b);  // Blue
      result->push<uint8_t>(color.g);  // Green
      result->push<uint8_t>(color.r);  // Red
      result->push<uint8_t>(value);    // Alpha
      //asciiart.push_back("_");
      // this seems to only contain alpha as nonzero (big-endian 1, 10000000, colors.r/g/b are zero 00000000
      //Log::Debug["data-source-dos.cc"] << "inside DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay, this alpha sprite has rgba " << std::bitset<8>(color.r) << ","<< std::bitset<8>(color.g) << "," << std::bitset<8>(color.b) << "," << std::bitset<8>(value);
    }
  }

/*
  // this works great!  draws the alpha layer/image in the console log
  std::vector<const char*>::iterator it = asciiart.begin();
  while (it != asciiart.end()){
    std::stringstream osf;
    for (size_t c=0;c < get_width();c++){
      osf << *it;
      it++;
    }
    Log::Debug["data-source-dos.cc"] << "ASCIIART " << osf.str();
  }
*/

  data = reinterpret_cast<uint8_t*>(result->unfix());
  //Log::Debug["data-source-dos.cc"] << "done DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay, data has " << bytes << " bytes";
}

DataSourceDOS::SpriteDosMask::SpriteDosMask(PBuffer _data)
  : SpriteBaseDOS(_data) {
  PMutableBuffer result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);

  while (_data->readable()) {
    size_t drop = _data->pop<uint8_t>();
    result->push<uint32_t>(0x00000000, drop);

    size_t fill = _data->pop<uint8_t>();
    result->push<uint32_t>(0xFFFFFFFF, fill);
  }

  data = reinterpret_cast<uint8_t*>(result->unfix());
}


DataSourceDOS::SpriteBaseDOS::SpriteBaseDOS(PBuffer _data) {
  if (_data->get_size() < 10) {
    throw ExceptionFreeserf("Failed to extract DOS sprite");
  }

  delta_x = _data->pop<int8_t>();
  delta_y = _data->pop<int8_t>();
  width = _data->pop<uint16_t>();
  height = _data->pop<uint16_t>();
  offset_x = _data->pop<int16_t>();
  offset_y = _data->pop<int16_t>();
}

PBuffer
DataSourceDOS::get_sound(size_t index) {
  PBuffer data = get_object(DATA_SFX_BASE + index);
  //Log::Info["data-source-dos.cc"] << "inside get_sound, index " << index;
  if (!data) {
    Log::Error["data"] << "Could not extract SFX clip: #" << index;
    return nullptr;
  }

  try {
    ConvertorSFX2WAV convertor(data, -32);
    return convertor.convert();
  } catch (...) {
    Log::Error["data"] << "Could not convert SFX clip to WAV: #" << index;
    return nullptr;
  }
}

PBuffer
DataSourceDOS::get_music(size_t index) {
  Log::Info["data-source-dos.cc"] << "inside get_music, trying to get index " << index;
  PBuffer data = get_object(DATA_MUSIC_GAME + index);
  if (!data) {
    Log::Error["data"] << "Could not extract XMI clip: #" << index;
    return nullptr;
  }

  try {
    ConvertorXMI2MID convertor(data);
    Log::Info["data-source-dos.cc"] << "inside get_music, successfully got music with index " << index;
    return convertor.convert();
  } catch (...) {
    Log::Error["data"] << "Could not convert XMI clip to MID: #" << index;
    return nullptr;
  }
}

DataSourceDOS::ColorDOS *
DataSourceDOS::get_dos_palette(size_t index) {
  PBuffer data = get_object(index);
  if (!data || (data->get_size() != sizeof(ColorDOS)*256)) {
    return nullptr;
  }

  return reinterpret_cast<ColorDOS*>(data->get_data());
}
