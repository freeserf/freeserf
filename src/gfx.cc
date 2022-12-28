/*
 * gfx.cc - General graphics and data file functions
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/gfx.h"

#include <utility>
#include <algorithm>

#include "src/log.h"
#include "src/data.h"
#include "src/video.h"
#include "src/game-options.h"
//#include "src/lookup.h"

const Color Color::black = Color(0x00, 0x00, 0x00);
const Color Color::white = Color(0xff, 0xff, 0xff);
const Color Color::green = Color(0x73, 0xb3, 0x43);
const Color Color::transparent = Color(0x00, 0x00, 0x00, 0x00);
// added by me
const Color Color::red     = Color(207,  99,  99);
const Color Color::yellow  = Color(239, 239, 143);
const Color Color::gold    = Color(255, 255,  55);
const Color Color::dk_gray = Color( 35,  35,  35);
const Color Color::blue    = Color( 44,  44, 255);
const Color Color::lt_gray = Color(188, 188, 188);
const Color Color::cyan    = Color(  0, 227, 227);
const Color Color::magenta = Color(223, 127, 239);

/* original player colors:
  {0x00, 0xe3, 0xe3},  // cyan   player0
  {0xcf, 0x63, 0x63},  // red    player1
  {0xdf, 0x7f, 0xef},  // pink   player2
  {0xef, 0xef, 0x8f},  // yellow player3
*/

// I am not sure why these get_cyan/magenta/etc functions exist
//  when they could just be hardcoded(?)

double
Color::get_cyan() const {
  double k = get_key();
  return (1. - (static_cast<double>(r)/255.) -k ) / (1. - k);
}

double
Color::get_magenta() const {
  double k = get_key();
  return (1. - (static_cast<double>(g)/255.) -k ) / (1. - k);
}

double
Color::get_yellow() const {
  double k = get_key();
  return (1. - (static_cast<double>(b)/255.) -k ) / (1. - k);
}

double
Color::get_key() const {
  return 1. - std::max(static_cast<double>(r)/255.,
                       std::max(static_cast<double>(g)/255.,
                       static_cast<double>(b)/255.));
}


// this -1 values of col3 of the data_resources[] table in data.cc, used to identify when a sprite/(sound?) is out of normal
//  range, indicating it should be handled as a mutated or custom sprite/(sound?) instead
const int last_original_data_index[34] = {-1,0,199,0,6,6,13,0,0,15,80,80,26,32,9,278,3,9,15,3,7,43,43,317,193,193,24,25,540,629,2,89,6,0};

ExceptionGFX::ExceptionGFX(const std::string &description)
  : ExceptionFreeserf(description) {
}

ExceptionGFX::~ExceptionGFX() {
}

Image::Image(Video *_video, Data::PSprite sprite) {
  video = _video;
  width = static_cast<unsigned int>(sprite->get_width());
  height = static_cast<unsigned int>(sprite->get_height());
  offset_x = sprite->get_offset_x();
  offset_y = sprite->get_offset_y();
  delta_x = sprite->get_delta_x();
  delta_y = sprite->get_delta_y();
  video_image = video->create_image(sprite->get_data(), width, height);
}

Image::~Image() {
  if (video_image != nullptr) {
    video->destroy_image(video_image);
    video_image = nullptr;
  }
  video = nullptr;
}

/* Sprite cache hash table */
Image::ImageCache Image::image_cache;

void
Image::cache_image(uint64_t id, Image *image) {
  image_cache[id] = image;
}

/* Return a pointer to the sprite pointer associated with id. */
Image *
Image::get_cached_image(uint64_t id) {
  ImageCache::iterator result = image_cache.find(id);
  if (result == image_cache.end()) {
    return nullptr;
  }
  return result->second;
}

void
Image::clear_cache() {
  while (!image_cache.empty()) {
    Image *image = image_cache.begin()->second;
    image_cache.erase(image_cache.begin());
    delete image;
  }
}

// added to support messing with weather/seasons/palette
//uint64_t id = Data::Sprite::create_id(res, index, 0, 0, pc);
void
Image::clear_cache_items(std::set<uint64_t> image_ids_to_purge) {
  for (uint64_t id : image_ids_to_purge){
    //Log::Debug["gfx"] << "inside Image::clear_cache_items, erasing id " << id;
    image_cache.erase(id);
  }
}

Graphics *Graphics::instance = nullptr;

Graphics::Graphics() {
  if (instance != nullptr) {
    throw ExceptionGFX("Unable to create second instance.");
  }

  try {
    video = &Video::get_instance();
  } catch (ExceptionVideo &e) {
    throw ExceptionGFX(e.what());
  }

  Data &data = Data::get_instance();
  Data::PSource data_source = data.get_data_source();
  Data::PSprite sprite = data_source->get_sprite(Data::AssetCursor, 0,
                                                 {0, 0, 0, 0});
  video->set_cursor(sprite->get_data(),
                    static_cast<unsigned int>(sprite->get_width()),
                    static_cast<unsigned int>(sprite->get_height()));

  Graphics::instance = this;
}

Graphics::~Graphics() {
  Image::clear_cache();
}

Graphics &
Graphics::get_instance() {
  static Graphics graphics;
  return graphics;
}

//
// EXPLANATION OF CUSTOM GRAPHICS:
//
//  - terrain "appearance swap" is done by changing the drawn original tile
//      to another type, without changing the underlying type in terms of gameplay
//     This effect is done totally within draw_triangle_up/down by swapping the
//      sprite type to be drawn and not actually modifying any sprites
//     But, because the terrain is rendered and acched by frame tile cache it
//      must be flushed when these swaps happen
//     Example:
//        FourSeasons changes snow level appearance on mountains by changing some tiles between Tundra and Snow
//
//  - new sprites for MapObjects (SpriteTransparent)
//     These are loaded from custom PNG files included in the game build 
//     These replace the the original sprites (when conditions are met)
//      and are set in place by logic in draw_map_objects_row that swaps the original
//      sprite indexes with the custom indexes.
//     These result in out-of-original-range sprite indexes
//      and will try to fall back to the original sprite if not found
//     Example:
//        Trees & their shadows for FourSeasons fall/winter/spring tree graphics
//          NOTE custom tree graphics use "crammed animation", see elsewhere in Forkserf code for explanation
//        The FourSeasons 'season dial' that shows summer/fall/winter/spring icon changing each season
//
//  - mutated original sprites for MapObjects (SpriteTransparent)
//     These replace original sprites with modified rgb color values as they are (re)loaded.
//     These take the orignal sprite index so the original sprite becomes unreachable.
//     The mutation is handled inside the SpriteDosTransparent constructor and not applied for Amiga
//     Example:
//        Seeds/immature wheat Fields have reduced green to look better with FourSeasons winter terrain
//
//  - mutated original sprites for MapGround (SpriteSolid) that are made available
//     in addition to original sprites, with both available at same time.  These are set in place
//     by logic in the draw_triangle_up/down functions and result in out-of-original-range sprite indexes
//     Example:
//        FogOfWar black shroud is just a black sprite the same 32x20 size as normal terrain sprites, for not-revealed po
//        FogOfWar darker versions of normal terrain sprites (type+luminosity) for revealed-but-not-currently-visible pos
//
// BOTH terrain and map objects are cached, in two different ways:
//
//   IN THE ORIGINAL CODE:
//    - map objects (and any other sprites) are cached upon first load, and never reloaded unless explicitly flushed
//    - terrain components: map_ground sprites (rectangular) and their map_mask_up/down masks are also cached like any other sprite
//    - terrain is rendered into triangles from terrain-sprite + mask and stored in 16x16 blocks called tile_frames or tile cache or frame cache?
//    - the tile frames are flushed every time certain events happen, at the very least when popups are closed (not sure why) and possibly other times
//
//   FOR CUSTOM GRAPHICS:
//    - cached sprites must be flushed explicitly to force an update, including terrain, if the sprite id is the same
//    - rendered terrain (tile frame cache) 16x16 blocks must be flushed when changing terrain 
//        using the various tricks such as game->set_redraw_XXX and viewport->resize
//     
//
// NOTE that 'index' is the relative index within the Data::Resource type
//  and the base resource type starting index is added in get_sprite_parts
//  when get_object is called to get the "from zero" data object id
//
//
// FUNCTION FLOW for default DOS sprites
//
// # terrain #
//  Viewport::internal_draw() called during each SDL Timer based loop
//    Viewport::draw_landscape()
//      Viewport::get_tile_frame(frame tile id)
//      | Viewport::draw_up/down_tile_col(map pos of start of row) if 16x16 rendered terrain frame not found in landscape_tiles cache
//      | | Viewport::draw_triangle_up/down(map pos)
//      | | | Frame::draw_masked_sprite(res type + relative sprite index)
//      | | | | DataSourceBase::get_sprite(res type + relative sprite index) for map_ground           if image[id] not found in cache (which contains final sprite w/mask applied)
//      | | | | DataSourceBase::get_sprite(res type + relative sprite index) for map_mask_up/down     if image[id] not found in cache (which contains final sprite w/mask applied)
//      | | | | | DataSourceDOS::get_sprite_parts(res type + relative sprite index)
//      | | | | |   DataSourceDos::get_object(absolute datafile index) from SPAx.PA        <-- this must be a real sprite when it reaches this point!  no fake sprite ids
//      | | | | |     DataSourceDos::SpriteDosSolid constructor
//      | | | | |       this reads the actual rgba value and returns as a Data::PSprite    <-- can mutate here during image load
//      | | | | applies mask to base terrain sprite and renders it on the Video Frame object using VideoSDL::draw_image
//      | | | | caches the image using "res type + relative sprite index" as key
//      | | draws the finalized sprite onto the 16x16 tile frame
//      | draws the column of terrain triangles
//      caches the 16x16 tile frame
//            
//              
//
//
void
Frame::draw_sprite(int x, int y, Data::Resource res, unsigned int index, bool use_off, const Color &color, float progress, int mutate) {
  //Log::Debug["gfx.cc"] << "inside Frame::draw_sprite  with res " << res << " and index " << index << ", mutate int is " << mutate;
  Data::Sprite::Color pc = {color.get_blue(),
                            color.get_green(),
                            color.get_red(),
                            color.get_alpha()};
  
  // for mutate versions of original game sprites when on downward slopes (left->right)
  //  The mutated terrain sprites must be cached with alernate sprite indexes
  //   to allow both the fully-visible and revealed-but-not-currently-visible
  //   sprites to be drawn at the same time
  //  To support this, fake the sprite index for the mutated sprites by using +XX
  //   so that they are cached and retrieved with the higher index but the original
  //   sprite index is actually passed to the downstream functions to retrieve from
  //   the SPAx.PA data file, as the mutated sprites don't actually exist anywhere,
  //   they are built by mutating the original sprite during loading
  uint64_t id; // image cache key
  if (mutate & 1){
    // fake high sprite indexes to allow caching both original and mutated
    if (res == Data::AssetMapObject){
      // this is pretty arbitrary
      id = Data::Sprite::create_id(res, index + 3000, 0, 0, pc);
      //Log::Debug["gfx.cc"] << "inside Frame::draw_sprite  with res " << res << " and index " << index << ", mutate int is " << mutate << ", caching with fake high id " << index + 3000;
    } else{
      throw ExceptionFreeserf("inside Frame::draw_sprite, unexpected Data::Asset type to mutate!");      
    }
  }else{
    // original sprite index
    id = Data::Sprite::create_id(res, index, 0, 0, pc);
  }

  Image *image = Image::get_cached_image(id);
  
  // if image not found already cached, fetch it and cache it
  if (image == nullptr) {
    Data::PSprite s;

    // handle special sprites, either mutated-originals or totally new Custom sprites
    //  new custom sprites will work for Amiga, but mutated ones will not as the
    //  mutation happens within the DOS data loading functions
    if (index > last_original_data_index[res]){
      Log::Debug["gfx.cc"] << "inside Frame::draw_sprite, sprite index " << index << " is higher than the last_original_data_index " << last_original_data_index[res] << " for this Data::Resource type " << res << ", assuming it is a special sprite";

      unsigned int orig_index = -1;

      if (res == Data::AssetFrameBottom // four seasons season dial in panelbar
       || res == Data::AssetMapObject  // new custom trees, flowers
       || res == Data::AssetMapShadow  // shadows for new custom trees
       || res == Data::AssetIcon   // for game-init icons
       || res == Data::AssetMapWaves){  // for splashes on water objects
        // these types, if having beyond-original indexes, are new graphics
        //  loaded from actual PN  files using the data_source_Custom
        Log::Debug["gfx.cc"] << "inside Frame::draw_sprite, sprite index " << index << " trying to load custom data source";
        Data &data = Data::get_instance();
        if (data.get_data_source_Custom() == nullptr){
          // try load the custom sprite
          Log::Warn["gfx.cc"] << "inside Frame::draw_sprite, custom datasource not available at all, cannot even attempt to load sprite index " << index << ", trying to fall back to default datasource for this sprite, using " << orig_index;
          s = data_source->get_sprite(res, orig_index, pc);
        }else{
          Log::Debug["gfx.cc"] << "inside Frame::draw_sprite, sprite index " << index << " about to call data_source_Custom->get_sprite";
          s = data.get_data_source_Custom()->get_sprite(res, index, pc);
          if (s == nullptr){
            // try falling back to original sprite if custom sprite couldn't be loaded
            if (res == Data::AssetFrameBottom){
              orig_index = 6;  // this is the original sprite that the seasons dial replaces
            }else{
              // stripping the first two digits results in 0-7 which hold the original Trees & Tree-shadows
              //  and the original frame_bottom sprite that the seasons dial replaces is #6, which will also work
              // NOTE - if sprites >#9 are to be modified in the future, this orig_index fallback logic must be modified
              orig_index = index % 10;
            }
            Log::Warn["gfx.cc"] << "inside Frame::draw_sprite, custom datasource not found for res type " << res << ", sprite index " << index <<", trying to fall back to default datasource for this sprite, using " << orig_index;
            s = data_source->get_sprite(res, orig_index, pc);
          //}else{
            //Log::Debug["gfx.cc"] << "inside Frame::draw_sprite, custom datasource successfully loaded for res type " << res << ", sprite index " << index;
          }
        }
      }else if (res == Data::AssetMapGround){
        //
        //  REMOVE THIS WHOLE SECTION, I think
        //
        Log::Debug["gfx.cc"] << "inside Frame::draw_sprite, sprite index " << index << "terrain sprite, nothing to do here";
        // for option_FogOfWar
        //  call get_sprite for the BASE terrain sprite index to be mutated
        //
        // logic inside the downstream SpriteDosSolid/Transparent/Overlay constructor
        //   will apply necessary mutation when it sees mutate int set
        //
        // this mutated sprite will be cached with its new higher-than-original index
        //  so it will be loaded from cache on subsequent draw_sprite calls without
        //  overwriting the original sprite, so both could be drawn at once
        //
        // this is not a typo, updating the 'index' to be the base terrain 
        //index = index % 100; // stripping the first digit results in 0-32 which hold the original map_ground terrain sprites
      }else{
        // throw exception
        Log::Error["gfx.cc"] << "inside Frame::draw_sprite, index " << index << " is higher than the last_original_data_index " << last_original_data_index[res] << " for this Data::Resource type " << res << ", but no matching custom rule found, crashing!";
        throw ExceptionFreeserf("inside Frame::draw_sprite, no matching custom sprite rule found for beyond-original-range sprite index");
      }
    }else{
      // normal sprite, unmodified from original DOS or Amiga data source
      //   ~~~~ OR ~~~~
      // mutated sprite that directly replaces the original at the same sprite index
      //  by logic inside the downstream SpriteDosSolid/Transparent/Overlay constructor
      //  This means both original and new cannot both be drawn in same frame
      //
      //    - option_FourSeasons uses this for seasonal changes to terrain
      //
      //s = data_source->get_sprite(res, index, pc);
      //Log::Debug["gfx.cc"] << "inside Frame::draw_sprite, using original data source, index " << index << ", mutate int is " << mutate;
      s = data_source->get_sprite(res, index, pc, mutate);
    } // if index beyond original range

    if (!s) {
      Log::Warn["gfx.cc"] << "inside Frame::draw_sprite, Failed to decode sprite #"
                            << Data::get_resource_name(res) << ":" << index;
      return;
    }
    image = new Image(video, s);
    Image::cache_image(id, image);
  } // if image not in cache

  if (use_off) {
    x += image->get_offset_x();
    y += image->get_offset_y();
  }
  int y_off = image->get_height() - static_cast<int>(image->get_height() *
                                                     progress);
  video->draw_image(image->get_video_image(), x, y, y_off, video_frame);
}

void
Frame::draw_sprite_relatively(int x, int y, Data::Resource res,
                              unsigned int index,
                              Data::Resource relative_to_res,
                              unsigned int relative_to_index) {
  Data::PSprite s = data_source->get_sprite(relative_to_res, relative_to_index,
                                            {0, 0, 0, 0});
  //Log::Info["gfx"] << "inside Frame::draw_sprite_relatively  with res " << res << " and index " << index;
  if (s == nullptr) {
    Log::Warn["graphics"] << "Failed to decode sprite #"
                          << Data::get_resource_name(res) << ":" << index;
    return;
  }

  x += s->get_delta_x();
  y += s->get_delta_y();

  draw_sprite(x, y, res, index, true, Color::transparent, 1.f);
}

//
// this function only seems to be used for MapGround type sprites - Terrain
//
/* Draw the masked sprite with given mask and sprite
   indices at x, y in dest frame. */
void
Frame::draw_masked_sprite(int x, int y, Data::Resource mask_res,
                          unsigned int mask_index, Data::Resource res,
                          unsigned int index, int mutate) {
  //Log::Debug["gfx.cc"] << "inside Frame::draw_masked_sprite  with res " << res << ", index " << index << ", mutate " << mutate;

  // for option_FogOfWar
  //  The mutated terrain sprites must be cached with alernate sprite indexes
  //   to allow both the fully-visible and revealed-but-not-currently-visible
  //   sprites to be drawn at the same time
  //  To support this, fake the sprite index for the mutated sprites by using +100
  //   so that they are cached and retrieved with the higher index but the original
  //   sprite index is actually passed to the downstream functions to retrieve from
  //   the SPAx.PA data file, as the mutated sprites don't actually exist anywhere,
  //   they are built by mutating the original sprite during loading
  uint64_t id; // image cache key
  int special_offset = 0;
  int real_index = index;

  if (mutate > 0){
    if (res != Data::AssetMapGround){
      throw ExceptionFreeserf("unexpected Data::Asset type to mutate!");      
    }
  }

  if (mutate & 1){  // odd numbered mutate indicates this is a darkened tile
    // fake high sprite indexes to allow caching both original and mutated
    // orig terrain types are all under 100, + 100
    special_offset = 100;
  }

  if (index >= 40 && index <= 42){  // special water luminosity by depth
    //Log::Debug["gfx.cc"] << "inside Frame::draw_masked_sprite  with res " << res << ", mutate is >1";

    // create different luminosity/colors for Water0 through Water3
    if (index == 40){
      // Water0
      real_index = 32;
      mutate += 10;
    }
    //
    // NOTE Water1 is unchanged
    //
      else if (index == 41){
      // Water2
      real_index = 32;
      mutate += 12;
    }else if (index == 42){
      // Water3
      real_index = 32;
      mutate += 14;
    }
  }

  id = Data::Sprite::create_id(res, index + special_offset, mask_res, mask_index, {0, 0, 0, 0});
  
  // see if finalized sprite w/ mask applied already in cache  
  Image *image = Image::get_cached_image(id);

  // if not, get the sprite and its mask sprite and apply the mask
  if (image == nullptr) {
    Data::PSprite s;
    //
    // fetch the base sprite, MASK IS NOT APPLIED YET
    //   mutate if specified
    s = data_source->get_sprite(res, real_index, {0, 0, 0, 0}, mutate);
    
    if (!s) {
      Log::Warn["graphics"] << "Failed to decode sprite #"
                            << Data::get_resource_name(res) << ":" << real_index;
      return;
    }

    //
    // fetch the mask sprite as if it were a normal sprite
    //
    // mutate int is false is default, but listing explicitly here to avoid confusion
    //   The mask is not modified, only the base terrain sprite is
    Data::PSprite m = data_source->get_sprite(mask_res, mask_index,{0, 0, 0, 0}, mutate = 0);
                                              
    if (!m) {
      Log::Warn["graphics"] << "Failed to decode sprite #"
                            << Data::get_resource_name(mask_res)
                            << ":" << mask_index;
      return;
    }

    Data::PSprite masked = s->get_masked(m);
    if (!masked) {
      Log::Warn["graphics"] << "Failed to apply mask #"
                            << Data::get_resource_name(mask_res)
                            << ":" << mask_index
                            << " to sprite #"
                            << Data::get_resource_name(res) << ":" << real_index;
      return;
    }

    s = std::move(masked);

    image = new Image(video, s);
    Image::cache_image(id, image);
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw the waves sprite with given mask and sprite
   indices at x, y in dest frame. */
   // why is this its own function?
void
Frame::draw_waves_sprite(int x, int y, Data::Resource mask_res,
                         unsigned int mask_index, Data::Resource res,
                         unsigned int index) {
  uint64_t id = Data::Sprite::create_id(res, index, mask_res, mask_index,
                                        {0, 0, 0, 0});
  Image *image = Image::get_cached_image(id);
  if (image == nullptr) {
    Data::PSprite s = data_source->get_sprite(res, index, {0, 0, 0, 0});
    if (!s) {
      Log::Warn["graphics"] << "Failed to decode sprite #"
                            << Data::get_resource_name(res) << ":" << index;
      return;
    }

    if (mask_res > 0) {
      Data::PSprite m = data_source->get_sprite(mask_res, mask_index,
                                                {0, 0, 0, 0});
      if (!m) {
        Log::Warn["graphics"] << "Failed to decode sprite #"
                              << Data::get_resource_name(mask_res)
                              << ":" << mask_index;
        return;
      }

      Data::PSprite masked = s->get_masked(m);
      if (!masked) {
        Log::Warn["graphics"] << "Failed to apply mask #"
                              << Data::get_resource_name(mask_res)
                              << ":" << mask_index
                              << " to sprite #"
                              << Data::get_resource_name(res) << ":" << index;
        return;
      }

      s = std::move(masked);
    }

    image = new Image(video, s);
    Image::cache_image(id, image);
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw a character at x, y in the dest frame. */
void
Frame::draw_char_sprite(int x, int y, unsigned char c, const Color &color,
                        const Color &shadow) {
  static const int sprite_offset_from_ascii[] = {
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 43, -1, -1,
    -1, -1, -1, -1, -1, 40, 39, -1,
    29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 41, -1, -1, -1, -1, 42,
    -1,  0,  1,  2,  3,  4,  5,  6,
     7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,
     7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, -1, -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
  };

  int s = sprite_offset_from_ascii[c];
  if (s < 0) return;

  if (shadow != Color::transparent) {
    draw_sprite(x, y, Data::AssetFontShadow, s, false, shadow);
  }
  draw_sprite(x, y, Data::AssetFont, s, false, color);
}

/* Draw the string str at x, y in the dest frame. */
void
Frame::draw_string(int x, int y, const std::string &str, const Color &color,
                   const Color &shadow) {
  int cx = x;

  for (char c : str) {
    if (c == '\t') {
      cx += 8 * 2;
    } else if (c == '\n') {
      y += 8;
      cx = x;
    } else {
      draw_char_sprite(cx, y, c, color, shadow);
      cx += 8;
    }
  }
}

/* Draw the number n at x, y in the dest frame. */
void
Frame::draw_number(int x, int y, int value, const Color &color,
                   const Color &shadow) {
  if (value < 0) {
    draw_char_sprite(x, y, '-', color, shadow);
    x += 8;
    value *= -1;
  }

  if (value == 0) {
    draw_char_sprite(x, y, '0', color, shadow);
    return;
  }

  int digits = 0;
  for (int i = value; i > 0; i /= 10) digits += 1;

  for (int i = digits-1; i >= 0; i--) {
    draw_char_sprite(x+8*i, y, '0'+(value % 10), color, shadow);
    value /= 10;
  }
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
Frame::draw_rect(int x, int y, int width, int height, const Color &color) {
  Video::Color c = { color.get_red(),
                     color.get_green(),
                     color.get_blue(),
                     color.get_alpha() };
  video->draw_rect(x, y, width, height, c, video_frame);
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
Frame::fill_rect(int x, int y, int width, int height, const Color &color) {
  Video::Color c = { color.get_red(),
                     color.get_green(),
                     color.get_blue(),
                     color.get_alpha() };
  video->fill_rect(x, y, width, height, c, video_frame);
}

/* Initialize new graphics frame. If dest is NULL a new
   backing surface is created, otherwise the same surface
   as dest is used. */
Frame::Frame(Video *video_, unsigned int width, unsigned int height) {
  video = video_;
  video_frame = video->create_frame(width, height);
  owner = true;
  data_source = Data::get_instance().get_data_source();
}

Frame::Frame(Video *video_, Video::Frame *video_frame_) {
  video = video_;
  video_frame = video_frame_;
  owner = false;
  data_source = Data::get_instance().get_data_source();
}

/* Deinitialize frame and backing surface. */
Frame::~Frame() {
  if (owner) {
    video->destroy_frame(video_frame);
  }
  video_frame = nullptr;
}

/* Draw source frame from rectangle at sx, sy with given
   width and height, to destination frame at dx, dy. */
void
Frame::draw_frame(int dx, int dy, int sx, int sy, Frame *src, int w, int h) {
  video->draw_frame(dx, dy, video_frame, sx, sy, src->video_frame, w, h);
}

void
Frame::draw_line(int x, int y, int x1, int y1, const Color &color) {
  Video::Color c = {color.get_red(),
                    color.get_green(),
                    color.get_blue(),
                    color.get_alpha()};
  video->draw_line(x, y, x1, y1, c, video_frame);
}

void
Frame::draw_thick_line(int x, int y, int x1, int y1, const Color &color) {
  Video::Color c = {color.get_red(),
                    color.get_green(),
                    color.get_blue(),
                    color.get_alpha()};
  video->draw_thick_line(x, y, x1, y1, c, video_frame);
}

Frame *
Graphics::create_frame(unsigned int width, unsigned int height) {
  return new Frame(video, width, height);
}

/* Enable or disable fullscreen mode */
void
Graphics::set_fullscreen(bool enable) {
  video->set_fullscreen(enable);
}

/* Check whether fullscreen mode is enabled */
bool
Graphics::is_fullscreen() {
  return video->is_fullscreen();
}

Frame *
Graphics::get_screen_frame() {
  return new Frame(video, video->get_screen_frame());
}

void
Graphics::set_resolution(unsigned int width, unsigned int height,
                         bool fullscreen) {
  video->set_resolution(width, height, fullscreen);
}

void
Graphics::get_resolution(unsigned int *width, unsigned int *height) {
  video->get_resolution(width, height);
}

void
Graphics::swap_buffers() {
  video->swap_buffers();
}

float
Graphics::get_zoom_factor() {
  return video->get_zoom_factor();
}

bool
Graphics::set_zoom_factor(float factor) {
  return video->set_zoom_factor(factor);
}

void
Graphics::get_screen_factor(float *fx, float *fy) {
  video->get_screen_factor(fx, fy);
}
