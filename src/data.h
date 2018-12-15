/*
 * data.h - Definitions for data file access.
 *
 * Copyright (C) 2012-2018  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_DATA_H_
#define SRC_DATA_H_

#include <string>
#include <list>
#include <memory>

class DataSource;
typedef std::shared_ptr<DataSource> PDataSource;

class Data;
typedef std::unique_ptr<Data> PData;

class Data {
 public:
  typedef enum Type {
    TypeUnknown,
    TypeSprite,
    TypeAnimation,
    TypeSound,
    TypeMusic,
  } Type;

  typedef enum Resource {
    AssetNone,
    AssetArtLandscape,
    AssetAnimation,
    AssetSerfShadow,
    AssetDottedLines,
    AssetArtFlag,
    AssetArtBox,
    AssetCreditsBg,
    AssetLogo,
    AssetSymbol,
    AssetMapMaskUp,
    AssetMapMaskDown,
    AssetPathMask,
    AssetMapGround,
    AssetPathGround,
    AssetGameObject,
    AssetFrameTop,
    AssetMapBorder,
    AssetMapWaves,
    AssetFramePopup,
    AssetIndicator,
    AssetFont,
    AssetFontShadow,
    AssetIcon,
    AssetMapObject,
    AssetMapShadow,
    AssetPanelButton,
    AssetFrameBottom,
    AssetSerfTorso,
    AssetSerfHead,
    AssetFrameSplit,
    AssetSound,
    AssetMusic,
    AssetCursor,
  } Asset;

 protected:
  PDataSource data_source;

  Data();

 public:
  Data(const Data& that) = delete;
  virtual ~Data();

  Data& operator = (const Data& that) = delete;

  static Data &get_instance();

  bool load(const std::string &path);

  PDataSource get_data_source() const { return data_source; }

  static Type get_resource_type(Resource resource);
  static unsigned int get_resource_count(Resource resource);
  static const std::string get_resource_name(Resource resource);

 protected:
  std::list<std::string> get_standard_search_paths() const;
};

#endif  // SRC_DATA_H_
