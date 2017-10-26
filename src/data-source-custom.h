/*
 * data-source-custom.h - Custom data loading
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

#ifndef SRC_DATA_SOURCE_CUSTOM_H_
#define SRC_DATA_SOURCE_CUSTOM_H_

#include <string>
#include <map>

#include "src/data-source.h"
#include "src/configfile.h"

class DataSourceCustom : public DataSource {
 protected:
  typedef struct ResInfo {
    PConfigFile meta;
    std::string path;
  } ResInfo;

 protected:
  PConfigFile meta_main;
  unsigned int scale;
  std::string name;
  std::map<Data::Resource, ResInfo> infos;

 public:
  explicit DataSourceCustom(const std::string &path);
  virtual ~DataSourceCustom();

  virtual std::string get_name() const { return name; }
  virtual unsigned int get_scale() const { return scale; }
  virtual unsigned int get_bpp() const { return 32; }

  virtual bool check();
  virtual bool load();

  virtual MaskImage get_sprite_parts(Data::Resource res, size_t index);

  virtual PBuffer get_sound(size_t index);
  virtual PBuffer get_music(size_t index);

 protected:
  ResInfo *get_info(Data::Resource res);
  bool load_animation_table();
};

#endif  // SRC_DATA_SOURCE_CUSTOM_H_
