/*
 * data-source-custom.cc - Custom data loading
 *
 * Copyright (C) 2017  Wicked_Digger <wicked_digger@mail.ru>
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

DataSourceCustom::DataSourceCustom(const std::string &_path)
  : DataSource(_path)
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

  loaded = load_animation_table();

  return loaded;
}

DataSource::MaskImage
DataSourceCustom::get_sprite_parts(Data::Resource res, size_t index) {
  ResInfo *info = get_info(res);
  if (info == nullptr) {
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

  PSpriteFile image;
  if (!image_file_name.empty()) {
    image = std::make_shared<SpriteFile>();
    if (image->load(info->path + "/" + image_file_name)) {
      image->set_delta(delta_x, delta_y);
      image->set_offset(offset_x, offset_y);
    } else {
      image = nullptr;
    }
  }

  PSpriteFile mask;
  if (!mask_file_name.empty()) {
    mask = std::make_shared<SpriteFile>();
    if (mask->load(info->path + "/" + mask_file_name)) {
      mask->set_delta(delta_x, delta_y);
      mask->set_offset(offset_x, offset_y);
    } else {
      mask = nullptr;
    }
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
    std::vector<Animation> animations;
    for (size_t j = 0; j < count; j++) {
      std::stringstream stream;
      stream << std::setfill('0') << std::setw(3) << j;
      Animation animation;
      animation.sprite = meta.value(stream.str(), "sprite", 0);
      animation.x = meta.value(stream.str(), "x", 0);
      animation.y = meta.value(stream.str(), "y", 0);
      animations.push_back(animation);
    }
    animation_table.push_back(animations);
  }

  return true;
}
