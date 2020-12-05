/*
 * game-init.cc - Game initialization GUI component
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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
#include "src/list.h"
#include "src/game-manager.h"
#include "src/popup.h"

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
    set_text(str);
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

  virtual bool handle_click_left(int cx, int cy) {
    TextInput::handle_click_left(cx, cy);
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

GameInitBox::GameInitBox(Interface *interface)
  : random_input(new RandomInput())
  , minimap(new Minimap(nullptr))
  , file_list(new ListSavedFiles()) {
  this->interface = interface;

  game_type = GameCustom;
  game_mission = 0;

  set_size(360, 254);

  custom_mission = std::make_shared<GameInfo>(Random());
  custom_mission->remove_all_players();
  Log::Debug["mission"] << " inside GameInitBox::GameInitBox(Interface) constructor, calling custom_mission->add_player(12, HARDCODED)";
  custom_mission->add_player(12, {0x00, 0xe3, 0xe3}, 40, 40, 40);
  Log::Debug["mission"] << " inside GameInitBox::GameInitBox(Interface) constructor, calling custom_mission->add_player(1, HARDCODED)";
  custom_mission->add_player(1, {0xcf, 0x63, 0x63}, 20, 30, 40);
  mission = custom_mission;

  minimap->set_displayed(true);
  minimap->set_size(150, 160);
  add_float(minimap.get(), 190, 55);

  generate_map_preview();

  random_input->set_random(custom_mission->get_random_base());
  random_input->set_displayed(true);
  add_float(random_input.get(), 19 + 31*8, 15);

  file_list->set_size(160, 160);
  file_list->set_displayed(false);
  file_list->set_selection_handler([this](const std::string &item) {
    Game game;
    if (GameStore::get_instance().load(item, &game)) {
      this->map = game.get_map();
      this->minimap->set_map(map);
    }
  });
  add_float(file_list.get(), 20, 55);
}

GameInitBox::~GameInitBox() {
}

void
GameInitBox::draw_box_icon(int ix, int iy, int sprite) {
  frame->draw_sprite(8 * ix + 20, iy + 16, Data::AssetIcon, sprite);
}

void
GameInitBox::draw_box_string(int sx, int sy, const std::string &str) {
  frame->draw_string(8 * sx + 20, sy + 16, str, Color::green, Color::black);
}

void
GameInitBox::draw_background() {
  // Background
  unsigned int icon = 290;
  for (int by = 0; by < height; by += 8) {
    for (int bx = 0; bx < width; bx += 40) {
      frame->draw_sprite(bx, by, Data::AssetIcon, icon);
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
    266, 0, 0,    // Start button
    267, 36, 0,   // Options button
    -1
  };

  const int *i = layout;
  while (i[0] >= 0) {
    draw_box_icon(i[1], i[2], i[0]);
    i += 3;
  }

  /* Game type settings */
  switch (game_type) {
    case GameMission: {
      draw_box_icon(5, 0, 260);  // Game type

      std::stringstream level;
      level << (game_mission+1);

      draw_box_string(10, 2, "Start mission");
      draw_box_string(10, 18, "Mission:");
      draw_box_string(20, 18, level.str());

      draw_box_icon(33, 0, 237);  // Up button
      draw_box_icon(33, 16, 240);  // Down button

      break;
    }
    case GameCustom: {
      draw_box_icon(5, 0, 263);  // Game type

      std::stringstream str_map_size;
      str_map_size << mission->get_map_size();

      draw_box_string(10, 2, "New game");
      draw_box_string(10, 18, "Mapsize:");
      draw_box_string(18, 18, str_map_size.str());

      draw_box_icon(25, 0, 265);

      break;
    }
    case GameLoad: {
      draw_box_icon(5, 0, 316);  // Game type

      draw_box_string(10, 2, "Load game");

      break;
    }
  }

  /* Game info */
  if (game_type != GameLoad) {
    int bx = 0;
    int by = 0;
    for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
      draw_player_box(i, 10 * bx, 40 + by * 80);
      bx++;
      if (i == 1) {
        by++;
        bx = 0;
      }
    }
  }

  /* Display program name and version in caption */
  draw_box_string(0, 212, FREESERF_VERSION);

  draw_box_icon(38, 208, 60); /* exit */
}

void
GameInitBox::draw_player_box(unsigned int player, int bx, int by) {
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
    draw_box_icon(bx+i[1], by+i[2], i[0]);
    i += 3;
  }

  by += 8;
  bx += 1;

  unsigned int face = 0;
  if (player < mission->get_player_count()) {
    face = mission->get_player(player)->get_face();
  }

  draw_box_icon(bx, by, get_player_face_sprite(face));
  draw_box_icon(bx+5, by, 282);
  if (game_type == GameCustom) {
    draw_box_icon(bx+4, by, 308);
    draw_box_icon(bx+5, by, (face == 0) ? 287 : 259);
  }

  if (player < mission->get_player_count()) {
    bx *= 8;

    PPlayerInfo pi = mission->get_player(player);
    unsigned int supplies = pi->get_supplies();
    frame->fill_rect(bx + 64, by + 76 - supplies, 4, supplies,
                     Color(0x00, 0x93, 0x87));

    unsigned int intelligence = pi->get_intelligence();
    frame->fill_rect(bx + 70, by + 76 - intelligence, 4, intelligence,
                     Color(0x6b, 0xab, 0x3b));

    unsigned int reproduction = pi->get_reproduction();
    frame->fill_rect(bx + 76, by + 76 - reproduction, 4, reproduction,
                     Color(0xa7, 0x27, 0x27));
  }
}

void
GameInitBox::handle_action(int action) {
  Log::Debug["game-init"] << " inside GameInitBox::handle_action";
  switch (action) {
    case ActionStartGame: {
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionStartGame'";

      if (game_type == GameLoad) {
        std::string path = file_list->get_selected();
		Log::Debug["game-init"] << "starting game using load_game(" << path << ")";
        if (!GameManager::get_instance().load_game(path)) {
          return;
        }
      } else {
		  Log::Debug["game-init"] << "starting game using start_game(mission)";
        if (!GameManager::get_instance().start_game(mission)) {
          return;
        }
      }

      interface->close_game_init();
      break;
    }
    case ActionToggleGameType:
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionToggleGameType'";
      game_type++;
      if (game_type > GameLoad) {
        game_type = GameCustom;
      }
      switch (game_type) {
        case GameMission: {
          mission = GameInfo::get_mission(game_mission);
          random_input->set_displayed(false);
          file_list->set_displayed(false);
          generate_map_preview();
          break;
        }
        case GameCustom: {
          mission = custom_mission;
          random_input->set_displayed(true);
          random_input->set_random(custom_mission->get_random_base());
          file_list->set_displayed(false);
          generate_map_preview();
          break;
        }
        case GameLoad: {
          random_input->set_displayed(false);
          file_list->set_displayed(true);
          break;
        }
      }
      break;
    case ActionShowOptions: {
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionShowOptions'";
      if (interface) {
        interface->open_popup(PopupBox::TypeOptions);
      }
      break;
    }
    case ActionIncrement:
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionIncrement'";
      switch (game_type) {
        case GameMission:
          game_mission = std::min(game_mission+1,
                             static_cast<int>(GameInfo::get_mission_count())-1);
          mission = GameInfo::get_mission(game_mission);
          break;
        case GameCustom:
          custom_mission->set_map_size(std::min(10u,
                                           custom_mission->get_map_size() + 1));
          break;
      }
      generate_map_preview();
      break;
    case ActionDecrement:
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionDecrement'";
      switch (game_type) {
        case GameMission:
          game_mission = std::max(0, game_mission-1);
          mission = GameInfo::get_mission(game_mission);
          break;
        case GameCustom:
          custom_mission->set_map_size(std::max(3u,
                                           custom_mission->get_map_size() - 1));
          break;
      }
      generate_map_preview();
      break;
    case ActionClose:
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionClose'";
      interface->close_game_init();
      break;
    case ActionGenRandom: {
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionGenRandom'";
      random_input->set_random(Random());
      set_redraw();
      break;
    }
    case ActionApplyRandom: {
		Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'ActionApplyRandom'";
      std::string str = random_input->get_text();
      if (str.length() == 16) {
        custom_mission->set_random_base(random_input->get_random());
        mission = custom_mission;
        generate_map_preview();
      }
      break;
    }
    default:
	  Log::Debug["game-init"] << " inside GameInitBox::handle_action, switch case is 'default'";
      break;
  }
}

bool
GameInitBox::handle_click_left(int cx, int cy) {
  const int clickmap_mission[] = {
    ActionStartGame,        20,  16, 32, 32,
    ActionToggleGameType,   60,  16, 32, 32,
    ActionShowOptions,     308,  16, 32, 32,
    ActionIncrement,       284,  16, 16, 16,
    ActionDecrement,       284,  32, 16, 16,
    ActionClose,           324, 216, 16, 16,
    -1
  };

  const int clickmap_custom[] = {
    ActionStartGame,        20,  16, 32, 32,
    ActionToggleGameType,   60,  16, 32, 32,
    ActionShowOptions,     308,  16, 32, 32,
    ActionIncrement,       220,  24, 24, 24,
    ActionDecrement,       220,  16,  8,  8,
    ActionGenRandom,       244,  16, 16,  8,
    ActionApplyRandom ,    244,  24, 16, 24,
    ActionClose,           324, 216, 16, 16,
    -1
  };

  const int clickmap_load[] = {
    ActionStartGame,        20,  16, 32, 32,
    ActionToggleGameType,   60,  16, 32, 32,
    ActionShowOptions,     308,  16, 32, 32,
    ActionClose,           324, 216, 16, 16,
    -1
  };

  const int *clickmap = nullptr;
  switch (game_type) {
    case GameMission:
      clickmap = clickmap_mission;
      break;
    case GameCustom:
      clickmap = clickmap_custom;
      break;
    case GameLoad:
      clickmap = clickmap_load;
      break;
    default:
      return false;
  }

  const int *i = clickmap;
  while (i[0] >= 0) {
    if (cx >= i[1] && cx < i[1]+i[3] && cy >= i[2] && cy < i[2]+i[4]) {
      set_redraw();
      handle_action(i[0]);
      return true;
    }
    i += 5;
  }

  /* Check player area */
  int lx = 0;
  int ly = 0;
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    int px = 20 + lx * 80;
    int py = 56 + ly * 80;
    if ((cx > px) && (cx < px + 80) && (cy > py) && (cy < py + 80)) {
      if (handle_player_click(i, cx - px, cy - py)) {
        break;
      }
    }
    lx++;
    if (i == 1) {
      ly++;
      lx = 0;
    }
  }

  return true;
}

unsigned int
GameInitBox::get_next_character(unsigned int player_index) {
  bool in_use = false;
  PPlayerInfo player = mission->get_player(player_index);
  unsigned int next = player->get_face();
  do {
    next = (next + 1) % 14;
    next = std::max(1u, next);
    /* Check that face is not already in use by another player */
    in_use = 0;
    for (size_t i = 0; i < mission->get_player_count(); i++) {
      if (player_index != i && mission->get_player(i)->get_face() == next) {
        in_use = true;
        break;
      }
    }
  } while (in_use);

  return next;
}

bool
GameInitBox::handle_player_click(unsigned int player_index, int cx, int cy) {
  if (game_type != GameCustom) {
    return true;
  }

  if (cx < 8 || cx > 8 + 64 || cy < 8 || cy > 76) {
    return false;
  }

  if ((cx < 8+32) && (cy < 72)) {
    if (player_index >= mission->get_player_count()) {
      return true;
    }
    PPlayerInfo player = mission->get_player(player_index);
    player->set_character(get_next_character(player_index));
  } else if ((cx > 16 + 32) && (cy < 24)) {
    if (player_index >= mission->get_player_count()) {
	  Log::Debug["mission"] << " inside GameInitBox::handle_player_click, calling mission->add_player(PLAYERINFO_SPECIFICS - hardcoded 0/{0/0/0}/20/20/20 - incomplete template??";
      mission->add_player(0, {0, 0, 0}, 20, 20, 20);
      player_index = static_cast<unsigned int>(mission->get_player_count() - 1);
      PPlayerInfo player = mission->get_player(player_index);
      player->set_character(get_next_character(player_index));
	  // hack to work around missing color for players 2+
	  Player::Color def_color[] = {
	  {0x00, 0xe3, 0xe3},
	  {0xcf, 0x63, 0x63},
	  {0xdf, 0x7f, 0xef},
	  {0xef, 0xef, 0x8f},
	  {0x00, 0x00, 0x00}
		  };
	  player->set_color(def_color[player_index]);
    } else {
      if (player_index > 0) {
        mission->remove_player(player_index);
      }
    }
  } else {
    if (player_index >= mission->get_player_count()) {
      return true;
    }
    PPlayerInfo player = mission->get_player(player_index);
    cx -= 8 + 32 + 8 + 3;
    if (cx < 0) {
      return false;
    }
    if (cy >= 27 && cy < 69) {
      unsigned int value = clamp(0, 68 - cy, 40);
      if (cx > 0 && cx < 6) {
        /* Supplies */
        player->set_supplies(value);
      } else if (cx > 6 && cx < 12) {
        /* Intelligence */
        player->set_intelligence(value);
      } else if (cx > 12 && cx < 18) {
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
  if (game_type == GameMission) {
    ClassicMissionMapGenerator generator(*map, mission->get_random_base());
    generator.init();
    generator.generate();
    map->init_tiles(generator);
  } else {
    ClassicMapGenerator generator(*map, mission->get_random_base());
    generator.init(MapGenerator::HeightGeneratorMidpoints, true);
    generator.generate();
    map->init_tiles(generator);
  }

  minimap->set_map(map);

  set_redraw();
}
