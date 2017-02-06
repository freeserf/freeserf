/*
 * game-init.cc - Game initialization GUI component
 *
 * Copyright (C) 2013-2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/game-init.h"

#include <ctime>
#include <algorithm>
#include <sstream>
#include <string>

#include "src/misc.h"
#include "src/mission.h"
#include "src/data.h"
#include "src/interface.h"
#include "src/version.h"
#include "src/text-input.h"
#include "src/minimap.h"
#include "src/map-generator.h"
#include "src/map-geometry.h"

class RandomInput : public TextInput {
 protected:
  std::string saved_text;

 public:
  RandomInput() {
    set_filter(text_input_filter);
    set_size(34, 34);
    set_max_length(16);
  }

  void set_random(const Random &rnd) {
    std::string str = rnd;
    set_text(str.c_str());
  }
  Random get_random() {
    return Random(text);
  }

 protected:
  static bool
  text_input_filter(const char key, TextInput *text_input) {
    if (key < '1' || key > '8') {
      return false;
    }

    if (text_input->get_text().length() > 16) {
      return false;
    }

    return true;
  }

  virtual bool handle_click_left(int x, int y) {
    TextInput::handle_click_left(x, y);
    saved_text = text;
    text.clear();
    return true;
  }

  virtual bool handle_focus_loose() {
    TextInput::handle_focus_loose();
    if ((text.length() < 16) && (saved_text.length() == 16)) {
      text = saved_text;
      saved_text.clear();
    }
    return true;
  }
};

void
GameInitBox::draw_box_icon(int x, int y, int sprite) {
  frame->draw_sprite(8*x+20, y+16, Data::AssetIcon, sprite);
}

void
GameInitBox::draw_box_string(int x, int y, const std::string &str) {
  frame->draw_string(8*x+20, y+16, str, Color::green, Color::black);
}

void
GameInitBox::draw_background() {
  // Background
  unsigned int icon = 290;
  for (int y = 0; y < height; y += 8) {
    for (int x = 0; x < width; x += 40) {
      frame->draw_sprite(x, y, Data::AssetIcon, icon);
    }
    icon--;
    if (icon < 290) {
      icon = 294;
    }
  }
}

/* Get the sprite number for a face. */
unsigned int
GameInitBox::get_player_face_sprite(size_t face) {
  if (face != 0) {
    return static_cast<unsigned int>(0x10b + face);
  }
  return 0x119; /* sprite_face_none */
}

void
GameInitBox::internal_draw() {
  draw_background();

  const int layout[] = {
    266, 0, 0,
    267, 31, 0,
    316, 36, 0,
    -1
  };

  const int *i = layout;
  while (i[0] >= 0) {
    draw_box_icon(i[1], i[2], i[0]);
    i += 3;
  }

  /* Game type settings */
  if (game_mission < 0) {
    draw_box_icon(5, 0, 263);

    std::stringstream str_map_size;
    str_map_size << mission->get_map_size();

    draw_box_string(10, 2, "New game");
    draw_box_string(10, 18, "Mapsize:");
    draw_box_string(18, 18, str_map_size.str());

    draw_box_icon(20, 0, 265);
  } else {
    draw_box_icon(5, 0, 260);

    std::stringstream level;
    level << (game_mission+1);

    draw_box_string(10, 2, "Start mission");
    draw_box_string(10, 18, "Mission:");
    draw_box_string(20, 18, level.str());

    draw_box_icon(28, 0, 237);
    draw_box_icon(28, 16, 240);
  }

  /* Game info */
  int x = 0;
  int y = 0;
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    draw_player_box(i, 10 * x, 40 + y * 80);
    x++;
    if (i == 1) {
      y++;
      x = 0;
    }
  }

  /* Display program name and version in caption */
  draw_box_string(0, 212, FREESERF_VERSION);

  draw_box_icon(38, 208, 60); /* exit */
}

void
GameInitBox::draw_player_box(unsigned int player, int x, int y) {
  const int layout[] = {
    251, 0, 0,
    252, 0, 72,
    253, 0, 8,
    254, 5, 8,
    255, 9, 8,
    -1
  };

  const int *i = layout;
  while (i[0] >= 0) {
    draw_box_icon(x+i[1], y+i[2], i[0]);
    i += 3;
  }

  y += 8;
  x += 1;

  unsigned int face = 0;
  if (player < mission->get_player_count()) {
    face = mission->get_player(player)->get_face();
  }

  draw_box_icon(x, y, get_player_face_sprite(face));
  draw_box_icon(x+5, y, 282);

  if (player < mission->get_player_count()) {
    x *= 8;

    unsigned int supplies = mission->get_player(player)->get_supplies();
    frame->fill_rect(x + 64, y + 76 - supplies, 4, supplies,
                     Color(0x00, 0x93, 0x87));

    unsigned int intelligence = mission->get_player(player)->get_intelligence();
    frame->fill_rect(x + 70, y + 76 - intelligence, 4, intelligence,
                     Color(0x6b, 0xab, 0x3b));

    unsigned int reproduction = mission->get_player(player)->get_reproduction();
    frame->fill_rect(x + 76, y + 76 - reproduction, 4, reproduction,
                     Color(0xa7, 0x27, 0x27));
  }
}

void
GameInitBox::handle_action(int action) {
  switch (action) {
    case ActionStartGame: {
      Game *game = new Game();
      if (!game->load_mission_map(mission)) return;

      Game *old_game = interface->get_game();
      if (old_game != NULL) {
        EventLoop::get_instance()->del_handler(old_game);
      }

      EventLoop::get_instance()->add_handler(game);
      interface->set_game(game);
      if (old_game != NULL) {
        delete old_game;
      }
      interface->set_player(0);
      interface->close_game_init();
      break;
    }
    case ActionToggleGameType:
      if (game_mission < 0) {
        game_mission = 0;
        mission = GameInfo::get_mission(game_mission);
        field->set_displayed(false);
        generate_map_preview();
      } else {
        game_mission = -1;
        mission = custom_mission;
        field->set_displayed(true);
        field->set_random(custom_mission->get_random_base());
        generate_map_preview();
      }
      break;
    case ActionShowOptions:
      break;
    case ActionShowLoadGame:
      break;
    case ActionIncrement:
      if (game_mission < 0) {
        custom_mission->set_map_size(std::min(10u,
                                           custom_mission->get_map_size() + 1));
      } else {
        game_mission = std::min(game_mission+1,
                             static_cast<int>(GameInfo::get_mission_count())-1);
        mission = GameInfo::get_mission(game_mission);
      }
      generate_map_preview();
      break;
    case ActionDecrement:
      if (game_mission < 0) {
        custom_mission->set_map_size(std::max(3u,
                                           custom_mission->get_map_size() - 1));
      } else {
        game_mission = std::max(0, game_mission-1);
        mission = GameInfo::get_mission(game_mission);
      }
      generate_map_preview();
      break;
    case ActionClose:
      interface->close_game_init();
      break;
    case ActionGenRandom: {
      field->set_random(Random());
      set_redraw();
      break;
    }
    case ActionApplyRandom: {
      std::string str = field->get_text();
      if (str.length() == 16) {
        custom_mission = PGameInfo(new GameInfo(field->get_random()));
        mission = custom_mission;
        generate_map_preview();
      }
      break;
    }
    default:
      break;
  }
}

bool
GameInitBox::handle_click_left(int x, int y) {
  const int clickmap_mission[] = {
    ActionStartGame,        20,  16, 32, 32,
    ActionToggleGameType,  60,  16, 32, 32,
    ActionShowOptions,     268,  16, 32, 32,
    ActionShowLoadGame,   308,  16, 32, 32,
    ActionIncrement,        244,  16, 16, 16,
    ActionDecrement,        244,  32, 16, 16,
    ActionClose,            324, 216, 16, 16,
    -1
  };

  const int clickmap_custom[] = {
    ActionStartGame,        20,  16, 32, 32,
    ActionToggleGameType,  60,  16, 32, 32,
    ActionShowOptions,     268,  16, 32, 32,
    ActionShowLoadGame,   308,  16, 32, 32,
    ActionIncrement,        180,  24, 24, 24,
    ActionDecrement,        180,  16,  8,  8,
    ActionGenRandom,       204,  16, 16,  8,
    ActionApplyRandom ,    204,  24, 16, 24,
    ActionClose,            324, 216, 16, 16,
    -1
  };

  const int *clickmap = clickmap_mission;
  if (game_mission < 0) {
    clickmap = clickmap_custom;
  }

  const int *i = clickmap;
  while (i[0] >= 0) {
    if (x >= i[1] && x < i[1]+i[3] && y >= i[2] && y < i[2]+i[4]) {
      set_redraw();
      handle_action(i[0]);
      return true;
    }
    i += 5;
  }

  /* Check player area */
  int cx = 0;
  int cy = 0;
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    int px = 20 + cx*80;
    int py = 56 + cy*80;
    if ((x > px) && (x < px + 80) && (y > py) && (y < py + 80)) {
      if (handle_player_click(i, x - px, y - py)) {
        break;
      }
    }
    cx++;
    if (i == 1) {
      cy++;
      cx = 0;
    }
  }

  return true;
}

bool
GameInitBox::handle_player_click(unsigned int player_index, int x, int y) {
  if (x < 8 || x > 8 + 64 || y < 8 || y > 76) {
    return false;
  }

  if (player_index >= mission->get_player_count()) {
    return true;
  }

  PPlayerInfo player = mission->get_player(player_index);

  if (x >= 8 && x < 8+32 && y >= 8 && y < 72) {
    /* Face */
    bool in_use = false;
    do {
      unsigned int next = (player->get_face() + 1) % 14;
      next = std::max(1u, next);
      player->set_character(next);

      /* Check that face is not already in use by another player */
      in_use = 0;
      for (size_t i = 0; i < mission->get_player_count(); i++) {
        if (player_index != i &&
            mission->get_player(i)->get_face() == next) {
          in_use = true;
          break;
        }
      }
    } while (in_use);
  } else {
    x -= 8 + 32 + 8 + 3;
    if (x < 0) {
      return false;
    }
    if (y >= 27 && y < 69) {
      unsigned int value = clamp(0, 68 - y, 40);
      if (x > 0 && x < 6) {
        /* Supplies */
        player->set_supplies(value);
      } else if (x > 6 && x < 12) {
        /* Intelligence */
        player->set_intelligence(value);
      } else if (x > 12 && x < 18) {
        /* Reproduction */
        player->set_reproduction(value);
      }
    }
  }

  set_redraw();

  return true;
}

void
GameInitBox::generate_map_preview() {
  map.reset(new Map(MapGeometry(mission->get_map_size())));
  if (game_mission < 0) {
    ClassicMapGenerator generator(*map, mission->get_random_base());
    generator.init(MapGenerator::HeightGeneratorMidpoints, true);
    generator.generate();
    map->init_tiles(generator);
  } else {
    ClassicMissionMapGenerator generator(*map, mission->get_random_base());
    generator.init();
    generator.generate();
    map->init_tiles(generator);
  }

  minimap->set_map(map);

  set_redraw();
}

GameInitBox::GameInitBox(Interface *interface)
    : minimap(new Minimap(nullptr)),
      field(new RandomInput()) {
  this->interface = interface;
  game_mission = -1;

  set_size(360, 254);

  custom_mission = PGameInfo(new GameInfo(Random()));
  custom_mission->add_player(12, {0x00, 0xe3, 0xe3}, 40, 40, 40);
  custom_mission->add_player(1, {0xcf, 0x63, 0x63}, 20, 30, 40);
  mission = custom_mission;

  minimap->set_displayed(true);
  minimap->set_size(150, 160);
  add_float(minimap.get(), 190, 55);

  generate_map_preview();

  field->set_random(custom_mission->get_random_base());
  field->set_displayed(true);
  add_float(field.get(), 19 + 26*8, 15);
}

GameInitBox::~GameInitBox() {}
