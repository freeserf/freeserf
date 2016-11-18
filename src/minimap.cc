/*
 * minimap.cc - Minimap GUI component
 *
 * Copyright (C) 2013-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/minimap.h"

#include <algorithm>

#include "src/game.h"
#include "src/interface.h"
#include "src/viewport.h"

const int
Minimap::max_scale = 8;

Minimap::Minimap(Map *_map) {
  offset_x = 0;
  offset_y = 0;
  scale = 1;

  draw_grid = false;

  set_map(_map);
}

void
Minimap::set_draw_grid(bool _draw_grid) {
  draw_grid = _draw_grid;
  set_redraw();
}

/* Initialize minimap data. */
void
Minimap::init_minimap() {
  static const int color_offset[] = {
    0, 85, 102, 119, 17, 17, 17, 17,
    34, 34, 34, 51, 51, 51, 68, 68
  };

  static const Color colors[] = {
    Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf),
    Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf),
    Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf),
    Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf),
    Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf),
    Color(0x00, 0x00, 0xaf), Color(0x00, 0x00, 0xaf), Color(0x73, 0xb3, 0x43),
    Color(0x73, 0xb3, 0x43), Color(0x6b, 0xab, 0x3b), Color(0x63, 0xa3, 0x33),
    Color(0x5f, 0x9b, 0x2f), Color(0x57, 0x93, 0x27), Color(0x53, 0x8b, 0x23),
    Color(0x4f, 0x83, 0x1b), Color(0x47, 0x7f, 0x17), Color(0x3f, 0x73, 0x13),
    Color(0x3b, 0x6b, 0x13), Color(0x33, 0x63, 0x0f), Color(0x2f, 0x57, 0x0b),
    Color(0x2b, 0x4f, 0x0b), Color(0x23, 0x43, 0x0b), Color(0x1f, 0x3b, 0x07),
    Color(0x1b, 0x33, 0x07), Color(0xef, 0xcf, 0xaf), Color(0xef, 0xcf, 0xaf),
    Color(0xe3, 0xbf, 0x9f), Color(0xd7, 0xb3, 0x8f), Color(0xd7, 0xb3, 0x8f),
    Color(0xcb, 0xa3, 0x7f), Color(0xbf, 0x97, 0x73), Color(0xbf, 0x97, 0x73),
    Color(0xb3, 0x87, 0x67), Color(0xab, 0x7b, 0x5b), Color(0xab, 0x7b, 0x5b),
    Color(0x9f, 0x6f, 0x4f), Color(0x93, 0x63, 0x43), Color(0x93, 0x63, 0x43),
    Color(0x87, 0x57, 0x3b), Color(0x7b, 0x4f, 0x33), Color(0x7b, 0x4f, 0x33),
    Color(0xd7, 0xb3, 0x8f), Color(0xd7, 0xb3, 0x8f), Color(0xcb, 0xa3, 0x7f),
    Color(0xcb, 0xa3, 0x7f), Color(0xbf, 0x97, 0x73), Color(0xbf, 0x97, 0x73),
    Color(0xb3, 0x87, 0x67), Color(0xab, 0x7b, 0x5b), Color(0x9f, 0x6f, 0x4f),
    Color(0x93, 0x63, 0x43), Color(0x87, 0x57, 0x3b), Color(0x7b, 0x4f, 0x33),
    Color(0x73, 0x43, 0x2b), Color(0x67, 0x3b, 0x23), Color(0x5b, 0x33, 0x1b),
    Color(0x4f, 0x2b, 0x17), Color(0x43, 0x23, 0x13), Color(0xff, 0xff, 0xff),
    Color(0xff, 0xff, 0xff), Color(0xef, 0xef, 0xef), Color(0xef, 0xef, 0xef),
    Color(0xdf, 0xdf, 0xdf), Color(0xd3, 0xd3, 0xd3), Color(0xc3, 0xc3, 0xc3),
    Color(0xb3, 0xb3, 0xb3), Color(0xa7, 0xa7, 0xa7), Color(0x97, 0x97, 0x97),
    Color(0x87, 0x87, 0x87), Color(0x7b, 0x7b, 0x7b), Color(0x6b, 0x6b, 0x6b),
    Color(0x5b, 0x5b, 0x5b), Color(0x4f, 0x4f, 0x4f), Color(0x3f, 0x3f, 0x3f),
    Color(0x2f, 0x2f, 0x2f), Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3),
    Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3),
    Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3),
    Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3),
    Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3),
    Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3), Color(0x07, 0x07, 0xb3),
    Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7),
    Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7),
    Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7),
    Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7),
    Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7),
    Color(0x0b, 0x0b, 0xb7), Color(0x0b, 0x0b, 0xb7), Color(0x13, 0x13, 0xbb),
    Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb),
    Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb),
    Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb),
    Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb),
    Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb), Color(0x13, 0x13, 0xbb),
    Color(0x13, 0x13, 0xbb)
  };

  if (map == NULL) {
    return;
  }

  size_t size = map->get_rows() * map->get_cols();
  minimap.reset(new Color[size]());

  Color *mpos = minimap.get();
  for (MapPos pos : map->geom()) {
    int type_off = color_offset[map->type_up(pos)];

    pos = map->move_right(pos);
    int h1 = map->get_height(pos);

    pos = map->move_left(map->move_down(pos));
    int h2 = map->get_height(pos);

    int h_off = h2 - h1 + 8;
    *(mpos++) = colors[type_off + h_off];
  }
}

void
Minimap::draw_minimap_point(int col, int row, const Color &color, int density) {
  int map_width = map->get_cols() * scale;
  int map_height = map->get_rows() * scale;

  if (0 == map_width || 0 == map_height) {
    return;
  }

  int mm_y = row * scale - offset_y;
  col -= (map->get_rows()/2) * static_cast<int>(mm_y / map_height);
  mm_y = mm_y % map_height;

  while (mm_y < height) {
    if (mm_y >= -density) {
      int mm_x = col * scale - (row * scale) / 2 - offset_x;
      mm_x = mm_x % map_width;
      while (mm_x < width) {
        if (mm_x >= -density) {
          frame->fill_rect(mm_x, mm_y, density, density, color);
        }
        mm_x += map_width;
      }
    }
    col -= map->get_rows()/2;
    mm_y += map_height;
  }
}

void
Minimap::draw_minimap_map() {
  Color *color_data = minimap.get();
  for (unsigned int row = 0; row < map->get_rows(); row++) {
    for (unsigned int col = 0; col < map->get_cols(); col++) {
      Color color = *(color_data++);
      draw_minimap_point(col, row, color, scale);
    }
  }
}

void
MinimapGame::draw_minimap_ownership(int density) {
  for (unsigned int row = 0; row < map->get_rows(); row += density) {
    for (unsigned int col = 0; col < map->get_cols(); col += density) {
      MapPos pos = map->pos(col, row);
      if (map->has_owner(pos)) {
        Color color = interface->get_player_color(map->get_owner(pos));
        draw_minimap_point(col, row, color, scale);
      }
    }
  }
}

void
MinimapGame::draw_minimap_roads() {
  for (unsigned int row = 0; row < map->get_rows(); row++) {
    for (unsigned int col = 0; col < map->get_cols(); col++) {
      int pos = map->pos(col, row);
      if (map->paths(pos)) {
        draw_minimap_point(col, row, Color::black, scale);
      }
    }
  }
}

void
MinimapGame::draw_minimap_buildings() {
  const int building_remap[] = {
    Building::TypeCastle,
    Building::TypeStock, Building::TypeTower, Building::TypeHut,
    Building::TypeFortress, Building::TypeToolMaker, Building::TypeSawmill,
    Building::TypeWeaponSmith, Building::TypeStonecutter,
    Building::TypeBoatbuilder, Building::TypeForester, Building::TypeLumberjack,
    Building::TypePigFarm, Building::TypeFarm, Building::TypeFisher,
    Building::TypeMill, Building::TypeButcher, Building::TypeBaker,
    Building::TypeStoneMine, Building::TypeCoalMine, Building::TypeIronMine,
    Building::TypeGoldMine, Building::TypeSteelSmelter,
    Building::TypeGoldSmelter
  };

  for (unsigned int row = 0; row < map->get_rows(); row++) {
    for (unsigned int col = 0; col < map->get_cols(); col++) {
      int pos = map->pos(col, row);
      int obj = map->get_obj(pos);
      if (obj > Map::ObjectFlag && obj <= Map::ObjectCastle) {
        Color color = interface->get_player_color(map->get_owner(pos));
        if (advanced > 0) {
          Building *bld = interface->get_game()->get_building_at_pos(pos);
          if (bld->get_type() == building_remap[advanced]) {
            draw_minimap_point(col, row, color, scale);
          }
        } else {
          draw_minimap_point(col, row, color, scale);
        }
      }
    }
  }
}

void
MinimapGame::draw_minimap_traffic() {
  for (unsigned int row = 0; row < map->get_rows(); row++) {
    for (unsigned int col = 0; col < map->get_cols(); col++) {
      int pos = map->pos(col, row);
      if (map->get_idle_serf(pos)) {
        Color color = interface->get_player_color(map->get_owner(pos));
        draw_minimap_point(col, row, color, scale);
      }
    }
  }
}

void
Minimap::draw_minimap_grid() {
  for (unsigned int y = 0; y < map->get_rows() * scale; y += 2) {
    draw_minimap_point(0, y, Color(0xab, 0x7b, 0x5b), 1);
    draw_minimap_point(0, y+1, Color::black, 1);
  }

  for (unsigned int x = 0; x < map->get_cols() * scale; x += 2) {
    draw_minimap_point(x, 0, Color(0xab, 0x7b, 0x5b), 1);
    draw_minimap_point(x+1, 0, Color::black, 1);
  }
}

void
Minimap::draw_minimap_rect() {
  int y = height/2;
  int x = width/2;
  frame->draw_sprite(x, y, Data::AssetGameObject, 33, true);
}

void
Minimap::internal_draw() {
  if (map == NULL) {
    frame->fill_rect(0, 0, width, height, Color::black);
    return;
  }

  draw_minimap_map();

  if (draw_grid) {
    draw_minimap_grid();
  }
}

int
Minimap::handle_scroll(int up) {
  int scale_ = 0;

  if (up) {
    scale_ = scale + 1;
  } else {
    scale_ = scale - 1;
  }

  set_scale(clamp(1, scale_, max_scale));

  return 0;
}

bool
Minimap::handle_drag(int dx, int dy) {
  if (dx != 0 || dy != 0) {
    move_by_pixels(dx, dy);
  }

  return true;
}

void
Minimap::set_map(Map *_map) {
  map = _map;
  init_minimap();
  set_redraw();
}

/* Set the scale of the map (zoom). Must be positive. */
void
Minimap::set_scale(int scale) {
  MapPos pos = get_current_map_pos();
  this->scale = scale;
  move_to_map_pos(pos);

  set_redraw();
}

void
Minimap::screen_pix_from_map_pix(int mx, int my, int *sx, int *sy) {
  int width = map->get_cols() * scale;
  int height = map->get_rows() * scale;

  *sx = mx - offset_x;
  *sy = my - offset_y;

  while (*sy < 0) {
    *sx -= height/2;
    *sy += height;
  }

  while (*sy >= height) {
    *sx += height/2;
    *sy -= height;
  }

  while (*sx < 0) *sx += width;
  while (*sx >= width) *sx -= width;
}

void
Minimap::map_pix_from_map_coord(MapPos pos, int *mx, int *my) {
  int width = map->get_cols() * scale;
  int height = map->get_rows() * scale;

  *mx = scale * map->pos_col(pos) - (scale * map->pos_row(pos))/2;
  *my = scale * map->pos_row(pos);

  if (*my < 0) {
    *mx -= height/2;
    *my += height;
  }

  if (*mx < 0) *mx += width;
  else if (*mx >= width) *mx -= width;
}

MapPos
Minimap::map_pos_from_screen_pix(int x, int y) {
  int mx = x + offset_x;
  int my = y + offset_y;

  int col = ((my/2 + mx)/scale) & map->get_col_mask();
  int row = (my/scale) & map->get_row_mask();

  return map->pos(col, row);
}

MapPos
Minimap::get_current_map_pos() {
  return map_pos_from_screen_pix(width/2, height/2);
}

void
Minimap::move_to_map_pos(MapPos pos) {
  int mx, my;
  map_pix_from_map_coord(pos, &mx, &my);

  int map_width = map->get_cols()*scale;
  int map_height = map->get_rows()*scale;

  /* Center view */
  mx -= width/2;
  my -= height/2;

  if (my < 0) {
    mx -= map_height/2;
    my += map_height;
  }

  if (mx < 0) mx += map_width;
  else if (mx >= map_width) mx -= map_width;

  offset_x = mx;
  offset_y = my;

  set_redraw();
}

void
Minimap::move_by_pixels(int dx, int dy) {
  int width = map->get_cols() * scale;
  int height = map->get_rows() * scale;

  offset_x += dx;
  offset_y += dy;

  if (offset_y < 0) {
    offset_y += height;
    offset_x -= height/2;
  } else if (offset_y >= height) {
    offset_y -= height;
    offset_x += height/2;
  }

  if (offset_x >= width) offset_x -= width;
  else if (offset_x < 0) offset_x += width;

  set_redraw();
}

MinimapGame::MinimapGame(Interface *_interface, Game *_game)
  : Minimap(_game->get_map()) {
  interface = _interface;
  game = _game;
  draw_roads = false;
  draw_buildings = true;
  ownership_mode = OwnershipModeNone;
  advanced = -1;
}

void
MinimapGame::set_ownership_mode(OwnershipMode _ownership_mode) {
  ownership_mode = _ownership_mode;
  set_redraw();
}

void
MinimapGame::set_draw_roads(bool _draw_roads) {
  draw_roads = _draw_roads;
  set_redraw();
}

void
MinimapGame::set_draw_buildings(bool _draw_buildings) {
  draw_buildings = _draw_buildings;
  set_redraw();
}

void
MinimapGame::internal_draw() {
  switch (ownership_mode) {
    case OwnershipModeNone:
      draw_minimap_map();
      break;
    case OwnershipModeMixed:
      draw_minimap_map();
      draw_minimap_ownership(2);
      break;
    case OwnershipModeSolid:
      frame->fill_rect(0, 0, 128, 128, Color::black);
      draw_minimap_ownership(1);
      break;
  }

  if (draw_roads) {
    draw_minimap_roads();
  }

  if (draw_buildings) {
    draw_minimap_buildings();
  }

  if (draw_grid) {
    draw_minimap_grid();
  }

  if (advanced > 0) {
    draw_minimap_traffic();
  }

  draw_minimap_rect();
}

bool
MinimapGame::handle_click_left(int x, int y) {
  MapPos pos = map_pos_from_screen_pix(x, y);
  interface->get_viewport()->move_to_map_pos(pos);

  interface->update_map_cursor_pos(pos);
  interface->close_popup();

  return true;
}
