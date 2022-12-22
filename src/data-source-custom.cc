/*
 * data-source-custom.cc - Custom data loading
 *
 * Copyright (C) 2017-2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/data-source-custom.h"

#include <iomanip>
#include <memory>
#include <vector>
#include <utility>

#include "src/sprite-file.h"
#include "src/buffer.h"

DataSourceCustom::DataSourceCustom(const std::string &_path)
  : DataSourceBase(_path)
  , scale(1)
  , name("Unnamed") {
}

DataSourceCustom::~DataSourceCustom() {
}

bool
DataSourceCustom::check() {
  if (check_file(path + "/meta.ini")) {
    return true;
  }

  return false;
}

bool
DataSourceCustom::load() {
  meta_main = std::make_shared<ConfigFile>();
  if (!meta_main->load(path + "/meta.ini")) {
    return false;
  }
  scale = meta_main->value("general", "scale", 1);
  name = meta_main->value("general", "name", "Unnamed");

  // it seems like this literally only loads animations, and not
  // any of the other custom data objects.  Perhaps the others are
  // loaded on the fly as needed during draw_sprite calls?
  // if so, it means it may be possible to skip this if no custom
  // animations are actually needed.  Trying this out...
  //  YES, that is the case.  disabling enforcement of this check allows partial custom data sources!
  loaded = load_animation_table();
  // throw a warning message at least
  if (!loaded){
    Log::Warn["data-source-custom.cc"] << "failed to load 'animation' data from custom datasource, however this only a problem if you are attempting to load custom animation.  Currently Forkserf does *not* use any custom animations.  Ignoring and continuing";
    loaded = true;  // and just fake success if we go this far.  If there is absolutely no custom data the top check for meta.ini will fail and 'loaded' bool will remain false
  }

  return loaded;
}

// using DataSourceCustom graphics requires SDL2_Image included in build or it will fail!
Data::MaskImage
//DataSourceCustom::get_sprite_parts(Data::Resource res, size_t index) {
DataSourceCustom::get_sprite_parts(Data::Resource res, size_t index, bool darken) {
  //Log::Info["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index;
  ResInfo *info = get_info(res);
  if (info == nullptr) {
    Log::Warn["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << " get_info returned nullptr";
    return std::make_tuple(nullptr, nullptr);
  }
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(3) << index;
  std::string section = stream.str();

  int delta_x = info->meta->value(section, "delta_x", 0);
  int delta_y = info->meta->value(section, "delta_y", 0);
  int offset_x = info->meta->value(section, "offset_x", 0);
  int offset_y = info->meta->value(section, "offset_y", 0);
  std::string image_file_name = info->meta->value(section,
                                                  "image_path",
                                                  std::string());
  std::string mask_file_name = info->meta->value(section,
                                                 "mask_path",
                                                 std::string());

  //Log::Debug["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", trying to find sprite file";
  PSpriteFile image;
  if (!image_file_name.empty()) {
    //Log::Debug["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", image file name is " << image_file_name;
    image = std::make_shared<SpriteFile>();
    if (image->load(info->path + "/" + image_file_name)) {
      image->set_delta(delta_x, delta_y);
      image->set_offset(offset_x, offset_y);
      //Log::Debug["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", image->load succeeded for image " << info->path << "/" << image_file_name;
    } else {
      Log::Warn["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", image->load succeeded for image " << info->path << "/" << image_file_name;
      image = nullptr;
    }
  //}else{
    //Log::Debug["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", image_file_name is empty!";
  }

  //Log::Debug["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", trying to find mask file";
  PSpriteFile mask;
  if (!mask_file_name.empty()) {
    mask = std::make_shared<SpriteFile>();
    if (mask->load(info->path + "/" + mask_file_name)) {
      mask->set_delta(delta_x, delta_y);
      mask->set_offset(offset_x, offset_y);
      //Log::Debug["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", mask->load succeeded for mask " << info->path << "/" << mask_file_name;
    } else {
      Log::Warn["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", mask->load failed for mask " << info->path << "/" << mask_file_name;
      mask = nullptr;
    }
  //}else{
   // Log::Debug["data-source-custom"] << "inside DataSourceCustom::get_sprite_parts with res " << res << ", index " << index << ", mask_file_name is empty!";
  }

  return std::make_tuple(mask, image);
}

PBuffer
DataSourceCustom::get_sound(size_t index) {
  ResInfo *info = get_info(Data::AssetSound);
  if (info == nullptr) {
    return nullptr;
  }

  std::stringstream stream;
  stream << std::setfill('0') << std::setw(3) << index;
  std::string file_name = info->meta->value(stream.str(), "path", stream.str());
  std::string fpath = info->path + "/" + file_name;

  try {
    PBuffer file = std::make_shared<Buffer>(fpath);
    return file;
  } catch (...) {
    return nullptr;
  }
}

PBuffer
DataSourceCustom::get_music(size_t index) {
  ResInfo *info = get_info(Data::AssetMusic);
  if (info == nullptr) {
    return nullptr;
  }

  std::stringstream stream;
  stream << std::setfill('0') << std::setw(3) << index;
  std::string file_name = info->meta->value(stream.str(), "path", stream.str());
  std::string fpath = info->path + "/" + file_name;

  try {
    PBuffer file = std::make_shared<Buffer>(fpath);
    return file;
  } catch (...) {
    return nullptr;
  }
}

DataSourceCustom::ResInfo *
DataSourceCustom::get_info(Data::Resource res) {
  if (infos.find(res) == infos.end()) {
    std::string dir_name = meta_main->value("resources",
                                            Data::get_resource_name(res),
                                            Data::get_resource_name(res));
    std::string dir_path = path + "/" + dir_name;

    PConfigFile meta = std::make_shared<ConfigFile>();
    if (!meta->load(dir_path + "/meta.ini")) {
      return nullptr;
    }
    ResInfo info;
    info.meta = std::move(meta);
    info.path = std::move(dir_path);
    infos[res] = info;
  }

  return &infos[res];
}

bool
DataSourceCustom::load_animation_table() {
  ResInfo *info = get_info(Data::AssetAnimation);
  if (info == nullptr) {
    return false;
  }

  for (size_t i = 0; i < Data::get_resource_count(Data::AssetAnimation); i++) {
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(3) << i;
    ConfigFile meta = ConfigFile();
    if (!meta.load(info->path + "/" + stream.str() + ".ini")) {
      return false;
    }
    size_t count = meta.value("general", "count", 0);
    std::vector<Data::Animation> animations;
    for (size_t j = 0; j < count; j++) {
      std::stringstream stream;
      stream << std::setfill('0') << std::setw(3) << j;
      Data::Animation animation;
      animation.sprite = meta.value(stream.str(), "sprite", 0);
      animation.x = meta.value(stream.str(), "x", 0);
      animation.y = meta.value(stream.str(), "y", 0);
      animations.push_back(animation);
    }
    animation_table.push_back(animations);
  }

  return true;
}
